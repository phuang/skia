/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnCaps.h"

#include <algorithm>

#include "include/gpu/graphite/TextureInfo.h"
#include "src/gpu/graphite/AttachmentTypes.h"
#include "src/gpu/graphite/ComputePipelineDesc.h"
#include "src/gpu/graphite/GraphicsPipelineDesc.h"
#include "src/gpu/graphite/GraphiteResourceKey.h"
#include "src/gpu/graphite/dawn/DawnUtils.h"


namespace {

// These are all the valid wgpu::TextureFormat that we currently support in Skia.
// They are roughly ordered from most frequently used to least to improve look
// up times in arrays.
static constexpr wgpu::TextureFormat kFormats[] = {
    wgpu::TextureFormat::RGBA8Unorm,
    wgpu::TextureFormat::R8Unorm,
    wgpu::TextureFormat::BGRA8Unorm,

    wgpu::TextureFormat::RGBA16Float,

    wgpu::TextureFormat::Stencil8,
    wgpu::TextureFormat::Depth32Float,
    wgpu::TextureFormat::Depth32FloatStencil8,

    wgpu::TextureFormat::Undefined,
};

}

namespace skgpu::graphite {

DawnCaps::DawnCaps(const wgpu::Device& device, const ContextOptions& options)
    : Caps() {
    this->initCaps(device);
    this->initFormatTable(device);
    this->finishInitialization(options);
}

DawnCaps::~DawnCaps() = default;

bool DawnCaps::onIsTexturable(const TextureInfo& info) const {
    if (!(info.dawnTextureSpec().fUsage & wgpu::TextureUsage::TextureBinding)) {
        return false;
    }
    return this->isTexturable(info.dawnTextureSpec().fFormat);
}

bool DawnCaps::isTexturable(wgpu::TextureFormat format) const {
    const FormatInfo& formatInfo = this->getFormatInfo(format);
    return SkToBool(FormatInfo::kTexturable_Flag && formatInfo.fFlags);
}

bool DawnCaps::isRenderable(const TextureInfo& info) const {
    return info.dawnTextureSpec().fUsage & wgpu::TextureUsage::RenderAttachment &&
    this->isRenderable(info.dawnTextureSpec().fFormat, info.numSamples());
}

uint32_t DawnCaps::maxRenderTargetSampleCount(wgpu::TextureFormat format) const {
    const FormatInfo& formatInfo = this->getFormatInfo(format);
    if (!SkToBool(formatInfo.fFlags & FormatInfo::kRenderable_Flag)) {
        return 0;
    }
    if (SkToBool(formatInfo.fFlags & FormatInfo::kMSAA_Flag)) {
        return fColorSampleCounts[fColorSampleCounts.size() - 1];
    } else {
        return 1;
    }
}

bool DawnCaps::isRenderable(wgpu::TextureFormat format, uint32_t sampleCount) const {
    return sampleCount <= this->maxRenderTargetSampleCount(format);
}

TextureInfo DawnCaps::getDefaultSampledTextureInfo(SkColorType colorType,
                                                   uint32_t levelCount,
                                                   Protected,
                                                   Renderable renderable) const {
    wgpu::TextureUsage usage = wgpu::TextureUsage::TextureBinding;
    if (renderable == Renderable::kYes) {
        usage |= wgpu::TextureUsage::RenderAttachment;
    }

    wgpu::TextureFormat format = this->getFormatFromColorType(colorType);
    if (format == wgpu::TextureFormat::Undefined) {
        SkASSERT(false);
        return {};
    }

    DawnTextureInfo info;
    info.fSampleCount = 1;
    info.fLevelCount = levelCount;
    info.fFormat = format;
    info.fUsage = usage;
    // info.fStorageMode = MTLStorageModePrivate;
    // info.fFramebufferOnly = false;

    return info;
}

TextureInfo DawnCaps::getDefaultMSAATextureInfo(const TextureInfo& singleSampledInfo,
                                                Discardable discardable) const {
    const DawnTextureSpec& singleSpec = singleSampledInfo.dawnTextureSpec();

    constexpr wgpu::TextureUsage usage = wgpu::TextureUsage::RenderAttachment |
                                         wgpu::TextureUsage::TextureBinding;

    DawnTextureInfo info;
    info.fSampleCount = this->defaultMSAASamples();
    info.fLevelCount = 1;
    info.fFormat = singleSpec.fFormat;
    info.fUsage = usage;
    // TODO: ?
    // info.fStorageMode = this->getDefaultMSAAStorageMode(discardable);
    // info.fFramebufferOnly = false;

    return info;
}

TextureInfo DawnCaps::getDefaultDepthStencilTextureInfo(SkEnumBitMask<DepthStencilFlags> depthStencilType,
                                                        uint32_t sampleCount,
                                                        Protected) const {
    constexpr wgpu::TextureUsage usage = wgpu::TextureUsage::RenderAttachment |
                                         wgpu::TextureUsage::TextureBinding;

    DawnTextureInfo info;
    info.fSampleCount = sampleCount;
    info.fLevelCount = 1;
    info.fFormat = DawnDepthStencilFlagsToFormat(depthStencilType);
    info.fUsage = usage;
    // TODO: ?
    // info.fStorageMode = this->getDefaultMSAAStorageMode(Discardable::kYes);
    // info.fFramebufferOnly = false;
    return info;
}

const Caps::ColorTypeInfo* DawnCaps::getColorTypeInfo(SkColorType colorType,
                                                      const TextureInfo& textureInfo) const {
    auto dawnFormat = textureInfo.dawnTextureSpec().fFormat;
    if (dawnFormat == wgpu::TextureFormat::Undefined) {
        SkASSERT(false);
        return nullptr;
    }

    const FormatInfo& info = this->getFormatInfo(dawnFormat);
    for (int i = 0; i < info.fColorTypeInfoCount; ++i) {
        const ColorTypeInfo& ctInfo = info.fColorTypeInfos[i];
        if (ctInfo.fColorType == colorType) {
            return &ctInfo;
        }
    }

    SkASSERT(false);
    return nullptr;
}

size_t DawnCaps::getTransferBufferAlignment(size_t bytesPerPixel) const {
    return std::max(bytesPerPixel, this->getMinBufferAlignment());
}

bool DawnCaps::supportsWritePixels(const TextureInfo& textureInfo) const {
    const auto& spec = textureInfo.dawnTextureSpec();
    return spec.fUsage & wgpu::TextureUsage::CopyDst;
}

bool DawnCaps::supportsReadPixels(const TextureInfo& textureInfo) const {
    const auto& spec = textureInfo.dawnTextureSpec();
    return spec.fUsage & wgpu::TextureUsage::CopySrc;
}

SkColorType DawnCaps::supportedWritePixelsColorType(SkColorType dstColorType,
                                                    const TextureInfo& dstTextureInfo,
                                                    SkColorType srcColorType) const {
    SkASSERT(false);
    return kUnknown_SkColorType;
}

SkColorType DawnCaps::supportedReadPixelsColorType(SkColorType srcColorType,
                                                   const TextureInfo& srcTextureInfo,
                                                   SkColorType dstColorType) const {
    SkASSERT(false);
    return kUnknown_SkColorType;
}

void DawnCaps::initCaps(const wgpu::Device& device) {
    wgpu::SupportedLimits limits;
    if (!device.GetLimits(&limits)) {
        SkASSERT(false);
    }
    fMaxTextureSize = limits.limits.maxTextureDimension2D;

    // TODO
    fRequiredUniformBufferAlignment = 16;
    fRequiredStorageBufferAlignment = fRequiredUniformBufferAlignment;

    // TODO
    fStorageBufferSupport = false;
    fStorageBufferPreferred = false;

    // TODO
    fClampToBorderSupport = false;

    // TODO
    // Init sample counts. All devices support 1 (i.e. 0 in skia).
    fColorSampleCounts.push_back(1);
    // TODO
    // for (auto sampleCnt : {2, 4, 8}) {
    //     if ([device supportsTextureSampleCount:sampleCnt]) {
    //         fColorSampleCounts.push_back(sampleCnt);
    //     }
    // }
}

void DawnCaps::initFormatTable(const wgpu::Device& device) {
    FormatInfo* info;
    // Format: RGBA8Unorm
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::RGBA8Unorm)];
        info->fFlags = FormatInfo::kAllFlags;
        info->fColorTypeInfoCount = 2;
        info->fColorTypeInfos.reset(new ColorTypeInfo[info->fColorTypeInfoCount]());
        int ctIdx = 0;
        // Format: RGBA8Unorm, Surface: kRGBA_8888
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kRGBA_8888_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
        }
        // Format: RGBA8Unorm, Surface: kRGB_888x
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kRGB_888x_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag;
            ctInfo.fReadSwizzle = skgpu::Swizzle::RGB1();
        }
    }


    // Format: R8Unorm
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::R8Unorm)];
        info->fFlags = FormatInfo::kAllFlags;
        info->fColorTypeInfoCount = 3;
        info->fColorTypeInfos.reset(new ColorTypeInfo[info->fColorTypeInfoCount]());
        int ctIdx = 0;
        // Format: R8Unorm, Surface: kR8_unorm
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kR8_unorm_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
        }
        // Format: R8Unorm, Surface: kAlpha_8
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kAlpha_8_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
            ctInfo.fReadSwizzle = skgpu::Swizzle("000r");
            ctInfo.fWriteSwizzle = skgpu::Swizzle("a000");
        }
        // Format: R8Unorm, Surface: kGray_8
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kGray_8_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag;
            ctInfo.fReadSwizzle = skgpu::Swizzle("rrr1");
        }
    }

    // Format: BGRA8Unorm
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::BGRA8Unorm)];
        info->fFlags = FormatInfo::kAllFlags;
        info->fColorTypeInfoCount = 1;
        info->fColorTypeInfos.reset(new ColorTypeInfo[info->fColorTypeInfoCount]());
        int ctIdx = 0;
        // Format: BGRA8Unorm, Surface: kBGRA_8888
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kBGRA_8888_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
        }
    }

    // Format: RGBA16Float
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::RGBA16Float)];
        info->fFlags = FormatInfo::kAllFlags;
        info->fColorTypeInfoCount = 1;
        info->fColorTypeInfos.reset(new ColorTypeInfo[info->fColorTypeInfoCount]());
        int ctIdx = 0;
        // Format: RGBA16Float, Surface: RGBA_F16
        {
            auto& ctInfo = info->fColorTypeInfos[ctIdx++];
            ctInfo.fColorType = kRGBA_F16_SkColorType;
            ctInfo.fFlags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
        }
    }

    /*
     * Non-color formats
     */

    // Format: Stencil8
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::Stencil8)];
        info->fFlags = FormatInfo::kMSAA_Flag;
        info->fColorTypeInfoCount = 0;
    }

    // Format: Depth32Float
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::Depth32Float)];
        info->fFlags = FormatInfo::kMSAA_Flag;
        // if (this->isMac() || fFamilyGroup >= 3) {
        //     info->fFlags |= FormatInfo::kResolve_Flag;
        // }
        info->fColorTypeInfoCount = 0;
    }

    // Format: Depth32Float_Stencil8
    {
        info = &fFormatTable[GetFormatIndex(wgpu::TextureFormat::Depth32FloatStencil8)];
        info->fFlags = FormatInfo::kMSAA_Flag;
        // if (this->isMac() || fFamilyGroup >= 3) {
        //     info->fFlags |= FormatInfo::kResolve_Flag;
        // }
         info->fColorTypeInfoCount = 0;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Map SkColorTypes (used for creating SkSurfaces) to wgpu::TextureFormat.
    // The order in which the formats are passed into the setColorType function
    // indicates the priority in selecting which format we use for a given
    // SkColorType.

    std::fill_n(fColorTypeToFormatTable, kSkColorTypeCnt, wgpu::TextureFormat::Undefined);

    this->setColorType(kAlpha_8_SkColorType,          { wgpu::TextureFormat::R8Unorm });
    // if (@available(macOS 11.0, iOS 8.0, *)) {
    //     if (this->isApple()) {
    //         this->setColorType(kRGB_565_SkColorType, {MTLPixelFormatB5G6R5Unorm});
    //     }
    // }

    this->setColorType(kRGBA_8888_SkColorType,        { wgpu::TextureFormat::RGBA8Unorm });
    this->setColorType(kRGB_888x_SkColorType,         { wgpu::TextureFormat::RGBA8Unorm });
    this->setColorType(kBGRA_8888_SkColorType,        { wgpu::TextureFormat::BGRA8Unorm });
    this->setColorType(kGray_8_SkColorType,           { wgpu::TextureFormat::R8Unorm });
    this->setColorType(kR8_unorm_SkColorType,         { wgpu::TextureFormat::R8Unorm });
    this->setColorType(kRGBA_F16_SkColorType,         { wgpu::TextureFormat::RGBA16Float });
}

size_t DawnCaps::GetFormatIndex(wgpu::TextureFormat format) {
    size_t index = 0;
    for (auto f : kFormats) {
        if (format == f)
            return index;
        ++index;
    }
    SkASSERT(false);
    return 0;
}

void DawnCaps::setColorType(SkColorType colorType, std::initializer_list<wgpu::TextureFormat> formats) {
// #ifdef SK_DEBUG
//     for (size_t i = 0; i < kNumMtlFormats; ++i) {
//         const auto& formatInfo = fFormatTable[i];
//         for (int j = 0; j < formatInfo.fColorTypeInfoCount; ++j) {
//             const auto& ctInfo = formatInfo.fColorTypeInfos[j];
//             if (ctInfo.fColorType == colorType) {
//                 bool found = false;
//                 for (auto it = formats.begin(); it != formats.end(); ++it) {
//                     if (kMtlFormats[i] == *it) {
//                         found = true;
//                     }
//                 }
//                 SkASSERT(found);
//             }
//         }
//     }
// #endif
    int idx = static_cast<int>(colorType);
    for (auto it = formats.begin(); it != formats.end(); ++it) {
        const auto& info = this->getFormatInfo(*it);
        for (int i = 0; i < info.fColorTypeInfoCount; ++i) {
            if (info.fColorTypeInfos[i].fColorType == colorType) {
                fColorTypeToFormatTable[idx] = *it;
                return;
            }
        }
    }
}

uint64_t DawnCaps::getRenderPassDescKey(const RenderPassDesc& renderPassDesc) const {
    DawnTextureInfo colorInfo, depthStencilInfo;
    renderPassDesc.fColorAttachment.fTextureInfo.getDawnTextureInfo(&colorInfo);
    renderPassDesc.fDepthStencilAttachment.fTextureInfo.getDawnTextureInfo(&depthStencilInfo);
    SkASSERT(static_cast<uint32_t>(colorInfo.fFormat) < 65535 &&
             static_cast<uint32_t>(depthStencilInfo.fFormat) < 65535);
    uint32_t colorAttachmentKey =
            static_cast<uint32_t>(colorInfo.fFormat) << 16 | colorInfo.fSampleCount;
    uint32_t dsAttachmentKey =
            static_cast<uint32_t>(depthStencilInfo.fFormat) << 16 | depthStencilInfo.fSampleCount;
    return (((uint64_t) colorAttachmentKey) << 32) | dsAttachmentKey;
}

UniqueKey DawnCaps::makeGraphicsPipelineKey(const GraphicsPipelineDesc& pipelineDesc,
                                            const RenderPassDesc& renderPassDesc) const {
    UniqueKey pipelineKey;
    {
        static const skgpu::UniqueKey::Domain kGraphicsPipelineDomain = UniqueKey::GenerateDomain();
        // 4 uint32_t's (render step id, paint id, uint64 renderpass desc)
        UniqueKey::Builder builder(&pipelineKey, kGraphicsPipelineDomain, 4, "GraphicsPipeline");
        // add graphicspipelinedesc key
        builder[0] = pipelineDesc.renderStepID();
        builder[1] = pipelineDesc.paintParamsID().asUInt();

        // add renderpassdesc key
        uint64_t renderPassKey = this->getRenderPassDescKey(renderPassDesc);
        builder[2] = renderPassKey & 0xFFFFFFFF;
        builder[3] = (renderPassKey >> 32) & 0xFFFFFFFF;
        builder.finish();
    }

    return pipelineKey;
}

UniqueKey DawnCaps::makeComputePipelineKey(const ComputePipelineDesc& pipelineDesc) const {
    UniqueKey pipelineKey;
    {
        static const skgpu::UniqueKey::Domain kComputePipelineDomain = UniqueKey::GenerateDomain();
        SkSpan<const uint32_t> pipelineDescKey = pipelineDesc.asKey();
        UniqueKey::Builder builder(
                &pipelineKey, kComputePipelineDomain, pipelineDescKey.size(), "ComputePipeline");
        // Add ComputePipelineDesc key
        for (unsigned int i = 0; i < pipelineDescKey.size(); ++i) {
            builder[i] = pipelineDescKey[i];
        }

        // TODO(b/240615224): The local work group size may need to factor into the key on platforms
        // that don't support specialization constants and require the workgroup/threadgroup size to
        // be specified in the shader text (D3D12, Vulkan 1.0, and OpenGL).

        builder.finish();
    }
    return pipelineKey;
}

namespace {
// There are only a few possible valid sample counts (1, 2, 4, 8, 16). So we can key on those 5
// options instead of the actual sample value.
uint32_t samples_to_key(uint32_t numSamples) {
    switch (numSamples) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        case 16:
            return 4;
        default:
            SkUNREACHABLE;
    }
}
}

void DawnCaps::buildKeyForTexture(SkISize dimensions,
                                  const TextureInfo& info,
                                  ResourceType type,
                                  Shareable shareable,
                                  GraphiteResourceKey* key) const {
    const DawnTextureSpec& dawnSpec = info.dawnTextureSpec();

    SkASSERT(!dimensions.isEmpty());

    SkASSERT(dawnSpec.fFormat != wgpu::TextureFormat::Undefined);
    uint32_t formatKey = static_cast<uint32_t>(dawnSpec.fFormat);

    uint32_t samplesKey = samples_to_key(info.numSamples());
    // We don't have to key the number of mip levels because it is inherit in the combination of
    // isMipped and dimensions.
    bool isMipped = info.numMipLevels() > 1;
    Protected isProtected = info.isProtected();

    // bool isFBOnly = dawnSpec.fFramebufferOnly;

    // Confirm all the below parts of the key can fit in a single uint32_t. The sum of the shift
    // amounts in the asserts must be less than or equal to 32.
    SkASSERT(samplesKey                             < (1u << 3));
    SkASSERT(static_cast<uint32_t>(isMipped)        < (1u << 1));
    SkASSERT(static_cast<uint32_t>(isProtected)     < (1u << 1));
    SkASSERT(static_cast<uint32_t>(dawnSpec.fUsage) < (1u << 5));
    // SkASSERT(dawnSpec.fStorageMode               < (1u << 2));
    // SkASSERT(static_cast<uint32_t>(isFBOnly)     < (1u << 1));

    // We need two uint32_ts for dimensions, 1 for format, and 1 for the rest of the key;
    static int kNum32DataCnt = 2 + 1 + 1;

    GraphiteResourceKey::Builder builder(key, type, kNum32DataCnt, shareable);

    builder[0] = dimensions.width();
    builder[1] = dimensions.height();
    builder[2] = formatKey;
    builder[3] = (samplesKey                                   << 0) |
                 (static_cast<uint32_t>(isMipped)              << 3) |
                 (static_cast<uint32_t>(isProtected)           << 4) |
                 (static_cast<uint32_t>(dawnSpec.fUsage)       << 5);

}

} // namespace skgpu::graphite

