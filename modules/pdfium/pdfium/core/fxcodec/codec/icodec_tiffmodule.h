// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_ICODEC_TIFFMODULE_H_
#define CORE_FXCODEC_CODEC_ICODEC_TIFFMODULE_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_system.h"

class CCodec_TiffContext;
class CFX_DIBAttribute;
class CFX_DIBitmap;
class IFX_SeekableReadStream;

class ICodec_TiffModule {
 public:
  virtual ~ICodec_TiffModule() {}

  virtual CCodec_TiffContext* CreateDecoder(
      const CFX_RetainPtr<IFX_SeekableReadStream>& file_ptr) = 0;
  virtual bool LoadFrameInfo(CCodec_TiffContext* ctx,
                             int32_t frame,
                             int32_t* width,
                             int32_t* height,
                             int32_t* comps,
                             int32_t* bpc,
                             CFX_DIBAttribute* pAttribute) = 0;
  virtual bool Decode(CCodec_TiffContext* ctx,
                      class CFX_DIBitmap* pDIBitmap) = 0;
  virtual void DestroyDecoder(CCodec_TiffContext* ctx) = 0;
};

#endif  // CORE_FXCODEC_CODEC_ICODEC_TIFFMODULE_H_
