// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_ICCMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_ICCMODULE_H_

#include "core/fxcodec/fx_codec_def.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CCodec_IccModule {
 public:
  CCodec_IccModule();
  ~CCodec_IccModule();

  void* CreateTransform_sRGB(const uint8_t* pProfileData,
                             uint32_t dwProfileSize,
                             uint32_t& nComponents,
                             int32_t intent = 0,
                             uint32_t dwSrcFormat = Icc_FORMAT_DEFAULT);
  void DestroyTransform(void* pTransform);
  void Translate(void* pTransform, FX_FLOAT* pSrcValues, FX_FLOAT* pDestValues);
  void TranslateScanline(void* pTransform,
                         uint8_t* pDest,
                         const uint8_t* pSrc,
                         int pixels);
  void SetComponents(uint32_t nComponents) { m_nComponents = nComponents; }

 protected:
  uint32_t m_nComponents;
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_ICCMODULE_H_
