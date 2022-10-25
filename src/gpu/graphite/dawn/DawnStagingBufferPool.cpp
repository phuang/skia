/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnStagingBufferPool.h"

#include "include/core/SkTypes.h"
#include "src/gpu/graphite/dawn/DawnQueueManager.h"

namespace skgpu::graphite {
DawnStagingBufferPool::DawnStagingBufferPool(size_t size) : fBufferSize(size) {}

void DawnStagingBufferPool::writeBuffer(const wgpu::Device& device,
                                        DawnQueueManager* cmdQueue,
                                        const wgpu::CommandEncoder& cmdEncoder,
                                        const wgpu::Buffer& bufferToUpdate,
                                        const void* data) {
    StagingBufferRecord* bufferRecord;
    if (fFreeBuffers.empty()) {
        device.Tick();
    }

    if (fFreeBuffers.empty()) {
        StagingBufferRecord newBufferRecord;
        wgpu::BufferDescriptor desc;
        desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite;
        desc.size = fBufferSize;
        desc.mappedAtCreation = true;
        newBufferRecord.fOwner = this;
        newBufferRecord.fBuffer = device.CreateBuffer(&desc);
        if (!newBufferRecord.fBuffer) {
            SkASSERT(false);
        }

        fAllocatedBuffers.push_back(std::move(newBufferRecord));
        bufferRecord = &fAllocatedBuffers.back();
    } else {
        bufferRecord = fFreeBuffers.back();
        fFreeBuffers.pop_back();
    }

    memcpy(bufferRecord->fBuffer.GetMappedRange(), data, fBufferSize);
    bufferRecord->fBuffer.Unmap();

    cmdEncoder.CopyBufferToBuffer(bufferRecord->fBuffer, 0, bufferToUpdate, 0, fBufferSize);

    // delay the next buffer's mapping until the command buffer is submitted
    cmdQueue->registerNextWorkSubmitCallback(
            [](void* userData) {
                auto bufferRecordPtr = static_cast<StagingBufferRecord*>(userData);
                bufferRecordPtr->fBuffer.MapAsync(
                        wgpu::MapMode::Write,
                        0,
                        WGPU_WHOLE_MAP_SIZE,
                        [](WGPUBufferMapAsyncStatus status, void* userData) {
                            SkASSERT(status == WGPUBufferMapAsyncStatus_Success);
                            auto innerBufferRecordPtr = static_cast<StagingBufferRecord*>(userData);
                            innerBufferRecordPtr->fOwner->fFreeBuffers.push_back(
                                    innerBufferRecordPtr);
                        },
                        /*userdata=*/bufferRecordPtr);
            },
            /*userdata=*/bufferRecord);
}

}  // namespace skgpu::graphite
