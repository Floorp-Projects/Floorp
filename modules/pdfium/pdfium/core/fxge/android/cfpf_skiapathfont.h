// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIAPATHFONT_H_
#define CORE_FXGE_ANDROID_CFPF_SKIAPATHFONT_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxge/android/cfpf_skiafontdescriptor.h"

#define FPF_SKIAFONTTYPE_Path 1

class CFPF_SkiaPathFont : public CFPF_SkiaFontDescriptor {
 public:
  CFPF_SkiaPathFont() : m_pPath(nullptr) {}
  ~CFPF_SkiaPathFont() override { FX_Free(m_pPath); }

  // CFPF_SkiaFontDescriptor
  int32_t GetType() const override { return FPF_SKIAFONTTYPE_Path; }

  void SetPath(const FX_CHAR* pPath) {
    FX_Free(m_pPath);
    int32_t iSize = FXSYS_strlen(pPath);
    m_pPath = FX_Alloc(FX_CHAR, iSize + 1);
    FXSYS_memcpy(m_pPath, pPath, iSize * sizeof(FX_CHAR));
    m_pPath[iSize] = 0;
  }
  FX_CHAR* m_pPath;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIAPATHFONT_H_
