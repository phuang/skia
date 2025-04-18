/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/RenderPassDesc.h"

#include "src/gpu/graphite/Caps.h"
#include "src/gpu/graphite/TextureInfoPriv.h"

namespace skgpu::graphite {

namespace {

const char* to_str(LoadOp op) {
    switch (op) {
        case LoadOp::kLoad:    return "load";
        case LoadOp::kClear:   return "clear";
        case LoadOp::kDiscard: return "discard";
    }

    SkUNREACHABLE;
}

const char* to_str(StoreOp op) {
    switch (op) {
        case StoreOp::kStore:   return "store";
        case StoreOp::kDiscard: return "discard";
    }

    SkUNREACHABLE;
}

} // anonymous namespace

RenderPassDesc RenderPassDesc::Make(const Caps* caps,
                                    const TextureInfo& targetInfo,
                                    LoadOp loadOp,
                                    StoreOp storeOp,
                                    SkEnumBitMask<DepthStencilFlags> depthStencilFlags,
                                    const std::array<float, 4>& clearColor,
                                    bool requiresMSAA,
                                    Swizzle writeSwizzle,
                                    const DstReadStrategy dstReadStrategy) {
    RenderPassDesc desc;
    desc.fWriteSwizzle = writeSwizzle;
    desc.fSampleCount = 1;
    // It doesn't make sense to have a storeOp for our main target not be store. Why are we doing
    // this DrawPass then
    SkASSERT(storeOp == StoreOp::kStore);
    if (requiresMSAA) {
        if (caps->msaaRenderToSingleSampledSupport()) {
            desc.fColorAttachment.fTextureInfo = targetInfo;
            desc.fColorAttachment.fLoadOp = loadOp;
            desc.fColorAttachment.fStoreOp = storeOp;
            desc.fSampleCount = caps->defaultMSAASamplesCount();
        } else {
            // TODO: If the resolve texture isn't readable, the MSAA color attachment will need to
            // be persistently associated with the framebuffer, in which case it's not discardable.
            auto msaaTextureInfo = caps->getDefaultMSAATextureInfo(targetInfo, Discardable::kYes);
            if (msaaTextureInfo.isValid()) {
                desc.fColorAttachment.fTextureInfo = msaaTextureInfo;
                if (loadOp != LoadOp::kClear) {
                    desc.fColorAttachment.fLoadOp = LoadOp::kDiscard;
                } else {
                    desc.fColorAttachment.fLoadOp = LoadOp::kClear;
                }
                desc.fColorAttachment.fStoreOp = StoreOp::kDiscard;

                desc.fColorResolveAttachment.fTextureInfo = targetInfo;
                if (loadOp != LoadOp::kLoad) {
                    desc.fColorResolveAttachment.fLoadOp = LoadOp::kDiscard;
                } else {
                    desc.fColorResolveAttachment.fLoadOp = LoadOp::kLoad;
                }
                desc.fColorResolveAttachment.fStoreOp = storeOp;

                desc.fSampleCount = msaaTextureInfo.numSamples();
            } else {
                // fall back to single sampled
                desc.fColorAttachment.fTextureInfo = targetInfo;
                desc.fColorAttachment.fLoadOp = loadOp;
                desc.fColorAttachment.fStoreOp = storeOp;
            }
        }
    } else {
        desc.fColorAttachment.fTextureInfo = targetInfo;
        desc.fColorAttachment.fLoadOp = loadOp;
        desc.fColorAttachment.fStoreOp = storeOp;
    }
    desc.fClearColor = clearColor;

    if (depthStencilFlags != DepthStencilFlags::kNone) {
        desc.fDepthStencilAttachment.fTextureInfo = caps->getDefaultDepthStencilTextureInfo(
                depthStencilFlags, desc.fSampleCount, targetInfo.isProtected(), Discardable::kYes);
        // Always clear the depth and stencil to 0 at the start of a DrawPass, but discard at the
        // end since their contents do not affect the next frame.
        desc.fDepthStencilAttachment.fLoadOp = LoadOp::kClear;
        desc.fClearDepth = 0.f;
        desc.fClearStencil = 0;
        desc.fDepthStencilAttachment.fStoreOp = StoreOp::kDiscard;
    }

    desc.fDstReadStrategy = dstReadStrategy;

    return desc;
}

SkString RenderPassDesc::toString() const {
    return SkStringPrintf("RP(color: %s, resolve: %s, ds: %s, samples: %u, swizzle: %s, "
                          "clear: c(%f,%f,%f,%f), d(%f), s(0x%02x), dst read: %u)",
                          fColorAttachment.toString().c_str(),
                          fColorResolveAttachment.toString().c_str(),
                          fDepthStencilAttachment.toString().c_str(),
                          fSampleCount,
                          fWriteSwizzle.asString().c_str(),
                          fClearColor[0], fClearColor[1], fClearColor[2], fClearColor[3],
                          fClearDepth,
                          fClearStencil,
                          (unsigned)fDstReadStrategy);
}

SkString RenderPassDesc::toPipelineLabel() const {
    // This intentionally only includes the fixed state that impacts pipeline compilation.
    // We include the load op of the color attachment when there is a resolve attachment because
    // the load may trigger a different renderpass description.
    const char* colorLoadStr = "";

    const bool loadMsaaFromResolve =
            fColorResolveAttachment.fTextureInfo.isValid() &&
            fColorResolveAttachment.fLoadOp == LoadOp::kLoad;

    // This should, technically, check Caps::loadOpAffectsMSAAPipelines before adding the extra
    // string. Only the Metal backend doesn't set that flag, however, so we just assume it is set
    // to reduce plumbing. Since the Metal backend doesn't differentiate its UniqueKeys wrt
    // resolve-loads, this can lead to instances where two Metal Pipeline labels will map to the
    // same UniqueKey (i.e., one with "w/ msaa load" and one without it).
    if (loadMsaaFromResolve /* && Caps::loadOpAffectsMSAAPipelines() */) {
        colorLoadStr = " w/ msaa load";
    }

    const auto& colorTexInfo = fColorAttachment.fTextureInfo;
    const auto& resolveTexInfo = fColorResolveAttachment.fTextureInfo;
    const auto& dsTexInfo = fDepthStencilAttachment.fTextureInfo;
    // TODO: Remove `fSampleCount` in label when the Dawn backend manages its MSAA color attachments
    // directly instead of relying on msaaRenderToSingleSampledSupport().
    return SkStringPrintf("RP(color: %s%s, resolve: %s, ds: %s, samples: %u, swizzle: %s)",
                          TextureInfoPriv::GetAttachmentLabel(colorTexInfo).c_str(),
                          colorLoadStr,
                          TextureInfoPriv::GetAttachmentLabel(resolveTexInfo).c_str(),
                          TextureInfoPriv::GetAttachmentLabel(dsTexInfo).c_str(),
                          fSampleCount,
                          fWriteSwizzle.asString().c_str());
}

SkString AttachmentDesc::toString() const {
    if (fTextureInfo.isValid()) {
        return SkStringPrintf("info: %s loadOp: %s storeOp: %s",
                              fTextureInfo.toString().c_str(),
                              to_str(fLoadOp),
                              to_str(fStoreOp));
    } else {
        return SkString("invalid attachment");
    }
}

} // namespace skgpu::graphite
