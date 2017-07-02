// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_DEFINE_H_
#define CORE_FXCODEC_JBIG2_JBIG2_DEFINE_H_

#include "core/fxcrt/fx_system.h"

#define JBIG2_memset FXSYS_memset
#define JBIG2_memcmp FXSYS_memcmp
#define JBIG2_memcpy FXSYS_memcpy
#define JBIG2_OOB 1

struct JBig2RegionInfo {
  int32_t width;
  int32_t height;
  int32_t x;
  int32_t y;
  uint8_t flags;
};

struct JBig2HuffmanCode {
  int32_t codelen;
  int32_t code;
};

#define JBIG2_MAX_REFERRED_SEGMENT_COUNT 64
#define JBIG2_MAX_EXPORT_SYSMBOLS 65535
#define JBIG2_MAX_NEW_SYSMBOLS 65535
#define JBIG2_MAX_PATTERN_INDEX 65535
#define JBIG2_MAX_IMAGE_SIZE 65535

#endif  // CORE_FXCODEC_JBIG2_JBIG2_DEFINE_H_
