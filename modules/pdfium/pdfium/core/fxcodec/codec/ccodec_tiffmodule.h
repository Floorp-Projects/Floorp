// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_TIFFMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_TIFFMODULE_H_

#include "core/fxcodec/codec/icodec_tiffmodule.h"
#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_system.h"

class CCodec_TiffModule : public ICodec_TiffModule {
 public:
  ~CCodec_TiffModule() override {}

  CCodec_TiffContext* CreateDecoder(
      const CFX_RetainPtr<IFX_SeekableReadStream>& file_ptr) override;
  bool LoadFrameInfo(CCodec_TiffContext* ctx,
                     int32_t frame,
                     int32_t* width,
                     int32_t* height,
                     int32_t* comps,
                     int32_t* bpc,
                     CFX_DIBAttribute* pAttribute) override;
  bool Decode(CCodec_TiffContext* ctx, class CFX_DIBitmap* pDIBitmap) override;
  void DestroyDecoder(CCodec_TiffContext* ctx) override;
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_TIFFMODULE_H_
