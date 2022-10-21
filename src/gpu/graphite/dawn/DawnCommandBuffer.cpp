/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnCommandBuffer.h"

#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/TextureProxy.h"
#include "src/gpu/graphite/dawn/DawnBuffer.h"
#include "src/gpu/graphite/dawn/DawnCaps.h"
#include "src/gpu/graphite/dawn/DawnGraphicsPipeline.h"
#include "src/gpu/graphite/dawn/DawnQueueManager.h"
#include "src/gpu/graphite/dawn/DawnSampler.h"
#include "src/gpu/graphite/dawn/DawnTexture.h"
#include "src/gpu/graphite/dawn/DawnResourceProvider.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"
#include "src/gpu/graphite/dawn/DawnUtils.h"

namespace skgpu::graphite {

namespace {
using IntrinsicConstant = float[4];
}

std::unique_ptr<DawnCommandBuffer> DawnCommandBuffer::Make(const DawnSharedContext* sharedContext,
                                                           DawnResourceProvider* resourceProvider) {
    return std::unique_ptr<DawnCommandBuffer>(
            new DawnCommandBuffer(sharedContext, resourceProvider));
}

DawnCommandBuffer::DawnCommandBuffer(const DawnSharedContext* sharedContext,
                                     DawnResourceProvider* resourceProvider)
        : fConstantStagingBufferPool(sizeof(IntrinsicConstant))
        , fSharedContext(sharedContext)
        , fResourceProvider(resourceProvider) {}

DawnCommandBuffer::~DawnCommandBuffer() {}

wgpu::CommandBuffer DawnCommandBuffer::finishEncoding() {
    // TODO
    SkASSERT(fCommandEncoder);
    wgpu::CommandBuffer cmdBuffer = fCommandEncoder.Finish();

    fCommandEncoder = nullptr;

    return cmdBuffer;
}

void DawnCommandBuffer::onResetCommandBuffer() {
    fActiveGraphicsPipeline = nullptr;
    fActiveRenderPassEncoder = nullptr;
    fActiveComputePassEncoder = nullptr;
    fCommandEncoder = nullptr;

    for (auto& bufferSlot : fBoundUniformBuffers) {
        bufferSlot = nullptr;
    }
    fBoundUniformBuffersDirty = true;
}

bool DawnCommandBuffer::setNewCommandBufferResources() {
    if (!fConstantBuffer) {
        wgpu::BufferDescriptor desc;
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        desc.size = sizeof(IntrinsicConstant);
        desc.mappedAtCreation = true;
        fConstantBuffer = fSharedContext->device().CreateBuffer(&desc);
        if (!fConstantBuffer) {
            SkASSERT(false);
        }
    }
    fCommandEncoder = fSharedContext->device().CreateCommandEncoder();
    return true;
}

bool DawnCommandBuffer::onAddRenderPass(const RenderPassDesc& renderPassDesc,
                                        const Texture* colorTexture,
                                        const Texture* resolveTexture,
                                        const Texture* depthStencilTexture,
                                        const std::vector<std::unique_ptr<DrawPass>>& drawPasses) {
    // Update viewport's constant buffer before starting a render pass.
    int numViewports = 0;
    for (auto& drawPass : drawPasses) {
        for (auto [type, cmdPtr] : drawPass->commands()) {
            switch (type) {
                case DrawPassCommands::Type::kSetViewport: {
                    numViewports++;
                    auto sv = static_cast<DrawPassCommands::SetViewport*>(cmdPtr);
                    preprocessViewport(*sv);
                } break;
            }
        }
    }

    // TODO: only support single viewport change per render pass for now.
    SkASSERT(numViewports <= 1);

    if (!this->beginRenderPass(renderPassDesc, colorTexture, resolveTexture, depthStencilTexture)) {
        return false;
    }

    for (size_t i = 0; i < drawPasses.size(); ++i) {
        this->addDrawPass(drawPasses[i].get());
    }

    this->endRenderPass();
    return true;
}

bool DawnCommandBuffer::onAddComputePass(const ComputePassDesc& computePassDesc,
                                         const ComputePipeline* pipeline,
                                         const std::vector<ResourceBinding>& bindings) {
    this->beginComputePass();
    this->bindComputePipeline(pipeline);
    for (const ResourceBinding& binding : bindings) {
        this->bindBuffer(
                binding.fResource.fBuffer.get(), binding.fResource.fOffset, binding.fIndex);
    }
    this->dispatchThreadgroups(computePassDesc.fGlobalDispatchSize,
                               computePassDesc.fLocalDispatchSize);
    this->endComputePass();
    return true;
}

bool DawnCommandBuffer::beginRenderPass(const RenderPassDesc& renderPassDesc,
                                       const Texture* colorTexture,
                                       const Texture* resolveTexture,
                                       const Texture* depthStencilTexture) {
    SkASSERT(!fActiveRenderPassEncoder);
    SkASSERT(!fActiveComputePassEncoder);

    constexpr static wgpu::LoadOp wgpuLoadActionMap[]{
            wgpu::LoadOp::Load,
            wgpu::LoadOp::Clear,
            wgpu::LoadOp::Clear  // Don't care
    };
    static_assert((int)LoadOp::kLoad == 0);
    static_assert((int)LoadOp::kClear == 1);
    static_assert((int)LoadOp::kDiscard == 2);
    static_assert(std::size(wgpuLoadActionMap) == kLoadOpCount);

    constexpr static wgpu::StoreOp wgpuStoreActionMap[]{wgpu::StoreOp::Store,
                                                        wgpu::StoreOp::Discard};
    static_assert((int)StoreOp::kStore == 0);
    static_assert((int)StoreOp::kDiscard == 1);
    static_assert(std::size(wgpuStoreActionMap) == kStoreOpCount);

    wgpu::RenderPassDescriptor wgpuRenderpass = {};
    wgpu::RenderPassColorAttachment colorAttachment;
    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;

    // Set up color attachment.
    auto& colorInfo = renderPassDesc.fColorAttachment;
    bool loadMSAAFromResolve = false;
    if (colorTexture) {
        wgpuRenderpass.colorAttachments = &colorAttachment;
        wgpuRenderpass.colorAttachmentCount = 1;

        // TODO: check Texture matches RenderPassDesc
        colorAttachment.view = ((DawnTexture*)colorTexture)->dawnTextureView();
        const std::array<float, 4>& clearColor = renderPassDesc.fClearColor;
        colorAttachment.clearValue = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
        colorAttachment.loadOp = wgpuLoadActionMap[static_cast<int>(colorInfo.fLoadOp)];
        colorAttachment.storeOp = wgpuStoreActionMap[static_cast<int>(colorInfo.fStoreOp)];
        // Set up resolve attachment
        if (resolveTexture) {
            SkASSERT(renderPassDesc.fColorResolveAttachment.fStoreOp == StoreOp::kStore);
            // TODO: check Texture matches RenderPassDesc
            colorAttachment.resolveTarget = ((DawnTexture*)resolveTexture)->dawnTextureView();
            // Inclusion of a resolve texture implies the client wants to finish the
            // renderpass with a resolve.
            SkASSERT(colorAttachment.storeOp == wgpu::StoreOp::Discard);

            // But it also means we have to load the resolve texture into the MSAA color attachment
            loadMSAAFromResolve = renderPassDesc.fColorResolveAttachment.fLoadOp == LoadOp::kLoad;
            // TODO: If the color resolve texture is read-only we can use a private (vs. memoryless)
            // msaa attachment that's coupled to the framebuffer and the StoreAndMultisampleResolve
            // action instead of loading as a draw.
        }
    }

    // Set up stencil/depth attachment
    auto& depthStencilInfo = renderPassDesc.fDepthStencilAttachment;
    if (depthStencilTexture) {
        wgpuRenderpass.depthStencilAttachment = &depthStencilAttachment;
        // TODO: check Texture matches RenderPassDesc
        auto dawnTextureView = ((DawnTexture*)depthStencilTexture)->dawnTextureView();
        depthStencilAttachment.view = dawnTextureView;

        depthStencilAttachment.depthClearValue = renderPassDesc.fClearDepth;
        depthStencilAttachment.depthLoadOp =
                wgpuLoadActionMap[static_cast<int>(depthStencilInfo.fLoadOp)];
        depthStencilAttachment.depthStoreOp =
                wgpuStoreActionMap[static_cast<int>(depthStencilInfo.fStoreOp)];

        depthStencilAttachment.stencilClearValue = renderPassDesc.fClearStencil;
        depthStencilAttachment.stencilLoadOp =
                wgpuLoadActionMap[static_cast<int>(depthStencilInfo.fLoadOp)];
        depthStencilAttachment.stencilStoreOp =
                wgpuStoreActionMap[static_cast<int>(depthStencilInfo.fStoreOp)];

    } else {
        SkASSERT(!depthStencilInfo.fTextureInfo.isValid());
    }

    fActiveRenderPassEncoder = fCommandEncoder.BeginRenderPass(&wgpuRenderpass);

    if (loadMSAAFromResolve) {
        // Manually load the contents of the resolve texture into the MSAA attachment as a draw,
        // so the actual load op for the MSAA attachment had better have been discard.
        SkASSERT(colorInfo.fLoadOp == LoadOp::kDiscard);

        // TODO
        SkASSERT(false);
    }

    return true;
}

void DawnCommandBuffer::endRenderPass() {
    SkASSERT(fActiveRenderPassEncoder);
    fActiveRenderPassEncoder.End();
}

void DawnCommandBuffer::addDrawPass(const DrawPass* drawPass) {
    drawPass->addResourceRefs(this);
    for (auto[type, cmdPtr] : drawPass->commands()) {
        switch (type) {
            case DrawPassCommands::Type::kBindGraphicsPipeline: {
                auto bgp = static_cast<DrawPassCommands::BindGraphicsPipeline*>(cmdPtr);
                this->bindGraphicsPipeline(drawPass->getPipeline(bgp->fPipelineIndex));
                break;
            }
            case DrawPassCommands::Type::kSetBlendConstants: {
                auto sbc = static_cast<DrawPassCommands::SetBlendConstants*>(cmdPtr);
                this->setBlendConstants(sbc->fBlendConstants);
                break;
            }
            case DrawPassCommands::Type::kBindUniformBuffer: {
                auto bub = static_cast<DrawPassCommands::BindUniformBuffer*>(cmdPtr);
                this->bindUniformBuffer(bub->fInfo, bub->fSlot);
                break;
            }
            case DrawPassCommands::Type::kBindDrawBuffers: {
                auto bdb = static_cast<DrawPassCommands::BindDrawBuffers*>(cmdPtr);
                this->bindDrawBuffers(bdb->fVertices, bdb->fInstances, bdb->fIndices);
                break;
            }
            case DrawPassCommands::Type::kBindTexturesAndSamplers: {
                auto bts = static_cast<DrawPassCommands::BindTexturesAndSamplers*>(cmdPtr);
                bindTextureAndSamplers(*drawPass, *bts);
                break;
            }
            case DrawPassCommands::Type::kSetViewport: {
                auto sv = static_cast<DrawPassCommands::SetViewport*>(cmdPtr);
                this->setViewport(sv->fViewport.fLeft,
                                  sv->fViewport.fTop,
                                  sv->fViewport.width(),
                                  sv->fViewport.height(),
                                  sv->fMinDepth,
                                  sv->fMaxDepth);
                break;
            }
            case DrawPassCommands::Type::kSetScissor: {
                auto ss = static_cast<DrawPassCommands::SetScissor*>(cmdPtr);
                const SkIRect& rect = ss->fScissor;
                this->setScissor(rect.fLeft, rect.fTop, rect.width(), rect.height());
                break;
            }
            case DrawPassCommands::Type::kDraw: {
                auto draw = static_cast<DrawPassCommands::Draw*>(cmdPtr);
                this->draw(draw->fType, draw->fBaseVertex, draw->fVertexCount);
                break;
            }
            case DrawPassCommands::Type::kDrawIndexed: {
                auto draw = static_cast<DrawPassCommands::DrawIndexed*>(cmdPtr);
                this->drawIndexed(draw->fType,
                                  draw->fBaseIndex,
                                  draw->fIndexCount,
                                  draw->fBaseVertex);
                break;
            }
            case DrawPassCommands::Type::kDrawInstanced: {
                auto draw = static_cast<DrawPassCommands::DrawInstanced*>(cmdPtr);
                this->drawInstanced(draw->fType,
                                    draw->fBaseVertex,
                                    draw->fVertexCount,
                                    draw->fBaseInstance,
                                    draw->fInstanceCount);
                break;
            }
            case DrawPassCommands::Type::kDrawIndexedInstanced: {
                auto draw = static_cast<DrawPassCommands::DrawIndexedInstanced*>(cmdPtr);
                this->drawIndexedInstanced(draw->fType,
                                           draw->fBaseIndex,
                                           draw->fIndexCount,
                                           draw->fBaseVertex,
                                           draw->fBaseInstance,
                                           draw->fInstanceCount);
                break;
            }
        }
    }
}

void DawnCommandBuffer::bindGraphicsPipeline(const GraphicsPipeline* graphicsPipeline) {
    SkASSERT(false);
    fActiveGraphicsPipeline = static_cast<const DawnGraphicsPipeline*>(graphicsPipeline);

    fBoundUniformBuffersDirty = true;
}

void DawnCommandBuffer::bindUniformBuffer(const BindBufferInfo& info, UniformSlot slot) {
    SkASSERT(fActiveRenderPassEncoder);

    auto dawnBuffer = static_cast<const DawnBuffer*>(info.fBuffer);

    unsigned int bufferIndex = 0;
    switch(slot) {
        case UniformSlot::kRenderStep:
            bufferIndex = DawnGraphicsPipeline::kRenderStepUniformBufferIndex;
            break;
        case UniformSlot::kPaint:
            bufferIndex = DawnGraphicsPipeline::kPaintUniformBufferIndex;
            break;
        default:
            SkASSERT(false);
    }

    fBoundUniformBuffers[bufferIndex] = dawnBuffer->dawnBuffer();
    fBoundUniformBufferOffsets[bufferIndex] = info.fOffset;
    if (dawnBuffer->size() < info.fOffset) {
        fBoundUniformBufferRanges[bufferIndex] = 0;
    } else {
        fBoundUniformBufferRanges[bufferIndex] = dawnBuffer->size() - info.fOffset;
    }

    fBoundUniformBuffersDirty = true;
}

void DawnCommandBuffer::bindDrawBuffers(const BindBufferInfo& vertices,
                                       const BindBufferInfo& instances,
                                       const BindBufferInfo& indices) {
    SkASSERT(fActiveRenderPassEncoder);

    if (vertices.fBuffer) {
        auto dawnBuffer = static_cast<const DawnBuffer*>(vertices.fBuffer)->dawnBuffer();
        fActiveRenderPassEncoder.SetVertexBuffer(
                DawnGraphicsPipeline::kVertexBufferIndex, dawnBuffer, vertices.fOffset);
    }
    if (instances.fBuffer) {
        auto dawnBuffer = static_cast<const DawnBuffer*>(instances.fBuffer)->dawnBuffer();
        fActiveRenderPassEncoder.SetVertexBuffer(
                DawnGraphicsPipeline::kInstanceBufferIndex, dawnBuffer, instances.fOffset);
    }
    if (indices.fBuffer) {
        auto dawnBuffer = static_cast<const DawnBuffer*>(indices.fBuffer)->dawnBuffer();
        fActiveRenderPassEncoder.SetIndexBuffer(
                dawnBuffer, wgpu::IndexFormat::Uint16, indices.fOffset);
    }
}

void DawnCommandBuffer::bindTextureAndSamplers(const DrawPass& drawPass,
                                const DrawPassCommands::BindTexturesAndSamplers& command) {
    SkASSERT(fActiveRenderPassEncoder);
    SkASSERT(fActiveGraphicsPipeline);

    // TODO: optimize for single texture.
    std::vector<wgpu::BindGroupEntry> entries(2 * command.fNumTexSamplers);

    for (int j = 0; j < command.fNumTexSamplers; ++j) {
        auto texture = drawPass.getTexture(command.fTextureIndices[j]);
        auto sampler = drawPass.getSampler(command.fSamplerIndices[j]);
        auto& wgpuTexture = static_cast<const DawnTexture*>(texture)->dawnTextureView();
        auto& wgpuSampler = static_cast<const DawnSampler*>(sampler)->dawnSampler();

        // since shader generator assigns binding slot to sampler then texture,
        // then the next sampler and texture, and so on, we need to use
        // 2 * j as base binding index of the sampler and texture.
        entries[2 * j].binding = 2 * j;
        entries[2 * j].sampler = wgpuSampler;

        entries[2 * j + 1].binding = 2 * j + 1;
        entries[2 * j + 1].textureView = wgpuTexture;
    }

    wgpu::BindGroupDescriptor desc;
    desc.layout = fActiveGraphicsPipeline->dawnRenderPipeline().GetBindGroupLayout(
            DawnGraphicsPipeline::kTextureBindGroupIndex);
    desc.entries = entries.data();
    desc.entryCount = entries.size();

    auto bindGroup = fSharedContext->device().CreateBindGroup(&desc);

    fActiveRenderPassEncoder.SetBindGroup(DawnGraphicsPipeline::kTextureBindGroupIndex, bindGroup);
}

void DawnCommandBuffer::syncUniformBuffers() {
    if (fBoundUniformBuffersDirty) {
        fBoundUniformBuffersDirty = false;

        constexpr uint32_t bindingIndices[] = {
                DawnGraphicsPipeline::kIntrinsicUniformBufferIndex,
                DawnGraphicsPipeline::kRenderStepUniformBufferIndex,
                DawnGraphicsPipeline::kPaintUniformBufferIndex,
        };
        std::array<wgpu::BindGroupEntry, std::size(bindingIndices)> entries;

        for (size_t i = 0; i < entries.size(); ++i) {
            auto bindingIndex = bindingIndices[i];
            entries[i].binding = bindingIndex;
            entries[i].buffer = fBoundUniformBuffers[bindingIndex];
            entries[i].offset = fBoundUniformBufferOffsets[bindingIndex];
            entries[i].size = fBoundUniformBufferRanges[bindingIndex];
        }

        wgpu::BindGroupDescriptor desc;
        desc.layout = fActiveGraphicsPipeline->dawnRenderPipeline().GetBindGroupLayout(
                DawnGraphicsPipeline::kUniformBufferBindGroupIndex);
        desc.entries = entries.data();
        desc.entryCount = entries.size();

        auto bindGroup = fSharedContext->device().CreateBindGroup(&desc);

        fActiveRenderPassEncoder.SetBindGroup(DawnGraphicsPipeline::kUniformBufferBindGroupIndex,
                                              bindGroup);
    }
}

void DawnCommandBuffer::setScissor(unsigned int left, unsigned int top,
                                   unsigned int width, unsigned int height) {
    SkASSERT(fActiveRenderPassEncoder);
    fActiveRenderPassEncoder.SetScissorRect(left, top, width, height);
}

void DawnCommandBuffer::preprocessViewport(const DrawPassCommands::SetViewport& viewportCommand) {
    SkASSERT(!fActiveRenderPassEncoder);
    SkASSERT(!fActiveComputePassEncoder);

    // Dawn's framebuffer space has (0, 0) at the top left. This agrees with Skia's device coords.
    // However, in NDC (-1, -1) is the bottom left. So we flip the origin here (assuming all
    // surfaces we have are TopLeft origin).
    const float x = viewportCommand.fViewport.fLeft;
    const float y = viewportCommand.fViewport.fTop;
    const float invTwoW = 2.f / viewportCommand.fViewport.width();
    const float invTwoH = 2.f / viewportCommand.fViewport.height();
    const IntrinsicConstant rtAdjust = {invTwoW, -invTwoH, -1.f - x * invTwoW, 1.f + y * invTwoH};

    fConstantStagingBufferPool.writeBuffer(fSharedContext->device(), fCommandEncoder, fConstantBuffer, &rtAdjust);
}

void DawnCommandBuffer::setViewport(float x, float y, float width, float height,
                                    float minDepth, float maxDepth) {
    SkASSERT(fActiveRenderPassEncoder);
    fActiveRenderPassEncoder.SetViewport(x, y, width, height, minDepth, maxDepth);
}

void DawnCommandBuffer::setBlendConstants(float* blendConstants) {
    SkASSERT(fActiveRenderPassEncoder);
    wgpu::Color blendConst = {
            blendConstants[0], blendConstants[1], blendConstants[2], blendConstants[3]};
    fActiveRenderPassEncoder.SetBlendConstant(&blendConst);
}

void DawnCommandBuffer::draw(PrimitiveType type,
                             unsigned int baseVertex,
                             unsigned int vertexCount) {
    SkASSERT(fActiveRenderPassEncoder);
    SkASSERT(fActiveGraphicsPipeline->primitiveType() == type);

    syncUniformBuffers();

    fActiveRenderPassEncoder.Draw(vertexCount, /*instanceCount=*/1, baseVertex);
}

void DawnCommandBuffer::drawIndexed(PrimitiveType type, unsigned int baseIndex,
                                    unsigned int indexCount, unsigned int baseVertex) {
    SkASSERT(fActiveRenderPassEncoder);
    SkASSERT(fActiveGraphicsPipeline->primitiveType() == type);

    syncUniformBuffers();

    fActiveRenderPassEncoder.DrawIndexed(indexCount, /*instanceCount=*/1, baseIndex, baseVertex);
}

void DawnCommandBuffer::drawInstanced(PrimitiveType type, unsigned int baseVertex,
                                      unsigned int vertexCount, unsigned int baseInstance,
                                      unsigned int instanceCount) {
    SkASSERT(fActiveRenderPassEncoder);
    SkASSERT(fActiveGraphicsPipeline->primitiveType() == type);

    syncUniformBuffers();

    fActiveRenderPassEncoder.Draw(vertexCount, instanceCount, baseVertex, baseInstance);
}

void DawnCommandBuffer::drawIndexedInstanced(PrimitiveType type,
                                             unsigned int baseIndex,
                                             unsigned int indexCount,
                                             unsigned int baseVertex,
                                             unsigned int baseInstance,
                                             unsigned int instanceCount) {
    SkASSERT(fActiveRenderPassEncoder);
    SkASSERT(fActiveGraphicsPipeline->primitiveType() == type);

    syncUniformBuffers();

    fActiveRenderPassEncoder.DrawIndexed(
            indexCount, instanceCount, baseIndex, baseVertex, baseInstance);
}

void DawnCommandBuffer::beginComputePass() {
    SkASSERT(false);
}

void DawnCommandBuffer::bindComputePipeline(const ComputePipeline* computePipeline) {
    SkASSERT(false);
}

void DawnCommandBuffer::bindBuffer(const Buffer* buffer, unsigned int offset, unsigned int index) {
    SkASSERT(false);
}

void DawnCommandBuffer::dispatchThreadgroups(const WorkgroupSize& globalSize,
                                             const WorkgroupSize& localSize) {
    SkASSERT(false);
}

void DawnCommandBuffer::endComputePass() {
    SkASSERT(false);
}

bool DawnCommandBuffer::onCopyTextureToBuffer(const Texture* texture,
                                              SkIRect srcRect,
                                              const Buffer* buffer,
                                              size_t bufferOffset,
                                              size_t bufferRowBytes) {
    SkASSERT(!fActiveRenderPassEncoder);
    SkASSERT(!fActiveComputePassEncoder);

    auto& wgpuTexture = static_cast<const DawnTexture*>(texture)->dawnTexture();
    auto& wgpuBuffer = static_cast<const DawnBuffer*>(buffer)->dawnBuffer();

    wgpu::ImageCopyTexture src;
    src.texture = wgpuTexture;
    src.origin.x = srcRect.fLeft;
    src.origin.y = srcRect.fTop;

    wgpu::ImageCopyBuffer dst;
    dst.buffer = wgpuBuffer;
    dst.layout.bytesPerRow = bufferRowBytes;
    dst.layout.offset = bufferOffset;

    wgpu::Extent3D copySize = {
            static_cast<uint32_t>(srcRect.width()), static_cast<uint32_t>(srcRect.height()), 1};

    fCommandEncoder.CopyTextureToBuffer(&src, &dst, &copySize);

    return true;
}

bool DawnCommandBuffer::onCopyBufferToTexture(const Buffer* buffer,
                                              const Texture* texture,
                                              const BufferTextureCopyData* copyData,
                                              int count) {
    SkASSERT(!fActiveRenderPassEncoder);
    SkASSERT(!fActiveComputePassEncoder);

    auto& wgpuTexture = static_cast<const DawnTexture*>(texture)->dawnTexture();
    auto& wgpuBuffer = static_cast<const DawnBuffer*>(buffer)->dawnBuffer();

    for (int i = 0; i < count; ++i) {
        wgpu::ImageCopyBuffer src;
        src.buffer = wgpuBuffer;
        src.layout.bytesPerRow = copyData[i].fBufferRowBytes;
        src.layout.offset = copyData[i].fBufferOffset;

        wgpu::ImageCopyTexture dst;
        dst.texture = wgpuTexture;
        dst.origin.x = copyData[i].fRect.fLeft;
        dst.origin.y = copyData[i].fRect.fTop;

        wgpu::Extent3D copySize = {static_cast<uint32_t>(copyData[i].fRect.width()),
                                   static_cast<uint32_t>(copyData[i].fRect.height()),
                                   1};
        fCommandEncoder.CopyBufferToTexture(&src, &dst, &copySize);
    }

    return true;
}

bool DawnCommandBuffer::onCopyTextureToTexture(const Texture* src,
                                               SkIRect srcRect,
                                               const Texture* dst,
                                               SkIPoint dstPoint) {
    SkASSERT(!fActiveRenderPassEncoder);
    SkASSERT(!fActiveComputePassEncoder);

    auto& wgpuTextureSrc = static_cast<const DawnTexture*>(src)->dawnTexture();
    auto& wgpuTextureDst = static_cast<const DawnTexture*>(dst)->dawnTexture();

    wgpu::ImageCopyTexture srcArgs;
    srcArgs.texture = wgpuTextureSrc;
    srcArgs.origin.x = srcRect.fLeft;
    srcArgs.origin.y = srcRect.fTop;

    wgpu::ImageCopyTexture dstArgs;
    dstArgs.texture = wgpuTextureDst;
    dstArgs.origin.x = dstPoint.fX;
    dstArgs.origin.y = dstPoint.fY;

    wgpu::Extent3D copySize = {
            static_cast<uint32_t>(srcRect.width()), static_cast<uint32_t>(srcRect.height()), 1};

    fCommandEncoder.CopyTextureToTexture(&srcArgs, &dstArgs, &copySize);

    return true;
}

bool DawnCommandBuffer::onSynchronizeBufferToCpu(const Buffer* buffer, bool* outDidResultInWork) {
    return true;
}

} // namespace skgpu::graphite
