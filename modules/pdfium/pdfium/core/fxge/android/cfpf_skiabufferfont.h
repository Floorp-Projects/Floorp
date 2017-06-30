// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIABUFFERFONT_H_
#define CORE_FXGE_ANDROID_CFPF_SKIABUFFERFONT_H_

#include "core/fxge/android/cfpf_skiafontdescriptor.h"

#define FPF_SKIAFONTTYPE_Buffer 3

class CFPF_SkiaBufferFont : public CFPF_SkiaFontDescriptor {
 public:
  CFPF_SkiaBufferFont() : m_pBuffer(nullptr), m_szBuffer(0) {}

  // CFPF_SkiaFontDescriptor
  int32_t GetType() const override { return FPF_SKIAFONTTYPE_Buffer; }

  void* m_pBuffer;
  size_t m_szBuffer;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIABUFFERFONT_H_
