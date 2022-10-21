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
#include "src/gpu/graphite/DrawTypes.h"
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
    inline static constexpr unsigned int kUniformBufferBindGroupIndex = 0;
    inline static constexpr unsigned int kTextureBindGroupIndex = 1;

    inline static constexpr unsigned int kIntrinsicUniformBufferIndex = 0;
    inline static constexpr unsigned int kRenderStepUniformBufferIndex = 1;
    inline static constexpr unsigned int kPaintUniformBufferIndex = 2;
    inline static constexpr unsigned int kNumUniformBuffers = 3;

    inline static constexpr unsigned int kVertexBufferIndex = 0;
    inline static constexpr unsigned int kInstanceBufferIndex = 1;
    inline static constexpr unsigned int kNumVertexBuffers = 2;

    using SPIRVFunction = std::pair<wgpu::ShaderModule, std::string>;
    static sk_sp<DawnGraphicsPipeline> Make(const DawnSharedContext*,
                                           std::string label,
                                           SPIRVFunction vertexMain,
                                           SkSpan<const Attribute> vertexAttrs,
                                           SkSpan<const Attribute> instanceAttrs,
                                           PrimitiveType primitiveType,
                                           SPIRVFunction fragmentMain,
                                           const DepthStencilSettings& depthStencilSettings,
                                           const BlendInfo& blendInfo,
                                           const RenderPassDesc&);

    ~DawnGraphicsPipeline() override {}

    uint32_t stencilReferenceValue() const { return fStencilReferenceValue; }
    PrimitiveType primitiveType() const { return fPrimitiveType; }
    const wgpu::RenderPipeline& dawnRenderPipeline() const { return fRenderPipeline; }
private:
    DawnGraphicsPipeline(const skgpu::graphite::SharedContext* sharedContext,
                         wgpu::RenderPipeline renderPipeline,
                         PrimitiveType primitiveType,
                         uint32_t refValue)
        : GraphicsPipeline(sharedContext)
        , fRenderPipeline(std::move(renderPipeline))
        , fStencilReferenceValue(refValue) {}

    void freeGpuData() override;

    wgpu::RenderPipeline fRenderPipeline;
    PrimitiveType fPrimitiveType;
    uint32_t fStencilReferenceValue;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnGraphicsPipeline_DEFINED
