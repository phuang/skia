/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnCommandBuffer.h"

#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/TextureProxy.h"
#include "src/gpu/graphite/dawn/DawnCaps.h"
#include "src/gpu/graphite/dawn/DawnResourceProvider.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"

namespace skgpu::graphite {

std::unique_ptr<DawnCommandBuffer> DawnCommandBuffer::Make(wgpu::CommandBuffer cmdBuffer,
                                                           const DawnSharedContext* sharedContext,
                                                           DawnResourceProvider* resourceProvider) {
    SkASSERT(false);
    return nullptr;
}

DawnCommandBuffer::DawnCommandBuffer(wgpu::CommandBuffer cmdBuffer,
                                     const DawnSharedContext* sharedContext,
                                     DawnResourceProvider* resourceProvider)
        : fCommandBuffer(std::move(cmdBuffer))
        , fSharedContext(sharedContext)
        , fResourceProvider(resourceProvider) {

}

DawnCommandBuffer::~DawnCommandBuffer() {}

bool DawnCommandBuffer::commit() {
    // TODO
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::onAddRenderPass(const RenderPassDesc& renderPassDesc,
                                        const Texture* colorTexture,
                                        const Texture* resolveTexture,
                                        const Texture* depthStencilTexture,
                                        const std::vector<std::unique_ptr<DrawPass>>& drawPasses) {
    // TODO
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::onAddComputePass(const ComputePassDesc& computePassDesc,
                                         const ComputePipeline* pipeline,
                                         const std::vector<ResourceBinding>& bindings) {
    // TODO
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::beginRenderPass(const RenderPassDesc& renderPassDesc,
                                       const Texture* colorTexture,
                                       const Texture* resolveTexture,
                                       const Texture* depthStencilTexture) {
    // TODO
    SkASSERT(false);
    return false;
}

void DawnCommandBuffer::endRenderPass() {
    SkASSERT(false);
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
                for (int j = 0; j < bts->fNumTexSamplers; ++j) {
                    this->bindTextureAndSampler(drawPass->getTexture(bts->fTextureIndices[j]),
                                                drawPass->getSampler(bts->fSamplerIndices[j]),
                                                j);
                }
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
}

void DawnCommandBuffer::bindUniformBuffer(const BindBufferInfo& info, UniformSlot slot) {
    SkASSERT(false);
}

void DawnCommandBuffer::bindDrawBuffers(const BindBufferInfo& vertices,
                                       const BindBufferInfo& instances,
                                       const BindBufferInfo& indices) {
    SkASSERT(false);
}

void DawnCommandBuffer::bindVertexBuffers(const Buffer* vertexBuffer,
                                         size_t vertexOffset,
                                         const Buffer* instanceBuffer,
                                         size_t instanceOffset) {
    SkASSERT(false);
}

void DawnCommandBuffer::bindIndexBuffer(const Buffer* indexBuffer, size_t offset) {
    SkASSERT(false);
}

void DawnCommandBuffer::bindTextureAndSampler(const Texture* texture,
                                             const Sampler* sampler,
                                             unsigned int bindIndex) {
    SkASSERT(false);
}

void DawnCommandBuffer::setScissor(unsigned int left, unsigned int top,
                                   unsigned int width, unsigned int height) {
    SkASSERT(false);
}

void DawnCommandBuffer::setViewport(float x, float y, float width, float height,
                                    float minDepth, float maxDepth) {
    SkASSERT(false);
}

void DawnCommandBuffer::setBlendConstants(float* blendConstants) {
    SkASSERT(false);
}

void DawnCommandBuffer::draw(PrimitiveType type,
                             unsigned int baseVertex,
                             unsigned int vertexCount) {
    SkASSERT(false);
}

void DawnCommandBuffer::drawIndexed(PrimitiveType type, unsigned int baseIndex,
                                    unsigned int indexCount, unsigned int baseVertex) {
    SkASSERT(false);
}

void DawnCommandBuffer::drawInstanced(PrimitiveType type, unsigned int baseVertex,
                                      unsigned int vertexCount, unsigned int baseInstance,
                                      unsigned int instanceCount) {
    SkASSERT(false);
}

void DawnCommandBuffer::drawIndexedInstanced(PrimitiveType type,
                                             unsigned int baseIndex,
                                             unsigned int indexCount,
                                             unsigned int baseVertex,
                                             unsigned int baseInstance,
                                             unsigned int instanceCount) {
    SkASSERT(false);
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
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::onCopyBufferToTexture(const Buffer* buffer,
                                              const Texture* texture,
                                              const BufferTextureCopyData* copyData,
                                              int count) {
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::onCopyTextureToTexture(const Texture* src,
                                               SkIRect srcRect,
                                               const Texture* dst,
                                               SkIPoint dstPoint) {
    SkASSERT(false);
    return false;
}

bool DawnCommandBuffer::onSynchronizeBufferToCpu(const Buffer* buffer,
                                                 bool* outDidResultInWork) {
    SkASSERT(false);
    return false;
}

void DawnCommandBuffer::onResetCommandBuffer() {
    SkASSERT(false);
}

bool DawnCommandBuffer::setNewCommandBufferResources() {
    return false;
}

} // namespace skgpu::graphite
