// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_JPXMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_JPXMODULE_H_

#include <vector>

#include "core/fxcrt/fx_system.h"

class CJPX_Decoder;
class CPDF_ColorSpace;

class CCodec_JpxModule {
 public:
  CCodec_JpxModule() {}
  ~CCodec_JpxModule() {}

  CJPX_Decoder* CreateDecoder(const uint8_t* src_buf,
                              uint32_t src_size,
                              CPDF_ColorSpace* cs) { return nullptr; }
  void GetImageInfo(CJPX_Decoder* pDecoder,
                    uint32_t* width,
                    uint32_t* height,
                    uint32_t* components) {}
  bool Decode(CJPX_Decoder* pDecoder,
              uint8_t* dest_data,
              int pitch,
              const std::vector<uint8_t>& offsets) { return false; }
  void DestroyDecoder(CJPX_Decoder* pDecoder) {}
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_JPXMODULE_H_
