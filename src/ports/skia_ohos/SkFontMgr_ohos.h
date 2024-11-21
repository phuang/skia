/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * 2023.4.23 SkFontMgr on ohos.
 *           Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
 */

#include "include/core/SkRefCnt.h"

#ifndef SKFONTMGR_OHOS_H
#define SKFONTMGR_OHOS_H

class SkFontMgr;

SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS(const char* path);

#endif /* SKFONTMGR_OHOS_H */
