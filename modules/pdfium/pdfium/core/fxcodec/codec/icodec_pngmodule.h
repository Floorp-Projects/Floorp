// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_ICODEC_PNGMODULE_H_
#define CORE_FXCODEC_CODEC_ICODEC_PNGMODULE_H_

#include "core/fxcrt/fx_system.h"

class CFX_DIBAttribute;
struct FXPNG_Context;

// Virtual interface to avoid linking in a concrete implementation
// if we do not enable this codec.
class ICodec_PngModule {
 public:
  class Delegate {
   public:
    virtual bool PngReadHeader(int width,
                               int height,
                               int bpc,
                               int pass,
                               int* color_type,
                               double* gamma) = 0;
    virtual bool PngAskScanlineBuf(int line, uint8_t*& src_buf) = 0;
    virtual void PngFillScanlineBufCompleted(int pass, int line) = 0;
  };

  virtual ~ICodec_PngModule() {}

  virtual FXPNG_Context* Start() = 0;
  virtual void Finish(FXPNG_Context* pContext) = 0;
  virtual bool Input(FXPNG_Context* pContext,
                     const uint8_t* src_buf,
                     uint32_t src_size,
                     CFX_DIBAttribute* pAttribute) = 0;

  Delegate* GetDelegate() const { return m_pDelegate; }
  void SetDelegate(Delegate* delegate) { m_pDelegate = delegate; }

 protected:
  Delegate* m_pDelegate;
};

#endif  // CORE_FXCODEC_CODEC_ICODEC_PNGMODULE_H_
