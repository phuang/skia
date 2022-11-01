/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_UniformManager_DEFINED
#define skgpu_UniformManager_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"
#include "include/private/SkColorData.h"
#include "include/private/SkTDArray.h"
#include "include/private/SkVx.h"
#include "src/core/SkSLTypeShared.h"
#include "src/core/SkUniform.h"

class SkM44;
struct SkPoint;
struct SkRect;
class SkUniformDataBlock;

namespace skgpu::graphite {

enum class CType : unsigned;

enum class Layout {
    kStd140,
    kStd430,
    kMetal, /** This is our own self-imposed layout we use for Metal. */
};

class UniformOffsetCalculator {
public:
    UniformOffsetCalculator(Layout layout, uint32_t startingOffset);

    size_t size() const { return fOffset; }

    // Calculates the correctly aligned offset to accommodate `count` instances of `type` and
    // advances the internal offset. Returns the correctly aligned start offset.
    //
    // After a call to this method, `size()` will return the offset to the end of `count` instances
    // of `type` (while the return value equals the aligned start offset). Subsequent calls will
    // calculate the new start offset starting at `size()`.
    size_t advanceOffset(SkSLType type, unsigned int count);

protected:
    SkSLType getUniformTypeForLayout(SkSLType type);

    using WriteUniformFn = uint32_t (*)(SkSLType type,
                                        CType ctype,
                                        void *dest,
                                        int n,
                                        const void *src);

    WriteUniformFn fWriteUniform;
    Layout fLayout;  // TODO: eventually 'fLayout' will not need to be stored
    uint32_t fOffset = 0;
};

class UniformManager : public UniformOffsetCalculator {
public:
    UniformManager(Layout layout) : UniformOffsetCalculator(layout, /*startingOffset=*/0) {}

    SkUniformDataBlock finishUniformDataBlock();
    size_t size() const { return fStorage.size(); }

    void reset();

    // TODO: do we need to add a 'makeArray' parameter to these?
    void write(const SkM44&);
    void write(const SkColor4f*, int count);
    void write(const SkPMColor4f*, int count);
    void write(const SkPMColor4f& color) { this->write(&color, SkUniform::kNonArray); }
    void write(const SkRect&);
    void write(SkPoint);
    void write(const float*, int count);
    void write(float f) { this->write(&f, SkUniform::kNonArray); }
    void write(int);
    void write(skvx::float2);
    void write(skvx::float4);
    void write(SkSLType type, unsigned int count, const void* src);

    // Debug only utilities used for debug assertions and tests.
    void checkReset() const;
    void setExpectedUniforms(SkSpan<const SkUniform>);
    void checkExpected(SkSLType, unsigned int count);
    void doneWithExpectedUniforms();

private:
#ifdef SK_DEBUG
    SkSpan<const SkUniform> fExpectedUniforms;
    int fExpectedUniformIndex = 0;
#endif // SK_DEBUG

    SkTDArray<char> fStorage;
    uint32_t fReqAlignment = 0;
};

} // namespace skgpu

#endif // skgpu_UniformManager_DEFINED
