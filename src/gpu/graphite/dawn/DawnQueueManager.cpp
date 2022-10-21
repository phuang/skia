/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/dawn/DawnQueueManager.h"

#include "src/gpu/graphite/GpuWorkSubmission.h"
#include "include/gpu/graphite/dawn/DawnBackendContext.h"
#include "src/gpu/graphite/dawn/DawnCommandBuffer.h"
#include "src/gpu/graphite/dawn/DawnResourceProvider.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"


namespace skgpu::graphite {

DawnQueueManager::DawnQueueManager(const DawnBackendContext& backendContext,
                                   const SharedContext* sharedContext)
        : QueueManager(sharedContext)
        , fQueue(backendContext.fQueue) {}

const DawnSharedContext* DawnQueueManager::dawnSharedContext() const {
    return static_cast<const DawnSharedContext*>(fSharedContext);
}

std::unique_ptr<CommandBuffer> DawnQueueManager::getNewCommandBuffer(ResourceProvider* resourceProvider) {
    return DawnCommandBuffer::Make(dawnSharedContext(),
                                   static_cast<DawnResourceProvider*>(resourceProvider));
}

class DawnWorkSubmission final : public GpuWorkSubmission {
public:
    DawnWorkSubmission(std::unique_ptr<CommandBuffer> cmdBuffer, QueueManager* queueManager)
        : GpuWorkSubmission(std::move(cmdBuffer), queueManager) {}
    ~DawnWorkSubmission() override {}

    bool isFinished() override {
        return static_cast<DawnCommandBuffer*>(this->commandBuffer())->isFinished();
    }
    void waitUntilFinished() override {
        return static_cast<DawnCommandBuffer*>(this->commandBuffer())->waitUntilFinished();
    }
};

QueueManager::OutstandingSubmission DawnQueueManager::onSubmitToGpu() {
    SkASSERT(fCurrentCommandBuffer);
    DawnCommandBuffer* dawnCmdBuffer = static_cast<DawnCommandBuffer*>(fCurrentCommandBuffer.get());
    if (!dawnCmdBuffer->commit()) {
        fCurrentCommandBuffer->callFinishedProcs(/*success=*/false);
        return nullptr;
    }

    return std::make_unique<DawnWorkSubmission>(std::move(fCurrentCommandBuffer), this);
}

#if GRAPHITE_TEST_UTILS
void DawnQueueManager::startCapture() {
}

void DawnQueueManager::stopCapture() {
}
#endif

} // namespace skgpu::graphite
