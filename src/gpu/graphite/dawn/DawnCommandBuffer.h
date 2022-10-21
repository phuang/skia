/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnCommandBuffer_DEFINED
#define skgpu_graphite_DawnCommandBuffer_DEFINED

#include "src/gpu/graphite/CommandBuffer.h"
#include "src/gpu/graphite/DrawPass.h"
#include "src/gpu/graphite/GpuWorkSubmission.h"
#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/dawn/DawnStagingBufferPool.h"


#include "webgpu/webgpu_cpp.h"

namespace skgpu::graphite {
class DawnGraphicsPipeline;
class DawnQueueManager;
class DawnResourceProvider;
class DawnSharedContext;

class DawnCommandBuffer final : public CommandBuffer {
public:
    static std::unique_ptr<DawnCommandBuffer> Make(const DawnSharedContext*,
                                                   DawnResourceProvider*);
    ~DawnCommandBuffer() override;

    bool isFinished() {
        // TODO
        SkASSERT(false);
        return false;
    }

    void waitUntilFinished(const SharedContext*) {
        // TODO
        SkASSERT(false);
    }

    wgpu::CommandBuffer finishEncoding();

private:
    DawnCommandBuffer(const DawnSharedContext* sharedContext,
                      DawnResourceProvider* resourceProvider);

    void onResetCommandBuffer() override;
    bool setNewCommandBufferResources() override;

    bool onAddRenderPass(const RenderPassDesc&,
                         const Texture* colorTexture,
                         const Texture* resolveTexture,
                         const Texture* depthStencilTexture,
                         const std::vector<std::unique_ptr<DrawPass>>& drawPasses) override;
    bool onAddComputePass(const ComputePassDesc&,
                          const ComputePipeline*,
                          const std::vector<ResourceBinding>& bindings) override;

    // Methods for populating a DawnRenderCommandEncoder:
    bool beginRenderPass(const RenderPassDesc&,
                         const Texture* colorTexture,
                         const Texture* resolveTexture,
                         const Texture* depthStencilTexture);
    void endRenderPass();

    void addDrawPass(const DrawPass*);

    void bindGraphicsPipeline(const GraphicsPipeline*);
    void setBlendConstants(float* blendConstants);

    void bindUniformBuffer(const BindBufferInfo& info, UniformSlot);
    void bindDrawBuffers(const BindBufferInfo& vertices,
                         const BindBufferInfo& instances,
                         const BindBufferInfo& indices);

    void bindTextureAndSamplers(const DrawPass& drawPass,
                                const DrawPassCommands::BindTexturesAndSamplers& command);

    void setScissor(unsigned int left, unsigned int top,
                    unsigned int width, unsigned int height);
    void preprocessViewport(const DrawPassCommands::SetViewport& viewportCommand);
    void setViewport(float x, float y, float width, float height,
                     float minDepth, float maxDepth);

    void draw(PrimitiveType type, unsigned int baseVertex, unsigned int vertexCount);
    void drawIndexed(PrimitiveType type, unsigned int baseIndex, unsigned int indexCount,
                     unsigned int baseVertex);
    void drawInstanced(PrimitiveType type,
                       unsigned int baseVertex, unsigned int vertexCount,
                       unsigned int baseInstance, unsigned int instanceCount);
    void drawIndexedInstanced(PrimitiveType type, unsigned int baseIndex,
                              unsigned int indexCount, unsigned int baseVertex,
                              unsigned int baseInstance, unsigned int instanceCount);

    // Methods for populating a DawnComputeCommandEncoder:
    void beginComputePass();
    void bindComputePipeline(const ComputePipeline*);
    void bindBuffer(const Buffer* buffer, unsigned int offset, unsigned int index);
    void dispatchThreadgroups(const WorkgroupSize& globalSize, const WorkgroupSize& localSize);
    void endComputePass();

    // Methods for populating a DawnBlitCommandEncoder:
    bool onCopyTextureToBuffer(const Texture*,
                               SkIRect srcRect,
                               const Buffer*,
                               size_t bufferOffset,
                               size_t bufferRowBytes) override;
    bool onCopyBufferToTexture(const Buffer*,
                               const Texture*,
                               const BufferTextureCopyData* copyData,
                               int count) override;
    bool onCopyTextureToTexture(const Texture* src,
                                SkIRect srcRect,
                                const Texture* dst,
                                SkIPoint dstPoint) override;
    bool onSynchronizeBufferToCpu(const Buffer*, bool* outDidResultInWork) override;

    void syncUniformBuffers();

    bool fBoundUniformBuffersDirty = false;

    // TODO: use proper max number of binding slots.
    std::array<wgpu::Buffer, 16> fBoundUniformBuffers;
    std::array<size_t, 16> fBoundUniformBufferOffsets;
    std::array<size_t, 16> fBoundUniformBufferRanges;

    wgpu::CommandEncoder fCommandEncoder;
    wgpu::RenderPassEncoder fActiveRenderPassEncoder;
    wgpu::ComputePassEncoder fActiveComputePassEncoder;
    wgpu::Buffer fConstantBuffer;
    DawnStagingBufferPool fConstantStagingBufferPool;
    const DawnGraphicsPipeline* fActiveGraphicsPipeline = nullptr;
    [[maybe_unused]] const DawnSharedContext* fSharedContext;
    [[maybe_unused]] DawnResourceProvider* fResourceProvider;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnCommandBuffer_DEFINED
