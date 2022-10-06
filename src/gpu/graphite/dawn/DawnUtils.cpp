/*
 * Copyright 2022 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnUtils.h"

#include "include/gpu/ShaderErrorHandler.h"
#include "include/private/SkSLString.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/utils/SkShaderUtils.h"


namespace skgpu::graphite {

bool DawnFormatIsDepthOrStencil(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::Stencil8: // fallthrough
        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

bool DawnFormatIsDepth(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

bool DawnFormatIsStencil(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::Stencil8: // fallthrough
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

wgpu::TextureFormat DawnDepthStencilFlagsToFormat(SkEnumBitMask<DepthStencilFlags> mask) {
    // TODO: Decide if we want to change this to always return a combined depth and stencil format
    // to allow more sharing of depth stencil allocations.
    if (mask == DepthStencilFlags::kDepth) {
        // wgpu::TextureFormatDepth16Unorm is also a universally supported option here
        return wgpu::TextureFormat::Depth32Float;
    } else if (mask == DepthStencilFlags::kStencil) {
        return wgpu::TextureFormat::Stencil8;
    } else if (mask == DepthStencilFlags::kDepthStencil) {
        // wgpu::TextureFormatDepth24Unorm_Stencil8 is supported on Mac family GPUs.
        return wgpu::TextureFormat::Depth32FloatStencil8;
    }
    SkASSERT(false);
    return wgpu::TextureFormat::Undefined;
}

// Print the source code for all shaders generated.
static constexpr bool gPrintSKSL  = true;
static constexpr bool gPrintSPIRV = false;
static constexpr bool gPrintWGSL  = false;

bool SkSLToSPIRV(SkSL::Compiler* compiler,
                 const std::string& sksl,
                 SkSL::ProgramKind programKind,
                 const SkSL::ProgramSettings& settings,
                 std::string* spirv,
                 SkSL::Program::Inputs* outInputs,
                 ShaderErrorHandler* errorHandler) {
#ifdef SK_DEBUG
    std::string src = SkShaderUtils::PrettyPrint(sksl);
#else
    const std::string& src = sksl;
#endif
    std::unique_ptr<SkSL::Program> program = compiler->convertProgram(programKind,
                                                                      src,
                                                                      settings);
    if (!program || !compiler->toSPIRV(*program, spirv)) {
        errorHandler->compileError(src.c_str(), compiler->errorText().c_str());
        return false;
    }

    if (gPrintSKSL || gPrintSPIRV) {
        SkShaderUtils::PrintShaderBanner(programKind);
        if (gPrintSKSL) {
            SkDebugf("SKSL:\n");
            SkShaderUtils::PrintLineByLine(SkShaderUtils::PrettyPrint(sksl));
        }
        if (gPrintSPIRV) {
            SkDebugf("SPIRV:\n");
            SkShaderUtils::PrintLineByLine(SkShaderUtils::PrettyPrint(*spirv));
        }
    }

    *outInputs = program->fInputs;
    return true;
}

bool SkSLToWGSL(SkSL::Compiler* compiler,
                const std::string& sksl,
                SkSL::ProgramKind programKind,
                const SkSL::ProgramSettings& settings,
                std::string* wgsl,
                SkSL::Program::Inputs* outInputs,
                ShaderErrorHandler* errorHandler) {
#ifdef SK_DEBUG
    std::string src = SkShaderUtils::PrettyPrint(sksl);
#else
    const std::string& src = sksl;
#endif
    std::unique_ptr<SkSL::Program> program = compiler->convertProgram(programKind,
                                                                      src,
                                                                      settings);
    if (!program || !compiler->toWGSL(*program, wgsl)) {
        errorHandler->compileError(src.c_str(), compiler->errorText().c_str());
        return false;
    }

    if (gPrintSKSL || gPrintWGSL) {
        SkShaderUtils::PrintShaderBanner(programKind);
        if (gPrintSKSL) {
            SkDebugf("SKSL:\n");
            SkShaderUtils::PrintLineByLine(SkShaderUtils::PrettyPrint(sksl));
        }
        if (gPrintWGSL) {
            SkDebugf("WGSL:\n");
            SkShaderUtils::PrintLineByLine(SkShaderUtils::PrettyPrint(*wgsl));
        }
    }

    *outInputs = program->fInputs;
    return true;
}

wgpu::ShaderModule DawnCompileSPIRVShaderModule(const DawnSharedContext* sharedContext,
                                                const std::string& spirv,
                                                ShaderErrorHandler* errorHandler) {
    wgpu::ShaderModuleSPIRVDescriptor spirvDesc;
    spirvDesc.codeSize = spirv.size() / 4;
    spirvDesc.code = reinterpret_cast<const uint32_t*>(spirv.c_str());

    wgpu::ShaderModuleDescriptor desc;
    desc.nextInChain = &spirvDesc;
    return sharedContext->device().CreateShaderModule(&desc);
}

wgpu::ShaderModule DawnCompileWGSLShaderModule(const DawnSharedContext* sharedContext,
                                               const std::string& wgsl,
                                               ShaderErrorHandler* errorHandler) {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc;
    wgslDesc.source = wgsl.c_str();

    wgpu::ShaderModuleDescriptor desc;
    desc.nextInChain = &wgslDesc;
    return sharedContext->device().CreateShaderModule(&desc);
}

} // namespace skgpu::graphite
