// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_PNGMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_PNGMODULE_H_

#include "core/fxcodec/codec/icodec_pngmodule.h"
#include "core/fxcrt/fx_system.h"

#define PNG_ERROR_SIZE 256

class CCodec_PngModule : public ICodec_PngModule {
 public:
  CCodec_PngModule();
  ~CCodec_PngModule() override;

  FXPNG_Context* Start() override;
  void Finish(FXPNG_Context* pContext) override;
  bool Input(FXPNG_Context* pContext,
             const uint8_t* src_buf,
             uint32_t src_size,
             CFX_DIBAttribute* pAttribute) override;

 protected:
  FX_CHAR m_szLastError[PNG_ERROR_SIZE];
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_PNGMODULE_H_
