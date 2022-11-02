/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnQueueManager.h"

#include "include/gpu/graphite/dawn/DawnBackendContext.h"
#include "src/gpu/graphite/dawn/DawnCommandBuffer.h"
#include "src/gpu/graphite/dawn/DawnResourceProvider.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"

namespace skgpu::graphite {

DawnQueueManager::DawnQueueManager(const DawnBackendContext& backendContext,
                                   const SharedContext* sharedContext)
        : QueueManager(sharedContext), fQueue(backendContext.fQueue) {}

const DawnSharedContext* DawnQueueManager::dawnSharedContext() const {
    return static_cast<const DawnSharedContext*>(fSharedContext);
}

std::unique_ptr<CommandBuffer> DawnQueueManager::getNewCommandBuffer(
        ResourceProvider* resourceProvider) {
    return DawnCommandBuffer::Make(
            dawnSharedContext(), this, static_cast<DawnResourceProvider*>(resourceProvider));
}

class DawnWorkSubmission final : public GpuWorkSubmission {
public:
    DawnWorkSubmission(std::unique_ptr<CommandBuffer> cmdBuffer,
                       QueueManager* queueManager,
                       wgpu::Device device)
            : GpuWorkSubmission(std::move(cmdBuffer), queueManager), fDevice(std::move(device)) {}
    ~DawnWorkSubmission() override {}

    bool isFinished() override {
        if (!fFinished) {
            fDevice.Tick();
        }
        return fFinished;
    }
    void waitUntilFinished() override {
        while (!fFinished) {
            fDevice.Tick();
        }
    }

    bool fFinished = false;

private:
    wgpu::Device fDevice;
};

QueueManager::OutstandingSubmission DawnQueueManager::onSubmitToGpu() {
    SkASSERT(fCurrentCommandBuffer);
    DawnCommandBuffer* dawnCmdBuffer = static_cast<DawnCommandBuffer*>(fCurrentCommandBuffer.get());
    auto wgpuCmdBuffer = dawnCmdBuffer->finishEncoding();
    if (!wgpuCmdBuffer) {
        fCurrentCommandBuffer->callFinishedProcs(/*success=*/false);
        return nullptr;
    }

    fQueue.Submit(1, &wgpuCmdBuffer);
    dawnCmdBuffer->onSubmitted();

    std::unique_ptr<DawnWorkSubmission> submission(new DawnWorkSubmission(
            std::move(fCurrentCommandBuffer), this, dawnSharedContext()->device()));

    fQueue.OnSubmittedWorkDone(
            0,
            [](WGPUQueueWorkDoneStatus, void* userdata) {
                auto finishedFlagPtr = static_cast<bool*>(userdata);
                *finishedFlagPtr = true;
            },
            &submission->fFinished);

    // Invoke user registered callbacks
    for (auto& entry : fNextSubmitUserCallbacks) {
        entry.second(entry.first);
    }

    fNextSubmitUserCallbacks.resize(0);

    return submission;
}

void DawnQueueManager::registerNextWorkSubmitCallback(SubmitCallback callback, void* userdata) {
    fNextSubmitUserCallbacks.emplace_back(userdata, callback);
}

#if GRAPHITE_TEST_UTILS
void DawnQueueManager::startCapture() {}

void DawnQueueManager::stopCapture() {}
#endif

}  // namespace skgpu::graphite
