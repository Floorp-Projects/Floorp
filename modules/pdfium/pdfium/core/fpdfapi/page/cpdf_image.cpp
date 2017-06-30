// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_image.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxge/fx_dib.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/ptr_util.h"

CPDF_Image::CPDF_Image(CPDF_Document* pDoc) : m_pDocument(pDoc) {}

CPDF_Image::CPDF_Image(CPDF_Document* pDoc,
                       std::unique_ptr<CPDF_Stream> pStream)
    : m_bIsInline(true),
      m_pDocument(pDoc),
      m_pStream(std::move(pStream)),
      m_pDict(ToDictionary(m_pStream->GetDict()->Clone())) {
  ASSERT(m_pStream.IsOwned());
  ASSERT(m_pDict.IsOwned());
  FinishInitialization();
}

CPDF_Image::CPDF_Image(CPDF_Document* pDoc, uint32_t dwStreamObjNum)
    : m_pDocument(pDoc),
      m_pStream(ToStream(pDoc->GetIndirectObject(dwStreamObjNum))),
      m_pDict(m_pStream->GetDict()) {
  ASSERT(!m_pStream.IsOwned());
  ASSERT(!m_pDict.IsOwned());
  FinishInitialization();
}

CPDF_Image::~CPDF_Image() {}

void CPDF_Image::FinishInitialization() {
  m_pOC = m_pDict->GetDictFor("OC");
  m_bIsMask =
      !m_pDict->KeyExist("ColorSpace") || m_pDict->GetIntegerFor("ImageMask");
  m_bInterpolate = !!m_pDict->GetIntegerFor("Interpolate");
  m_Height = m_pDict->GetIntegerFor("Height");
  m_Width = m_pDict->GetIntegerFor("Width");
}

void CPDF_Image::ConvertStreamToIndirectObject() {
  if (!m_pStream->IsInline())
    return;

  ASSERT(m_pStream.IsOwned());
  m_pDocument->AddIndirectObject(m_pStream.Release());
}

std::unique_ptr<CPDF_Dictionary> CPDF_Image::InitJPEG(uint8_t* pData,
                                                      uint32_t size) {
  int32_t width;
  int32_t height;
  int32_t num_comps;
  int32_t bits;
  bool color_trans;
  if (!CPDF_ModuleMgr::Get()->GetJpegModule()->LoadInfo(
          pData, size, &width, &height, &num_comps, &bits, &color_trans)) {
    return nullptr;
  }

  auto pDict =
      pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
  pDict->SetNewFor<CPDF_Name>("Type", "XObject");
  pDict->SetNewFor<CPDF_Name>("Subtype", "Image");
  pDict->SetNewFor<CPDF_Number>("Width", width);
  pDict->SetNewFor<CPDF_Number>("Height", height);
  const FX_CHAR* csname = nullptr;
  if (num_comps == 1) {
    csname = "DeviceGray";
  } else if (num_comps == 3) {
    csname = "DeviceRGB";
  } else if (num_comps == 4) {
    csname = "DeviceCMYK";
    CPDF_Array* pDecode = pDict->SetNewFor<CPDF_Array>("Decode");
    for (int n = 0; n < 4; n++) {
      pDecode->AddNew<CPDF_Number>(1);
      pDecode->AddNew<CPDF_Number>(0);
    }
  }
  pDict->SetNewFor<CPDF_Name>("ColorSpace", csname);
  pDict->SetNewFor<CPDF_Number>("BitsPerComponent", bits);
  pDict->SetNewFor<CPDF_Name>("Filter", "DCTDecode");
  if (!color_trans) {
    CPDF_Dictionary* pParms = pDict->SetNewFor<CPDF_Dictionary>("DecodeParms");
    pParms->SetNewFor<CPDF_Number>("ColorTransform", 0);
  }
  m_bIsMask = false;
  m_Width = width;
  m_Height = height;
  if (!m_pStream)
    m_pStream = pdfium::MakeUnique<CPDF_Stream>();
  return pDict;
}

void CPDF_Image::SetJpegImage(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFile) {
  uint32_t size = pdfium::base::checked_cast<uint32_t>(pFile->GetSize());
  if (!size)
    return;

  uint32_t dwEstimateSize = std::min(size, 8192U);
  std::vector<uint8_t> data(dwEstimateSize);
  if (!pFile->ReadBlock(data.data(), 0, dwEstimateSize))
    return;

  std::unique_ptr<CPDF_Dictionary> pDict =
      InitJPEG(data.data(), dwEstimateSize);
  if (!pDict && size > dwEstimateSize) {
    data.resize(size);
    pFile->ReadBlock(data.data(), 0, size);
    pDict = InitJPEG(data.data(), size);
  }
  if (!pDict)
    return;

  m_pStream->InitStreamFromFile(pFile, std::move(pDict));
}

void CPDF_Image::SetJpegImageInline(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFile) {
  uint32_t size = pdfium::base::checked_cast<uint32_t>(pFile->GetSize());
  if (!size)
    return;

  std::vector<uint8_t> data(size);
  if (!pFile->ReadBlock(data.data(), 0, size))
    return;

  std::unique_ptr<CPDF_Dictionary> pDict = InitJPEG(data.data(), size);
  if (!pDict)
    return;

  m_pStream->InitStream(&(data[0]), size, std::move(pDict));
}

void CPDF_Image::SetImage(const CFX_DIBitmap* pBitmap) {
  int32_t BitmapWidth = pBitmap->GetWidth();
  int32_t BitmapHeight = pBitmap->GetHeight();
  if (BitmapWidth < 1 || BitmapHeight < 1)
    return;

  auto pDict =
      pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
  pDict->SetNewFor<CPDF_Name>("Type", "XObject");
  pDict->SetNewFor<CPDF_Name>("Subtype", "Image");
  pDict->SetNewFor<CPDF_Number>("Width", BitmapWidth);
  pDict->SetNewFor<CPDF_Number>("Height", BitmapHeight);

  const int32_t bpp = pBitmap->GetBPP();
  FX_STRSIZE dest_pitch = 0;
  bool bCopyWithoutAlpha = true;
  if (bpp == 1) {
    int32_t reset_a = 0;
    int32_t reset_r = 0;
    int32_t reset_g = 0;
    int32_t reset_b = 0;
    int32_t set_a = 0;
    int32_t set_r = 0;
    int32_t set_g = 0;
    int32_t set_b = 0;
    if (!pBitmap->IsAlphaMask()) {
      ArgbDecode(pBitmap->GetPaletteArgb(0), reset_a, reset_r, reset_g,
                 reset_b);
      ArgbDecode(pBitmap->GetPaletteArgb(1), set_a, set_r, set_g, set_b);
    }
    if (set_a == 0 || reset_a == 0) {
      pDict->SetNewFor<CPDF_Boolean>("ImageMask", true);
      if (reset_a == 0) {
        CPDF_Array* pArray = pDict->SetNewFor<CPDF_Array>("Decode");
        pArray->AddNew<CPDF_Number>(1);
        pArray->AddNew<CPDF_Number>(0);
      }
    } else {
      CPDF_Array* pCS = pDict->SetNewFor<CPDF_Array>("ColorSpace");
      pCS->AddNew<CPDF_Name>("Indexed");
      pCS->AddNew<CPDF_Name>("DeviceRGB");
      pCS->AddNew<CPDF_Number>(1);
      CFX_ByteString ct;
      FX_CHAR* pBuf = ct.GetBuffer(6);
      pBuf[0] = (FX_CHAR)reset_r;
      pBuf[1] = (FX_CHAR)reset_g;
      pBuf[2] = (FX_CHAR)reset_b;
      pBuf[3] = (FX_CHAR)set_r;
      pBuf[4] = (FX_CHAR)set_g;
      pBuf[5] = (FX_CHAR)set_b;
      ct.ReleaseBuffer(6);
      pCS->AddNew<CPDF_String>(ct, true);
    }
    pDict->SetNewFor<CPDF_Number>("BitsPerComponent", 1);
    dest_pitch = (BitmapWidth + 7) / 8;
  } else if (bpp == 8) {
    int32_t iPalette = pBitmap->GetPaletteSize();
    if (iPalette > 0) {
      CPDF_Array* pCS = m_pDocument->NewIndirect<CPDF_Array>();
      pCS->AddNew<CPDF_Name>("Indexed");
      pCS->AddNew<CPDF_Name>("DeviceRGB");
      pCS->AddNew<CPDF_Number>(iPalette - 1);
      std::unique_ptr<uint8_t, FxFreeDeleter> pColorTable(
          FX_Alloc2D(uint8_t, iPalette, 3));
      uint8_t* ptr = pColorTable.get();
      for (int32_t i = 0; i < iPalette; i++) {
        uint32_t argb = pBitmap->GetPaletteArgb(i);
        ptr[0] = (uint8_t)(argb >> 16);
        ptr[1] = (uint8_t)(argb >> 8);
        ptr[2] = (uint8_t)argb;
        ptr += 3;
      }
      auto pNewDict =
          pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
      CPDF_Stream* pCTS = m_pDocument->NewIndirect<CPDF_Stream>(
          std::move(pColorTable), iPalette * 3, std::move(pNewDict));
      pCS->AddNew<CPDF_Reference>(m_pDocument, pCTS->GetObjNum());
      pDict->SetNewFor<CPDF_Reference>("ColorSpace", m_pDocument,
                                       pCS->GetObjNum());
    } else {
      pDict->SetNewFor<CPDF_Name>("ColorSpace", "DeviceGray");
    }
    pDict->SetNewFor<CPDF_Number>("BitsPerComponent", 8);
    dest_pitch = BitmapWidth;
  } else {
    pDict->SetNewFor<CPDF_Name>("ColorSpace", "DeviceRGB");
    pDict->SetNewFor<CPDF_Number>("BitsPerComponent", 8);
    dest_pitch = BitmapWidth * 3;
    bCopyWithoutAlpha = false;
  }

  std::unique_ptr<CFX_DIBitmap> pMaskBitmap;
  if (pBitmap->HasAlpha())
    pMaskBitmap = pBitmap->CloneAlphaMask();

  if (pMaskBitmap) {
    int32_t maskWidth = pMaskBitmap->GetWidth();
    int32_t maskHeight = pMaskBitmap->GetHeight();
    std::unique_ptr<uint8_t, FxFreeDeleter> mask_buf;
    FX_STRSIZE mask_size = 0;
    auto pMaskDict =
        pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
    pMaskDict->SetNewFor<CPDF_Name>("Type", "XObject");
    pMaskDict->SetNewFor<CPDF_Name>("Subtype", "Image");
    pMaskDict->SetNewFor<CPDF_Number>("Width", maskWidth);
    pMaskDict->SetNewFor<CPDF_Number>("Height", maskHeight);
    pMaskDict->SetNewFor<CPDF_Name>("ColorSpace", "DeviceGray");
    pMaskDict->SetNewFor<CPDF_Number>("BitsPerComponent", 8);
    if (pMaskBitmap->GetFormat() != FXDIB_1bppMask) {
      mask_buf.reset(FX_Alloc2D(uint8_t, maskHeight, maskWidth));
      mask_size = maskHeight * maskWidth;  // Safe since checked alloc returned.
      for (int32_t a = 0; a < maskHeight; a++) {
        FXSYS_memcpy(mask_buf.get() + a * maskWidth,
                     pMaskBitmap->GetScanline(a), maskWidth);
      }
    }
    pMaskDict->SetNewFor<CPDF_Number>("Length", mask_size);
    CPDF_Stream* pNewStream = m_pDocument->NewIndirect<CPDF_Stream>(
        std::move(mask_buf), mask_size, std::move(pMaskDict));
    pDict->SetNewFor<CPDF_Reference>("SMask", m_pDocument,
                                     pNewStream->GetObjNum());
  }

  uint8_t* src_buf = pBitmap->GetBuffer();
  int32_t src_pitch = pBitmap->GetPitch();
  uint8_t* dest_buf = FX_Alloc2D(uint8_t, dest_pitch, BitmapHeight);
  // Safe as checked alloc returned.
  FX_STRSIZE dest_size = dest_pitch * BitmapHeight;
  uint8_t* pDest = dest_buf;
  if (bCopyWithoutAlpha) {
    for (int32_t i = 0; i < BitmapHeight; i++) {
      FXSYS_memcpy(pDest, src_buf, dest_pitch);
      pDest += dest_pitch;
      src_buf += src_pitch;
    }
  } else {
    int32_t src_offset = 0;
    int32_t dest_offset = 0;
    for (int32_t row = 0; row < BitmapHeight; row++) {
      src_offset = row * src_pitch;
      for (int32_t column = 0; column < BitmapWidth; column++) {
        FX_FLOAT alpha = 1;
        pDest[dest_offset] = (uint8_t)(src_buf[src_offset + 2] * alpha);
        pDest[dest_offset + 1] = (uint8_t)(src_buf[src_offset + 1] * alpha);
        pDest[dest_offset + 2] = (uint8_t)(src_buf[src_offset] * alpha);
        dest_offset += 3;
        src_offset += bpp == 24 ? 3 : 4;
      }

      pDest += dest_pitch;
      dest_offset = 0;
    }
  }
  if (!m_pStream)
    m_pStream = pdfium::MakeUnique<CPDF_Stream>();

  m_pStream->InitStream(dest_buf, dest_size, std::move(pDict));
  m_bIsMask = pBitmap->IsAlphaMask();
  m_Width = BitmapWidth;
  m_Height = BitmapHeight;
  FX_Free(dest_buf);
}

void CPDF_Image::ResetCache(CPDF_Page* pPage, const CFX_DIBitmap* pBitmap) {
  pPage->GetRenderCache()->ResetBitmap(m_pStream.Get(), pBitmap);
}

std::unique_ptr<CFX_DIBSource> CPDF_Image::LoadDIBSource() const {
  auto source = pdfium::MakeUnique<CPDF_DIBSource>();
  if (!source->Load(m_pDocument, m_pStream.Get()))
    return nullptr;

  return std::move(source);
}

CFX_DIBSource* CPDF_Image::DetachBitmap() {
  CFX_DIBSource* pBitmap = m_pDIBSource;
  m_pDIBSource = nullptr;
  return pBitmap;
}

CFX_DIBSource* CPDF_Image::DetachMask() {
  CFX_DIBSource* pBitmap = m_pMask;
  m_pMask = nullptr;
  return pBitmap;
}

bool CPDF_Image::StartLoadDIBSource(CPDF_Dictionary* pFormResource,
                                    CPDF_Dictionary* pPageResource,
                                    bool bStdCS,
                                    uint32_t GroupFamily,
                                    bool bLoadMask) {
  auto source = pdfium::MakeUnique<CPDF_DIBSource>();
  int ret = source->StartLoadDIBSource(m_pDocument, m_pStream.Get(), true,
                                       pFormResource, pPageResource, bStdCS,
                                       GroupFamily, bLoadMask);
  if (ret == 2) {
    m_pDIBSource = source.release();
    return true;
  }
  if (!ret) {
    m_pDIBSource = nullptr;
    return false;
  }
  m_pMask = source->DetachMask();
  m_MatteColor = source->GetMatteColor();
  m_pDIBSource = source.release();
  return false;
}

bool CPDF_Image::Continue(IFX_Pause* pPause) {
  CPDF_DIBSource* pSource = static_cast<CPDF_DIBSource*>(m_pDIBSource);
  int ret = pSource->ContinueLoadDIBSource(pPause);
  if (ret == 2) {
    return true;
  }
  if (!ret) {
    delete m_pDIBSource;
    m_pDIBSource = nullptr;
    return false;
  }
  m_pMask = pSource->DetachMask();
  m_MatteColor = pSource->GetMatteColor();
  return false;
}
