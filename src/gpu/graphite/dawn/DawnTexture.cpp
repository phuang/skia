/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnTexture.h"

#include "include/gpu/graphite/dawn/DawnTypes.h"
#include "include/private/gpu/graphite/DawnTypesPriv.h"
#include "src/gpu/graphite/dawn/DawnCaps.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"
#include "src/gpu/graphite/dawn/DawnUtils.h"


namespace skgpu::graphite {
namespace {
const char* texture_info_to_label(const TextureInfo& info,
                                  const DawnTextureSpec& dawnSpec) {
#ifdef SK_ENABLE_DAWN_DEBUG_INFO
    if (dawnSpec.fUsage & wgpu::TextureUsage::RenderAttachment) {
        if (DawnFormatIsDepthOrStencil(dawnSpec.fFormat)) {
            return "DepthStencil";
        } else {
            if (info.numSamples() > 1) {
                if (dawnSpec.fUsage & wgpu::TextureUsage::TextureBinding) {
                    return "MSAA SampledTexture-ColorAttachment";
                } else {
                    return "MSAA ColorAttachment";
                }
            } else {
                if (dawnSpec.fUsage & wgpu::TextureUsage::TextureBinding) {
                    return "SampledTexture-ColorAttachment";
                } else {
                    return "ColorAttachment";
                }
            }
        }
    } else {
        SkASSERT(dawnSpec.fUsage & wgpu::TextureUsage::TextureBinding);
        return "SampledTexture";
    }
#else
    return nullptr;
#endif
}
}

wgpu::Texture DawnTexture::MakeDawnTexture(const DawnSharedContext* sharedContext,
                                           SkISize dimensions,
                                           const TextureInfo& info) {
    const Caps* caps = sharedContext->caps();
    if (dimensions.width() > caps->maxTextureSize() ||
        dimensions.height() > caps->maxTextureSize()) {
        SkASSERT(false);
        return {};
    }

    const DawnTextureSpec& dawnSpec = info.dawnTextureSpec();

    // TODO?
    // SkASSERT(!dawnSpec.fFramebufferOnly);

    if (dawnSpec.fUsage & wgpu::TextureUsage::TextureBinding && !caps->isTexturable(info)) {
        SkASSERT(false);
        return {};
    }

    if (dawnSpec.fUsage & wgpu::TextureUsage::RenderAttachment &&
        !(caps->isRenderable(info) || DawnFormatIsDepthOrStencil(dawnSpec.fFormat))) {
        SkASSERT(false);
        return {};
    }

    wgpu::TextureDescriptor desc;
    desc.label                      = texture_info_to_label(info, dawnSpec);
    desc.usage                      = dawnSpec.fUsage;
    desc.dimension                  = wgpu::TextureDimension::e2D;
    desc.size.width                 = dimensions.width();
    desc.size.height                = dimensions.height();
    desc.size.depthOrArrayLayers    = 1;
    desc.format                     = dawnSpec.fFormat;
    desc.mipLevelCount              = info.numMipLevels();
    desc.sampleCount                = info.numSamples();
    desc.viewFormatCount            = 0;
    desc.viewFormats                = nullptr;

    auto texture = sharedContext->device().CreateTexture(&desc);
    if (!texture) {
        SkASSERT(false);
        return {};
    }

    return texture;
}

DawnTexture::DawnTexture(const DawnSharedContext* sharedContext,
                         SkISize dimensions,
                         const TextureInfo& info,
                         wgpu::Texture texture,
                         wgpu::TextureView textureView,
                         Ownership ownership,
                         SkBudgeted budgeted)
        : Texture(sharedContext, dimensions, info, ownership, budgeted)
        , fTexture(std::move(texture))
        , fTextureView(std::move(textureView)) {}

sk_sp<Texture> DawnTexture::Make(const DawnSharedContext* sharedContext,
                                 SkISize dimensions,
                                 const TextureInfo& info,
                                 SkBudgeted budgeted) {
    auto texture = MakeDawnTexture(sharedContext, dimensions, info);
    if (!texture) {
        SkASSERT(false);
        return {};
    }
    return sk_sp<Texture>(new DawnTexture(sharedContext,
                                          dimensions,
                                          info,
                                          std::move(texture),
                                          nullptr,
                                          Ownership::kOwned,
                                          budgeted));
}

sk_sp<Texture> DawnTexture::MakeWrapped(const DawnSharedContext* sharedContext,
                                        SkISize dimensions,
                                        const TextureInfo& info,
                                        wgpu::Texture texture) {
    if (!texture) {
        SkASSERT(false);
        return {};
    }
    return sk_sp<Texture>(new DawnTexture(sharedContext,
                                          dimensions,
                                          info,
                                          std::move(texture),
                                          nullptr,
                                          Ownership::kWrapped,
                                          SkBudgeted::kNo));
}

sk_sp<Texture> DawnTexture::MakeWrapped(const DawnSharedContext* sharedContext,
                                        SkISize dimensions,
                                        const TextureInfo& info,
                                        wgpu::TextureView textureView) {
    if (!textureView) {
        SkASSERT(false);
        return {};
    }
    return sk_sp<Texture>(new DawnTexture(sharedContext,
                                          dimensions,
                                          info,
                                          nullptr,
                                          std::move(textureView),
                                          Ownership::kWrapped,
                                          SkBudgeted::kNo));
}

void DawnTexture::freeGpuData() {
    fTexture = nullptr;
}

} // namespace skgpu::graphite

