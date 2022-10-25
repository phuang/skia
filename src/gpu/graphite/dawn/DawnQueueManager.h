/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnQueueManager_DEFINED
#define skgpu_graphite_DawnQueueManager_DEFINED

#include "src/gpu/graphite/QueueManager.h"

#include "webgpu/webgpu_cpp.h"

namespace skgpu::graphite {

struct DawnBackendContext;
class DawnSharedContext;
class SharedContext;

class DawnQueueManager : public QueueManager {
public:
    DawnQueueManager(const DawnBackendContext&, const SharedContext*);
    ~DawnQueueManager() override {}

    using SubmitCallback = void (*)(void*);
    void registerNextWorkSubmitCallback(SubmitCallback callback, void* userdata);
private:
    const DawnSharedContext* dawnSharedContext() const;

    std::unique_ptr<CommandBuffer> getNewCommandBuffer(ResourceProvider*) override;
    OutstandingSubmission onSubmitToGpu() override;

#if GRAPHITE_TEST_UTILS
    void startCapture() override;
    void stopCapture() override;
#endif

    wgpu::Queue fQueue;

    std::vector<std::pair<void*, SubmitCallback>> fNextSubmitUserCallbacks;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnQueueManager_DEFINED
