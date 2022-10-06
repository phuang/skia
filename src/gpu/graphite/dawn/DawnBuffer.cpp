/*
 * Copyright 2022 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnBuffer.h"

#include "src/gpu/graphite/dawn/DawnSharedContext.h"

namespace skgpu::graphite {

#ifdef SK_ENABLE_DAWN_DEBUG_INFO
const char* kBufferTypeNames[kBufferTypeCount] = {
    "Vertex",
    "Index",
    "Xfer CPU to GPU",
    "Xfer GPU to CPU",
    "Uniform",
    "Storage",
};
#endif

sk_sp<Buffer> DawnBuffer::Make(const DawnSharedContext* sharedContext,
                               size_t size,
                               BufferType type,
                               PrioritizeGpuReads prioritizeGpuReads) {
    if (size <= 0) {
        return nullptr;
    }

    const DawnCaps* dawnCaps = sharedContext->dawnCaps();


    wgpu::BufferUsage usage = wgpu::BufferUsage::None;
    switch (type) {
    case BufferType::kVertex:
        usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        break;
    case BufferType::kIndex:
        usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
        break;
    case BufferType::kXferCpuToGpu:
        usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite;
        break;
    case BufferType::kXferGpuToCpu:
        usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        break;
    case BufferType::kUniform:
        usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        break;
    case BufferType::kStorage:
        usage = wgpu::BufferUsage::Storage;
        break;
    }

    size = SkAlignTo(size, dawnCaps->getMinBufferAlignment());
    wgpu::BufferDescriptor desc;
#ifdef SK_ENABLE_DAWN_DEBUG_INFO
    desc.label = kBufferTypeNames[static_cast<int>(type)];
#endif
    desc.usage = usage;
    desc.size  = size;
    // For wgpu::Buffer can be mapped at creation time for the initial data
    // uploading. we should use it for better performance?
    // desc.mappedAtCreation = true;

    auto buffer = sharedContext->device().CreateBuffer(&desc);
    if (!buffer) {
        SkASSERT(false);
        return {};
    }

    return sk_sp<Buffer>(new DawnBuffer(sharedContext,
                                        size,
                                        type,
                                        prioritizeGpuReads,
                                        std::move(buffer)));
}

DawnBuffer::DawnBuffer(const DawnSharedContext* sharedContext,
                       size_t size,
                       BufferType type,
                       PrioritizeGpuReads prioritizeGpuReads,
                       wgpu::Buffer buffer)
        : Buffer(sharedContext, size, type, prioritizeGpuReads)
        , fBuffer(std::move(buffer)) {}

void DawnBuffer::onMap() {
    SkASSERT(fBuffer);
    SkASSERT(!this->isMapped());

    bool supportMapRead  = fBuffer.GetUsage() & wgpu::BufferUsage::MapRead;
    bool supportMapWrite = fBuffer.GetUsage() & wgpu::BufferUsage::MapWrite;

    // Dawn doesn't allow map Vertex, Index, Uniform buffer, so we have to use
    // a staging buffer to emulate map() and unmap().
    // TODO: for better performance, we should use a staging buffer backed
    // by wgpu::Buffer with CopySrc|MapWrite usage.
    // Or improve graphite to use wgpu::Queue::WriteBuffer() directly.
    if (!(supportMapRead || supportMapWrite)) {
        fStagingBuffer.resize(size());
        fMapPtr = fStagingBuffer.data();
        // TODO: copy content from fBuffer to fStagingBuffer, if supportMapRead
        // is true.
        return;
    }

    std::optional<wgpu::BufferMapAsyncStatus> status;
    fBuffer.MapAsync(supportMapWrite ? wgpu::MapMode::Write: wgpu::MapMode::Read,
                     0,
                     fBuffer.GetSize(),
                     [](WGPUBufferMapAsyncStatus s, void* userData) {
                        auto* status = reinterpret_cast<std::optional<WGPUBufferMapAsyncStatus>*>(userData);
                        status->emplace(s);
                     },
                     &status);

    while(!status) {
        this->dawnSharedContext()->device().Tick();
    }
    SkASSERT(status.value() == wgpu::BufferMapAsyncStatus::Success);

    fMapPtr = fBuffer.GetMappedRange();
    SkASSERT(fMapPtr);
}

void DawnBuffer::onUnmap() {
    SkASSERT(fBuffer);
    SkASSERT(this->isMapped());

    fMapPtr = nullptr;

    bool supportMapRead  = fBuffer.GetUsage() & wgpu::BufferUsage::MapRead;
    bool supportMapWrite = fBuffer.GetUsage() & wgpu::BufferUsage::MapWrite;
    if (supportMapRead || supportMapWrite) {
        fBuffer.Unmap();
        return;
    }

    if (supportMapWrite) {
        dawnSharedContext()->queue().WriteBuffer(fBuffer,
                                                 0,
                                                 fStagingBuffer.data(),
                                                 size());
    }
}

void DawnBuffer::freeGpuData() {
    fBuffer = nullptr;
}

} // namespace skgpu::graphite

