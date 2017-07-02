// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_HUFFMANTABLE_STANDARD_H_
#define CORE_FXCODEC_JBIG2_JBIG2_HUFFMANTABLE_STANDARD_H_

#include "core/fxcrt/fx_system.h"

struct JBig2TableLine {
  uint8_t PREFLEN;
  uint8_t RANDELEN;
  int32_t RANGELOW;
};

extern const bool HuffmanTable_HTOOB_B1;
extern const JBig2TableLine HuffmanTable_B1[];
extern const size_t HuffmanTable_B1_Size;

extern const bool HuffmanTable_HTOOB_B2;
extern const JBig2TableLine HuffmanTable_B2[];
extern const size_t HuffmanTable_B2_Size;

extern const bool HuffmanTable_HTOOB_B3;
extern const JBig2TableLine HuffmanTable_B3[];
extern const size_t HuffmanTable_B3_Size;

extern const bool HuffmanTable_HTOOB_B4;
extern const JBig2TableLine HuffmanTable_B4[];
extern const size_t HuffmanTable_B4_Size;

extern const bool HuffmanTable_HTOOB_B5;
extern const JBig2TableLine HuffmanTable_B5[];
extern const size_t HuffmanTable_B5_Size;

extern const bool HuffmanTable_HTOOB_B6;
extern const JBig2TableLine HuffmanTable_B6[];
extern const size_t HuffmanTable_B6_Size;

extern const bool HuffmanTable_HTOOB_B7;
extern const JBig2TableLine HuffmanTable_B7[];
extern const size_t HuffmanTable_B7_Size;

extern const bool HuffmanTable_HTOOB_B8;
extern const JBig2TableLine HuffmanTable_B8[];
extern const size_t HuffmanTable_B8_Size;

extern const bool HuffmanTable_HTOOB_B9;
extern const JBig2TableLine HuffmanTable_B9[];
extern const size_t HuffmanTable_B9_Size;

extern const bool HuffmanTable_HTOOB_B10;
extern const JBig2TableLine HuffmanTable_B10[];
extern const size_t HuffmanTable_B10_Size;

extern const bool HuffmanTable_HTOOB_B11;
extern const JBig2TableLine HuffmanTable_B11[];
extern const size_t HuffmanTable_B11_Size;

extern const bool HuffmanTable_HTOOB_B12;
extern const JBig2TableLine HuffmanTable_B12[];
extern const size_t HuffmanTable_B12_Size;

extern const bool HuffmanTable_HTOOB_B13;
extern const JBig2TableLine HuffmanTable_B13[];
extern const size_t HuffmanTable_B13_Size;

extern const bool HuffmanTable_HTOOB_B14;
extern const JBig2TableLine HuffmanTable_B14[];
extern const size_t HuffmanTable_B14_Size;

extern const bool HuffmanTable_HTOOB_B15;
extern const JBig2TableLine HuffmanTable_B15[];
extern const size_t HuffmanTable_B15_Size;

#endif  // CORE_FXCODEC_JBIG2_JBIG2_HUFFMANTABLE_STANDARD_H_
