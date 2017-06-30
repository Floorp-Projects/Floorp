// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_JPEGMODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_JPEGMODULE_H_

#include <memory>

#include "core/fxcrt/fx_system.h"

class CCodec_ScanlineDecoder;
class CFX_DIBSource;
struct FXJPEG_Context;

#ifdef PDF_ENABLE_XFA
class CFX_DIBAttribute;
#endif  // PDF_ENABLE_XFA

class CCodec_JpegModule {
 public:
  CCodec_JpegModule() {}

  std::unique_ptr<CCodec_ScanlineDecoder> CreateDecoder(const uint8_t* src_buf,
                                                        uint32_t src_size,
                                                        int width,
                                                        int height,
                                                        int nComps,
                                                        bool ColorTransform);
  bool LoadInfo(const uint8_t* src_buf,
                uint32_t src_size,
                int* width,
                int* height,
                int* num_components,
                int* bits_per_components,
                bool* color_transform);
  FXJPEG_Context* Start();
  void Finish(FXJPEG_Context* pContext);
  void Input(FXJPEG_Context* pContext,
             const uint8_t* src_buf,
             uint32_t src_size);

#ifndef PDF_ENABLE_XFA
  int ReadHeader(FXJPEG_Context* pContext,
                 int* width,
                 int* height,
                 int* nComps);
#else   // PDF_ENABLE_XFA
  int ReadHeader(FXJPEG_Context* pContext,
                 int* width,
                 int* height,
                 int* nComps,
                 CFX_DIBAttribute* pAttribute);
#endif  // PDF_ENABLE_XFA

  bool StartScanline(FXJPEG_Context* pContext, int down_scale);
  bool ReadScanline(FXJPEG_Context* pContext, uint8_t* dest_buf);
  uint32_t GetAvailInput(FXJPEG_Context* pContext, uint8_t** avail_buf_ptr);

#if _FX_OS_ == _FX_WIN32_DESKTOP_ || _FX_OS_ == _FX_WIN64_DESKTOP_
  static bool JpegEncode(const CFX_DIBSource* pSource,
                         uint8_t** dest_buf,
                         FX_STRSIZE* dest_size);
#endif
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_JPEGMODULE_H_
