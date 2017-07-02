// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_

#include "core/fxcodec/codec/icodec_gifmodule.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CCodec_GifModule : public ICodec_GifModule {
 public:
  CCodec_GifModule();
  ~CCodec_GifModule() override;

  FXGIF_Context* Start() override;
  void Finish(FXGIF_Context* pContext) override;
  uint32_t GetAvailInput(FXGIF_Context* pContext,
                         uint8_t** avail_buf_ptr = nullptr) override;

  void Input(FXGIF_Context* pContext,
             const uint8_t* src_buf,
             uint32_t src_size) override;

  int32_t ReadHeader(FXGIF_Context* pContext,
                     int* width,
                     int* height,
                     int* pal_num,
                     void** pal_pp,
                     int* bg_index,
                     CFX_DIBAttribute* pAttribute) override;

  int32_t LoadFrameInfo(FXGIF_Context* pContext, int* frame_num) override;
  int32_t LoadFrame(FXGIF_Context* pContext,
                    int frame_num,
                    CFX_DIBAttribute* pAttribute) override;

 protected:
  FX_CHAR m_szLastError[256];
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_GIFMODULE_H_
