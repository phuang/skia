/*
 * Copyright 2022 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnStagingBufferPool_DEFINED
#define skgpu_graphite_DawnStagingBufferPool_DEFINED

#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace skgpu::graphite {
class DawnStagingBufferPool {
public:
    DawnStagingBufferPool(size_t size);

    void writeBuffer(const wgpu::Device& device,
                     const wgpu::CommandEncoder& cmdEncoder,
                     const wgpu::Buffer& bufferToUpdate,
                     const void* data);

private:
    struct StagingBufferRecord {
        DawnStagingBufferPool* fOwner;
        wgpu::Buffer fBuffer;
    };

    std::vector<StagingBufferRecord*> fFreeBuffers;
    std::vector<StagingBufferRecord> fAllocatedBuffers;
    size_t fBufferSize;
};
}  // namespace skgpu::graphite

#endif