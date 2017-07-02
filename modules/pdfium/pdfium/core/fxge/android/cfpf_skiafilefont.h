// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIAFILEFONT_H_
#define CORE_FXGE_ANDROID_CFPF_SKIAFILEFONT_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxge/android/cfpf_skiafontdescriptor.h"

class IFX_SeekableReadStream;

#define FPF_SKIAFONTTYPE_File 2

class CFPF_SkiaFileFont : public CFPF_SkiaFontDescriptor {
 public:
  CFPF_SkiaFileFont() {}

  // CFPF_SkiaFontDescriptor
  int32_t GetType() const override { return FPF_SKIAFONTTYPE_File; }

  CFX_RetainPtr<IFX_SeekableReadStream> m_pFile;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIAFILEFONT_H_
