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

#include <optional>

#include "webgpu/webgpu_cpp.h"

class SkUniform;

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

    static sk_sp<DawnGraphicsPipeline> Make(const DawnSharedContext*,
                                           std::string label,
                                           wgpu::ShaderModule vsModule,
                                           SkSpan<const SkUniform> uniforms,
                                           size_t numTextures,
                                           SkSpan<const Attribute> vertexAttrs,
                                           SkSpan<const Attribute> instanceAttrs,
                                           PrimitiveType primitiveType,
                                           wgpu::ShaderModule fsModule,
                                           const DepthStencilSettings& depthStencilSettings,
                                           const BlendInfo& blendInfo,
                                           const RenderPassDesc&);

    ~DawnGraphicsPipeline() override {}

    uint32_t stencilReferenceValue() const { return fStencilReferenceValue; }
    PrimitiveType primitiveType() const { return fPrimitiveType; }
    bool hasStepUniforms() const { return fHasStepUniforms; }
    bool hasFragment() const { return fHasFragment; }
    const wgpu::RenderPipeline& dawnRenderPipeline() const;

private:
    DawnGraphicsPipeline(const skgpu::graphite::SharedContext* sharedContext,
                         PrimitiveType primitiveType,
                         uint32_t refValue,
                         bool hasStepUniforms,
                         bool hasFragment)
        : GraphicsPipeline(sharedContext)
        , fPrimitiveType(primitiveType)
        , fStencilReferenceValue(refValue)
        , fHasStepUniforms(hasStepUniforms)
        , fHasFragment(hasFragment) {}

    void freeGpuData() override;

    std::optional<wgpu::RenderPipeline> fRenderPipeline;
    const PrimitiveType fPrimitiveType;
    const uint32_t fStencilReferenceValue;
    const bool fHasStepUniforms;
    const bool fHasFragment;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnGraphicsPipeline_DEFINED
