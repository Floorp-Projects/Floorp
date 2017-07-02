// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIAFONTDESCRIPTOR_H_
#define CORE_FXGE_ANDROID_CFPF_SKIAFONTDESCRIPTOR_H_

#include "core/fxcrt/fx_system.h"

#define FPF_SKIAFONTTYPE_Unknown 0

class CFPF_SkiaFontDescriptor {
 public:
  CFPF_SkiaFontDescriptor()
      : m_pFamily(nullptr),
        m_dwStyle(0),
        m_iFaceIndex(0),
        m_dwCharsets(0),
        m_iGlyphNum(0) {}
  virtual ~CFPF_SkiaFontDescriptor() { FX_Free(m_pFamily); }

  virtual int32_t GetType() const { return FPF_SKIAFONTTYPE_Unknown; }

  void SetFamily(const FX_CHAR* pFamily) {
    FX_Free(m_pFamily);
    int32_t iSize = FXSYS_strlen(pFamily);
    m_pFamily = FX_Alloc(FX_CHAR, iSize + 1);
    FXSYS_memcpy(m_pFamily, pFamily, iSize * sizeof(FX_CHAR));
    m_pFamily[iSize] = 0;
  }
  FX_CHAR* m_pFamily;
  uint32_t m_dwStyle;
  int32_t m_iFaceIndex;
  uint32_t m_dwCharsets;
  int32_t m_iGlyphNum;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIAFONTDESCRIPTOR_H_
