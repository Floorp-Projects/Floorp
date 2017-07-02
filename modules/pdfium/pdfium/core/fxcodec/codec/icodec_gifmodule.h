// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_ICODEC_GIFMODULE_H_
#define CORE_FXCODEC_CODEC_ICODEC_GIFMODULE_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CFX_DIBAttribute;
struct FXGIF_Context;

// Virtual interface to avoid linking in a concrete implementation
// if we do not enable this codec.
class ICodec_GifModule {
 public:
  class Delegate {
   public:
    virtual void GifRecordCurrentPosition(uint32_t& cur_pos) = 0;
    virtual uint8_t* GifAskLocalPaletteBuf(int32_t frame_num,
                                           int32_t pal_size) = 0;
    virtual bool GifInputRecordPositionBuf(uint32_t rcd_pos,
                                           const FX_RECT& img_rc,
                                           int32_t pal_num,
                                           void* pal_ptr,
                                           int32_t delay_time,
                                           bool user_input,
                                           int32_t trans_index,
                                           int32_t disposal_method,
                                           bool interlace) = 0;
    virtual void GifReadScanline(int32_t row_num, uint8_t* row_buf) = 0;
  };

  virtual ~ICodec_GifModule() {}

  virtual FXGIF_Context* Start() = 0;
  virtual void Finish(FXGIF_Context* pContext) = 0;
  virtual uint32_t GetAvailInput(FXGIF_Context* pContext,
                                 uint8_t** avail_buf_ptr = nullptr) = 0;

  virtual void Input(FXGIF_Context* pContext,
                     const uint8_t* src_buf,
                     uint32_t src_size) = 0;

  virtual int32_t ReadHeader(FXGIF_Context* pContext,
                             int* width,
                             int* height,
                             int* pal_num,
                             void** pal_pp,
                             int* bg_index,
                             CFX_DIBAttribute* pAttribute) = 0;

  virtual int32_t LoadFrameInfo(FXGIF_Context* pContext, int* frame_num) = 0;
  virtual int32_t LoadFrame(FXGIF_Context* pContext,
                            int frame_num,
                            CFX_DIBAttribute* pAttribute) = 0;

  Delegate* GetDelegate() const { return m_pDelegate; }
  void SetDelegate(Delegate* pDelegate) { m_pDelegate = pDelegate; }

 protected:
  Delegate* m_pDelegate;
};

#endif  // CORE_FXCODEC_CODEC_ICODEC_GIFMODULE_H_
