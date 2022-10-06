/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnGraphicsPipeline_DEFINED
#define skgpu_graphite_DawnGraphicsPipeline_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"
#include "include/ports/SkCFObject.h"
#include "src/gpu/graphite/GraphicsPipeline.h"
#include <memory>

#include "webgpu/webgpu_cpp.h"

namespace skgpu {
struct BlendInfo;
}

namespace skgpu::graphite {

class Attribute;
class Context;
class GraphicsPipelineDesc;
class DawnResourceProvider;
class DawnSharedContext;
struct DepthStencilSettings;
struct RenderPassDesc;

class DawnGraphicsPipeline final : public GraphicsPipeline {
public:
    // inline static constexpr unsigned int kIntrinsicUniformBufferIndex = 0;
    // inline static constexpr unsigned int kRenderStepUniformBufferIndex = 1;
    // inline static constexpr unsigned int kPaintUniformBufferIndex = 2;
    inline static constexpr unsigned int kVertexBufferIndex = 0;
    inline static constexpr unsigned int kInstanceBufferIndex = 1;
    inline static constexpr unsigned int kNumberBuffer = 2;

    using SPIRVFunction = std::pair<wgpu::ShaderModule, std::string>;
    static sk_sp<DawnGraphicsPipeline> Make(const DawnSharedContext*,
                                           std::string label,
                                           SPIRVFunction vertexMain,
                                           SkSpan<const Attribute> vertexAttrs,
                                           SkSpan<const Attribute> instanceAttrs,
                                           SPIRVFunction fragmentMain,
                                        //    sk_cfp<id<MTLDepthStencilState>>,
                                           const DepthStencilSettings& depthStencilSettings,
                                           const BlendInfo& blendInfo,
                                           const RenderPassDesc&);

    ~DawnGraphicsPipeline() override {}

    // id<MTLRenderPipelineState> mtlPipelineState() const { return fPipelineState.get(); }
    // id<MTLDepthStencilState> mtlDepthStencilState() const { return fDepthStencilState.get(); }
    uint32_t stencilReferenceValue() const { return fStencilReferenceValue; }

private:
    DawnGraphicsPipeline(const skgpu::graphite::SharedContext* sharedContext,
                        //  sk_cfp<id<MTLRenderPipelineState>> pso,
                        //  sk_cfp<id<MTLDepthStencilState>> dss,
                         wgpu::RenderPipeline renderPipeline,
                         uint32_t refValue)
        : GraphicsPipeline(sharedContext)
        // , fPipelineState(std::move(pso))
        // , fDepthStencilState(dss)
        , fRenderPipeline(std::move(renderPipeline))
        , fStencilReferenceValue(refValue) {}

    void freeGpuData() override;

    wgpu::RenderPipeline fRenderPipeline;
    uint32_t fStencilReferenceValue;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnGraphicsPipeline_DEFINED
