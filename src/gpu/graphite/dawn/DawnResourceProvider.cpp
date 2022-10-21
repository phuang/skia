/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnResourceProvider.h"

#include "include/gpu/graphite/BackendTexture.h"
#include "src/gpu/graphite/ComputePipeline.h"
#include "src/gpu/graphite/ContextUtils.h"
#include "src/gpu/graphite/GraphicsPipeline.h"
#include "src/gpu/graphite/GraphicsPipelineDesc.h"
#include "src/gpu/graphite/Sampler.h"
#include "src/gpu/graphite/Texture.h"
#include "src/gpu/graphite/dawn/DawnBuffer.h"
#include "src/gpu/graphite/dawn/DawnGraphicsPipeline.h"
#include "src/gpu/graphite/dawn/DawnSampler.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"
#include "src/gpu/graphite/dawn/DawnTexture.h"
#include "src/gpu/graphite/dawn/DawnUtils.h"
#include "src/sksl/ir/SkSLProgram.h"


namespace skgpu::graphite {

DawnResourceProvider::DawnResourceProvider(SharedContext* sharedContext,
                                           SingleOwner* singleOwner)
        : ResourceProvider(sharedContext, singleOwner) {}

DawnResourceProvider::~DawnResourceProvider() = default;

sk_sp<Texture> DawnResourceProvider::createWrappedTexture(const BackendTexture& texture) {
    wgpu::Texture dawnTexture         = texture.getDawnTexture();
    wgpu::TextureView dawnTextureView = texture.getDawnTextureView();
    SkASSERT(!dawnTexture || !dawnTextureView);

    if (!dawnTexture && !dawnTextureView) {
        return {};
    }

    if (dawnTexture) {
        return DawnTexture::MakeWrapped(this->dawnSharedContext(),
                                        texture.dimensions(),
                                        texture.info(),
                                        std::move(dawnTexture));
    } else {
        return DawnTexture::MakeWrapped(this->dawnSharedContext(),
                                        texture.dimensions(),
                                        texture.info(),
                                        std::move(dawnTextureView));
    }
}

sk_sp<GraphicsPipeline> DawnResourceProvider::createGraphicsPipeline(
        const SkRuntimeEffectDictionary* runtimeDict,
        const GraphicsPipelineDesc& pipelineDesc,
        const RenderPassDesc& renderPassDesc) {
    SkSL::Program::Inputs vsInputs, fsInputs;
    SkSL::ProgramSettings settings;

    settings.fForceNoRTFlip = true;

    auto skslCompiler = this->skslCompiler();
    ShaderErrorHandler* errorHandler = fSharedContext->caps()->shaderErrorHandler();

    const RenderStep* step =
            fSharedContext->rendererProvider()->lookup(pipelineDesc.renderStepID());

    bool useShadingSsboIndex =
            fSharedContext->caps()->storageBufferPreferred() && step->performsShading();

    BlendInfo blendInfo;
    bool localCoordsNeeded = false;
#if 0
    std::string vsWGSL, fsWGSL;
    if (!SkSLToWGSL(skslCompiler,
                     GetSkSLFS(fSharedContext->shaderCodeDictionary(),
                               runtimeDict,
                               step,
                               pipelineDesc.paintParamsID(),
                               useShadingSsboIndex,
                               &blendInfo,
                               &localCoordsNeeded),
                     SkSL::ProgramKind::kGraphiteFragment,
                     settings,
                     &fsWGSL,
                     &fsInputs,
                     errorHandler)) {
        SkASSERT(false);
        return nullptr;
    }

    if (!SkSLToWGSL(skslCompiler,
                    GetSkSLVS(step, useShadingSsboIndex, localCoordsNeeded),
                    SkSL::ProgramKind::kGraphiteVertex,
                    settings,
                    &vsWGSL,
                    &vsInputs,
                    errorHandler)) {
        SkASSERT(false);
        return nullptr;
    }

    auto vsModule = DawnCompileWGSLShaderModule(this->dawnSharedContext(), vsWGSL, errorHandler);
    if (!vsModule) {
        SkASSERT(false);
        return nullptr;
    }

    auto fsModule = DawnCompileSPIRVShaderModule(this->dawnSharedContext(), fsWGSL, errorHandler);
    if (!fsModule) {
        SkASSERT(false);
        return nullptr;
    }
#else
    std::string vsSPIRV, fsSPIRV;
    if (!SkSLToSPIRV(skslCompiler,
                     GetSkSLFS(fSharedContext->shaderCodeDictionary(),
                               runtimeDict,
                               step,
                               pipelineDesc.paintParamsID(),
                               useShadingSsboIndex,
                               &blendInfo,
                               &localCoordsNeeded),
                     SkSL::ProgramKind::kGraphiteFragment,
                     settings,
                     &fsSPIRV,
                     &fsInputs,
                     errorHandler)) {
        SkASSERT(false);
        return nullptr;
    }

    if (!SkSLToSPIRV(skslCompiler,
                     GetSkSLVS(step, useShadingSsboIndex, localCoordsNeeded),
                     SkSL::ProgramKind::kGraphiteVertex,
                     settings,
                     &vsSPIRV,
                     &vsInputs,
                     errorHandler)) {
        SkASSERT(false);
        return nullptr;
    }

    SkDebugf("EEEEE compile vsmodule\n");
    auto vsModule = DawnCompileSPIRVShaderModule(this->dawnSharedContext(), vsSPIRV, errorHandler);
    if (!vsModule) {
        SkASSERT(false);
        return nullptr;
    }

    SkDebugf("EEEEE compile fsmodule\n");
    auto fsModule = DawnCompileSPIRVShaderModule(this->dawnSharedContext(), fsSPIRV, errorHandler);
    SkDebugf("EEEEE compile fsmodule done fsModule=%d\n", !!fsModule);
    if (!fsModule) {
        SkASSERT(false);
        return nullptr;
    }
#endif

    return DawnGraphicsPipeline::Make(this->dawnSharedContext(),
                                      step->name(),
                                      {std::move(vsModule), "main"},
                                      step->vertexAttributes(),
                                      step->instanceAttributes(),
                                      step->primitiveType(),
                                      {std::move(fsModule), "main"},
                                      step->depthStencilSettings(),
                                      blendInfo,
                                      renderPassDesc);
}

sk_sp<ComputePipeline> DawnResourceProvider::createComputePipeline(const ComputePipelineDesc&) {
    SkASSERT(false);
    return nullptr;
}

sk_sp<Texture> DawnResourceProvider::createTexture(SkISize dimensions,
                                                   const TextureInfo& info,
                                                   SkBudgeted budgeted) {
    return DawnTexture::Make(this->dawnSharedContext(), dimensions, info, budgeted);

}

sk_sp<Buffer> DawnResourceProvider::createBuffer(size_t size,
                                                 BufferType type,
                                                 PrioritizeGpuReads prioritizeGpuReads) {

    return DawnBuffer::Make(dawnSharedContext(), size, type, prioritizeGpuReads);
}

sk_sp<Sampler> DawnResourceProvider::createSampler(const SkSamplingOptions& options,
                                                   SkTileMode xTileMode,
                                                   SkTileMode yTileMode) {
    return DawnSampler::Make(dawnSharedContext(), options, xTileMode, yTileMode);
}

BackendTexture DawnResourceProvider::onCreateBackendTexture(SkISize dimensions,
                                                            const TextureInfo& info) {
    SkASSERT(false);
    return {};
}

void DawnResourceProvider::onDeleteBackendTexture(BackendTexture& texture) {
    SkASSERT(texture.isValid());
    SkASSERT(texture.backend() == BackendApi::kDawn);
    // TODO: should modify texture?
    // texture.fDawnTexture = nullptr;
    // texture.fDawnTextureView = nullptr;
    SkASSERT(false);
}

const DawnSharedContext* DawnResourceProvider::dawnSharedContext() const {
    return static_cast<const DawnSharedContext*>(fSharedContext);
}

} // namespace skgpu::graphite
