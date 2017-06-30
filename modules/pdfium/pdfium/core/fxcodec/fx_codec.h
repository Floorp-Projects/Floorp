// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_FX_CODEC_H_
#define CORE_FXCODEC_FX_CODEC_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "core/fxcodec/codec/ccodec_basicmodule.h"
#include "core/fxcodec/codec/ccodec_faxmodule.h"
#include "core/fxcodec/codec/ccodec_flatemodule.h"
#include "core/fxcodec/codec/ccodec_iccmodule.h"
#include "core/fxcodec/codec/ccodec_jbig2module.h"
#include "core/fxcodec/codec/ccodec_jpegmodule.h"
#include "core/fxcodec/codec/ccodec_jpxmodule.h"
#include "core/fxcodec/codec/ccodec_scanlinedecoder.h"
#include "core/fxcodec/fx_codec_def.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"

#ifdef PDF_ENABLE_XFA
#include "core/fxcodec/codec/icodec_bmpmodule.h"
#include "core/fxcodec/codec/icodec_gifmodule.h"
#include "core/fxcodec/codec/icodec_pngmodule.h"
#include "core/fxcodec/codec/icodec_tiffmodule.h"
#endif  // PDF_ENABLE_XFA

class CFX_DIBSource;
class CJPX_Decoder;
class CPDF_ColorSpace;
class CPDF_StreamAcc;

#ifdef PDF_ENABLE_XFA
class CCodec_ProgressiveDecoder;

class CFX_DIBAttribute {
 public:
  CFX_DIBAttribute();
  ~CFX_DIBAttribute();

  int32_t m_nXDPI;
  int32_t m_nYDPI;
  FX_FLOAT m_fAspectRatio;
  uint16_t m_wDPIUnit;
  CFX_ByteString m_strAuthor;
  uint8_t m_strTime[20];
  int32_t m_nGifLeft;
  int32_t m_nGifTop;
  uint32_t* m_pGifLocalPalette;
  uint32_t m_nGifLocalPalNum;
  int32_t m_nBmpCompressType;
  std::map<uint32_t, void*> m_Exif;
};
#endif  // PDF_ENABLE_XFA

class CCodec_ModuleMgr {
 public:
  CCodec_ModuleMgr();
  ~CCodec_ModuleMgr();

  CCodec_BasicModule* GetBasicModule() const { return m_pBasicModule.get(); }
  CCodec_FaxModule* GetFaxModule() const { return m_pFaxModule.get(); }
  CCodec_JpegModule* GetJpegModule() const { return m_pJpegModule.get(); }
  CCodec_JpxModule* GetJpxModule() const { return m_pJpxModule.get(); }
  CCodec_Jbig2Module* GetJbig2Module() const { return m_pJbig2Module.get(); }
  CCodec_IccModule* GetIccModule() const { return m_pIccModule.get(); }
  CCodec_FlateModule* GetFlateModule() const { return m_pFlateModule.get(); }

#ifdef PDF_ENABLE_XFA
  std::unique_ptr<CCodec_ProgressiveDecoder> CreateProgressiveDecoder();
  void SetBmpModule(std::unique_ptr<ICodec_BmpModule> module) {
    m_pBmpModule = std::move(module);
  }
  void SetGifModule(std::unique_ptr<ICodec_GifModule> module) {
    m_pGifModule = std::move(module);
  }
  void SetPngModule(std::unique_ptr<ICodec_PngModule> module) {
    m_pPngModule = std::move(module);
  }
  void SetTiffModule(std::unique_ptr<ICodec_TiffModule> module) {
    m_pTiffModule = std::move(module);
  }
  ICodec_BmpModule* GetBmpModule() const { return m_pBmpModule.get(); }
  ICodec_GifModule* GetGifModule() const { return m_pGifModule.get(); }
  ICodec_PngModule* GetPngModule() const { return m_pPngModule.get(); }
  ICodec_TiffModule* GetTiffModule() const { return m_pTiffModule.get(); }
#endif  // PDF_ENABLE_XFA

 protected:
  std::unique_ptr<CCodec_BasicModule> m_pBasicModule;
  std::unique_ptr<CCodec_FaxModule> m_pFaxModule;
  std::unique_ptr<CCodec_JpegModule> m_pJpegModule;
  std::unique_ptr<CCodec_JpxModule> m_pJpxModule;
  std::unique_ptr<CCodec_Jbig2Module> m_pJbig2Module;
  std::unique_ptr<CCodec_IccModule> m_pIccModule;

#ifdef PDF_ENABLE_XFA
  std::unique_ptr<ICodec_BmpModule> m_pBmpModule;
  std::unique_ptr<ICodec_GifModule> m_pGifModule;
  std::unique_ptr<ICodec_PngModule> m_pPngModule;
  std::unique_ptr<ICodec_TiffModule> m_pTiffModule;
#endif  // PDF_ENABLE_XFA

  std::unique_ptr<CCodec_FlateModule> m_pFlateModule;
};

void ReverseRGB(uint8_t* pDestBuf, const uint8_t* pSrcBuf, int pixels);
uint32_t ComponentsForFamily(int family);
void sRGB_to_AdobeCMYK(FX_FLOAT R,
                       FX_FLOAT G,
                       FX_FLOAT B,
                       FX_FLOAT& c,
                       FX_FLOAT& m,
                       FX_FLOAT& y,
                       FX_FLOAT& k);
void AdobeCMYK_to_sRGB(FX_FLOAT c,
                       FX_FLOAT m,
                       FX_FLOAT y,
                       FX_FLOAT k,
                       FX_FLOAT& R,
                       FX_FLOAT& G,
                       FX_FLOAT& B);
void AdobeCMYK_to_sRGB1(uint8_t c,
                        uint8_t m,
                        uint8_t y,
                        uint8_t k,
                        uint8_t& R,
                        uint8_t& G,
                        uint8_t& B);
bool MD5ComputeID(const void* buf, uint32_t dwSize, uint8_t ID[16]);
void FaxG4Decode(const uint8_t* src_buf,
                 uint32_t src_size,
                 int* pbitpos,
                 uint8_t* dest_buf,
                 int width,
                 int height,
                 int pitch);

#endif  // CORE_FXCODEC_FX_CODEC_H_
