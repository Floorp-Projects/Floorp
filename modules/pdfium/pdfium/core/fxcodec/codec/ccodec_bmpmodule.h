// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_BMPMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_BMPMODULE_H_

#include "core/fxcodec/codec/icodec_bmpmodule.h"
#include "core/fxcrt/fx_system.h"

class CCodec_BmpModule : public ICodec_BmpModule {
 public:
  CCodec_BmpModule();
  ~CCodec_BmpModule() override;

  FXBMP_Context* Start() override;
  void Finish(FXBMP_Context* pContext) override;
  uint32_t GetAvailInput(FXBMP_Context* pContext,
                         uint8_t** avail_buf_ptr) override;
  void Input(FXBMP_Context* pContext,
             const uint8_t* src_buf,
             uint32_t src_size) override;
  int32_t ReadHeader(FXBMP_Context* pContext,
                     int32_t* width,
                     int32_t* height,
                     bool* tb_flag,
                     int32_t* components,
                     int32_t* pal_num,
                     uint32_t** pal_pp,
                     CFX_DIBAttribute* pAttribute) override;
  int32_t LoadImage(FXBMP_Context* pContext) override;

 protected:
  FX_CHAR m_szLastError[256];
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_BMPMODULE_H_
