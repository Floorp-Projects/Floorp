// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_dibsource.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/ptr_util.h"

namespace {

unsigned int GetBits8(const uint8_t* pData, uint64_t bitpos, size_t nbits) {
  ASSERT(nbits == 1 || nbits == 2 || nbits == 4 || nbits == 8 || nbits == 16);
  ASSERT((bitpos & (nbits - 1)) == 0);
  unsigned int byte = pData[bitpos / 8];
  if (nbits == 8)
    return byte;

  if (nbits == 16)
    return byte * 256 + pData[bitpos / 8 + 1];

  return (byte >> (8 - nbits - (bitpos % 8))) & ((1 << nbits) - 1);
}

FX_SAFE_UINT32 CalculatePitch8(uint32_t bpc, uint32_t components, int width) {
  FX_SAFE_UINT32 pitch = bpc;
  pitch *= components;
  pitch *= width;
  pitch += 7;
  pitch /= 8;
  return pitch;
}

FX_SAFE_UINT32 CalculatePitch32(int bpp, int width) {
  FX_SAFE_UINT32 pitch = bpp;
  pitch *= width;
  pitch += 31;
  pitch /= 32;  // quantized to number of 32-bit words.
  pitch *= 4;   // and then back to bytes, (not just /8 in one step).
  return pitch;
}

bool IsAllowedBPCValue(int bpc) {
  return bpc == 1 || bpc == 2 || bpc == 4 || bpc == 8 || bpc == 16;
}

bool IsAllowedICCComponents(int nComp) {
  return nComp == 1 || nComp == 3 || nComp == 4;
}

template <typename T>
T ClampValue(T value, T max_value) {
  value = std::min(value, max_value);
  value = std::max<T>(0, value);
  return value;
}

// Wrapper class to use with std::unique_ptr for CJPX_Decoder.
class JpxBitMapContext {
 public:
  explicit JpxBitMapContext(CCodec_JpxModule* jpx_module)
      : jpx_module_(jpx_module), decoder_(nullptr) {}

  ~JpxBitMapContext() { jpx_module_->DestroyDecoder(decoder_); }

  // Takes ownership of |decoder|.
  void set_decoder(CJPX_Decoder* decoder) { decoder_ = decoder; }

  CJPX_Decoder* decoder() { return decoder_; }

 private:
  CCodec_JpxModule* const jpx_module_;  // Weak pointer.
  CJPX_Decoder* decoder_;               // Decoder, owned.

  // Disallow evil constructors
  JpxBitMapContext(const JpxBitMapContext&);
  void operator=(const JpxBitMapContext&);
};

const int kMaxImageDimension = 0x01FFFF;

}  // namespace

CPDF_DIBSource::CPDF_DIBSource()
    : m_pDocument(nullptr),
      m_pStream(nullptr),
      m_pDict(nullptr),
      m_pColorSpace(nullptr),
      m_Family(0),
      m_bpc(0),
      m_bpc_orig(0),
      m_nComponents(0),
      m_GroupFamily(0),
      m_MatteColor(0),
      m_bLoadMask(false),
      m_bDefaultDecode(true),
      m_bImageMask(false),
      m_bDoBpcCheck(true),
      m_bColorKey(false),
      m_bHasMask(false),
      m_bStdCS(false),
      m_pCompData(nullptr),
      m_pLineBuf(nullptr),
      m_pMaskedLine(nullptr),
      m_pMask(nullptr),
      m_pMaskStream(nullptr),
      m_Status(0) {}

CPDF_DIBSource::~CPDF_DIBSource() {
  FX_Free(m_pMaskedLine);
  FX_Free(m_pLineBuf);
  m_pCachedBitmap.reset();
  FX_Free(m_pCompData);
  CPDF_ColorSpace* pCS = m_pColorSpace;
  if (pCS && m_pDocument) {
    m_pDocument->GetPageData()->ReleaseColorSpace(pCS->GetArray());
  }
}

bool CPDF_DIBSource::Load(CPDF_Document* pDoc, const CPDF_Stream* pStream) {
  if (!pStream)
    return false;

  m_pDocument = pDoc;
  m_pDict = pStream->GetDict();
  if (!m_pDict)
    return false;

  m_pStream = pStream;
  m_Width = m_pDict->GetIntegerFor("Width");
  m_Height = m_pDict->GetIntegerFor("Height");
  if (m_Width <= 0 || m_Height <= 0 || m_Width > kMaxImageDimension ||
      m_Height > kMaxImageDimension) {
    return false;
  }
  m_GroupFamily = 0;
  m_bLoadMask = false;
  if (!LoadColorInfo(nullptr, nullptr))
    return false;

  if (m_bDoBpcCheck && (m_bpc == 0 || m_nComponents == 0))
    return false;

  FX_SAFE_UINT32 src_size =
      CalculatePitch8(m_bpc, m_nComponents, m_Width) * m_Height;
  if (!src_size.IsValid())
    return false;

  m_pStreamAcc = pdfium::MakeUnique<CPDF_StreamAcc>();
  m_pStreamAcc->LoadAllData(pStream, false, src_size.ValueOrDie(), true);
  if (m_pStreamAcc->GetSize() == 0 || !m_pStreamAcc->GetData())
    return false;

  if (!CreateDecoder())
    return false;

  if (m_bImageMask) {
    m_bpp = 1;
    m_bpc = 1;
    m_nComponents = 1;
    m_AlphaFlag = 1;
  } else if (m_bpc * m_nComponents == 1) {
    m_bpp = 1;
  } else if (m_bpc * m_nComponents <= 8) {
    m_bpp = 8;
  } else {
    m_bpp = 24;
  }
  FX_SAFE_UINT32 pitch = CalculatePitch32(m_bpp, m_Width);
  if (!pitch.IsValid())
    return false;

  m_pLineBuf = FX_Alloc(uint8_t, pitch.ValueOrDie());
  LoadPalette();
  if (m_bColorKey) {
    m_bpp = 32;
    m_AlphaFlag = 2;
    pitch = CalculatePitch32(m_bpp, m_Width);
    if (!pitch.IsValid())
      return false;

    m_pMaskedLine = FX_Alloc(uint8_t, pitch.ValueOrDie());
  }
  m_Pitch = pitch.ValueOrDie();
  return true;
}

int CPDF_DIBSource::ContinueToLoadMask() {
  if (m_bImageMask) {
    m_bpp = 1;
    m_bpc = 1;
    m_nComponents = 1;
    m_AlphaFlag = 1;
  } else if (m_bpc * m_nComponents == 1) {
    m_bpp = 1;
  } else if (m_bpc * m_nComponents <= 8) {
    m_bpp = 8;
  } else {
    m_bpp = 24;
  }
  if (!m_bpc || !m_nComponents) {
    return 0;
  }
  FX_SAFE_UINT32 pitch = CalculatePitch32(m_bpp, m_Width);
  if (!pitch.IsValid()) {
    return 0;
  }
  m_pLineBuf = FX_Alloc(uint8_t, pitch.ValueOrDie());
  if (m_pColorSpace && m_bStdCS) {
    m_pColorSpace->EnableStdConversion(true);
  }
  LoadPalette();
  if (m_bColorKey) {
    m_bpp = 32;
    m_AlphaFlag = 2;
    pitch = CalculatePitch32(m_bpp, m_Width);
    if (!pitch.IsValid()) {
      return 0;
    }
    m_pMaskedLine = FX_Alloc(uint8_t, pitch.ValueOrDie());
  }
  m_Pitch = pitch.ValueOrDie();
  return 1;
}

int CPDF_DIBSource::StartLoadDIBSource(CPDF_Document* pDoc,
                                       const CPDF_Stream* pStream,
                                       bool bHasMask,
                                       CPDF_Dictionary* pFormResources,
                                       CPDF_Dictionary* pPageResources,
                                       bool bStdCS,
                                       uint32_t GroupFamily,
                                       bool bLoadMask) {
  if (!pStream) {
    return 0;
  }
  m_pDocument = pDoc;
  m_pDict = pStream->GetDict();
  m_pStream = pStream;
  m_bStdCS = bStdCS;
  m_bHasMask = bHasMask;
  m_Width = m_pDict->GetIntegerFor("Width");
  m_Height = m_pDict->GetIntegerFor("Height");
  if (m_Width <= 0 || m_Height <= 0 || m_Width > kMaxImageDimension ||
      m_Height > kMaxImageDimension) {
    return 0;
  }
  m_GroupFamily = GroupFamily;
  m_bLoadMask = bLoadMask;
  if (!LoadColorInfo(m_pStream->IsInline() ? pFormResources : nullptr,
                     pPageResources)) {
    return 0;
  }
  if (m_bDoBpcCheck && (m_bpc == 0 || m_nComponents == 0)) {
    return 0;
  }
  FX_SAFE_UINT32 src_size =
      CalculatePitch8(m_bpc, m_nComponents, m_Width) * m_Height;
  if (!src_size.IsValid()) {
    return 0;
  }
  m_pStreamAcc = pdfium::MakeUnique<CPDF_StreamAcc>();
  m_pStreamAcc->LoadAllData(pStream, false, src_size.ValueOrDie(), true);
  if (m_pStreamAcc->GetSize() == 0 || !m_pStreamAcc->GetData()) {
    return 0;
  }
  int ret = CreateDecoder();
  if (!ret)
    return ret;

  if (ret != 1) {
    if (!ContinueToLoadMask()) {
      return 0;
    }
    if (m_bHasMask) {
      StratLoadMask();
    }
    return ret;
  }
  if (!ContinueToLoadMask()) {
    return 0;
  }
  if (m_bHasMask) {
    ret = StratLoadMask();
  }
  if (ret == 2) {
    return ret;
  }
  if (m_pColorSpace && m_bStdCS) {
    m_pColorSpace->EnableStdConversion(false);
  }
  return ret;
}

int CPDF_DIBSource::ContinueLoadDIBSource(IFX_Pause* pPause) {
  FXCODEC_STATUS ret;
  if (m_Status == 1) {
    const CFX_ByteString& decoder = m_pStreamAcc->GetImageDecoder();
    if (decoder == "JPXDecode") {
      return 0;
    }
    CCodec_Jbig2Module* pJbig2Module = CPDF_ModuleMgr::Get()->GetJbig2Module();
    if (!m_pJbig2Context) {
      m_pJbig2Context = pdfium::MakeUnique<CCodec_Jbig2Context>();
      if (m_pStreamAcc->GetImageParam()) {
        CPDF_Stream* pGlobals =
            m_pStreamAcc->GetImageParam()->GetStreamFor("JBIG2Globals");
        if (pGlobals) {
          m_pGlobalStream = pdfium::MakeUnique<CPDF_StreamAcc>();
          m_pGlobalStream->LoadAllData(pGlobals, false);
        }
      }
      ret = pJbig2Module->StartDecode(
          m_pJbig2Context.get(), m_pDocument->CodecContext(), m_Width, m_Height,
          m_pStreamAcc.get(), m_pGlobalStream.get(),
          m_pCachedBitmap->GetBuffer(), m_pCachedBitmap->GetPitch(), pPause);
      if (ret < 0) {
        m_pCachedBitmap.reset();
        m_pGlobalStream.reset();
        m_pJbig2Context.reset();
        return 0;
      }
      if (ret == FXCODEC_STATUS_DECODE_TOBECONTINUE) {
        return 2;
      }
      int ret1 = 1;
      if (m_bHasMask) {
        ret1 = ContinueLoadMaskDIB(pPause);
        m_Status = 2;
      }
      if (ret1 == 2) {
        return ret1;
      }
      if (m_pColorSpace && m_bStdCS) {
        m_pColorSpace->EnableStdConversion(false);
      }
      return ret1;
    }
    ret = pJbig2Module->ContinueDecode(m_pJbig2Context.get(), pPause);
    if (ret < 0) {
      m_pCachedBitmap.reset();
      m_pGlobalStream.reset();
      m_pJbig2Context.reset();
      return 0;
    }
    if (ret == FXCODEC_STATUS_DECODE_TOBECONTINUE) {
      return 2;
    }
    int ret1 = 1;
    if (m_bHasMask) {
      ret1 = ContinueLoadMaskDIB(pPause);
      m_Status = 2;
    }
    if (ret1 == 2) {
      return ret1;
    }
    if (m_pColorSpace && m_bStdCS) {
      m_pColorSpace->EnableStdConversion(false);
    }
    return ret1;
  }
  if (m_Status == 2) {
    return ContinueLoadMaskDIB(pPause);
  }
  return 0;
}

bool CPDF_DIBSource::LoadColorInfo(const CPDF_Dictionary* pFormResources,
                                   const CPDF_Dictionary* pPageResources) {
  m_bpc_orig = m_pDict->GetIntegerFor("BitsPerComponent");
  if (m_pDict->GetIntegerFor("ImageMask"))
    m_bImageMask = true;

  if (m_bImageMask || !m_pDict->KeyExist("ColorSpace")) {
    if (!m_bImageMask) {
      CPDF_Object* pFilter = m_pDict->GetDirectObjectFor("Filter");
      if (pFilter) {
        CFX_ByteString filter;
        if (pFilter->IsName()) {
          filter = pFilter->GetString();
        } else if (CPDF_Array* pArray = pFilter->AsArray()) {
          filter = pArray->GetStringAt(pArray->GetCount() - 1);
        }

        if (filter == "JPXDecode") {
          m_bDoBpcCheck = false;
          return true;
        }
      }
    }
    m_bImageMask = true;
    m_bpc = m_nComponents = 1;
    CPDF_Array* pDecode = m_pDict->GetArrayFor("Decode");
    m_bDefaultDecode = !pDecode || !pDecode->GetIntegerAt(0);
    return true;
  }

  CPDF_Object* pCSObj = m_pDict->GetDirectObjectFor("ColorSpace");
  if (!pCSObj)
    return false;

  CPDF_DocPageData* pDocPageData = m_pDocument->GetPageData();
  if (pFormResources)
    m_pColorSpace = pDocPageData->GetColorSpace(pCSObj, pFormResources);
  if (!m_pColorSpace)
    m_pColorSpace = pDocPageData->GetColorSpace(pCSObj, pPageResources);
  if (!m_pColorSpace)
    return false;

  m_Family = m_pColorSpace->GetFamily();
  m_nComponents = m_pColorSpace->CountComponents();
  if (m_Family == PDFCS_ICCBASED && pCSObj->IsName()) {
    CFX_ByteString cs = pCSObj->GetString();
    if (cs == "DeviceGray") {
      m_nComponents = 1;
    } else if (cs == "DeviceRGB") {
      m_nComponents = 3;
    } else if (cs == "DeviceCMYK") {
      m_nComponents = 4;
    }
  }
  ValidateDictParam();
  m_pCompData = GetDecodeAndMaskArray(m_bDefaultDecode, m_bColorKey);
  return !!m_pCompData;
}

DIB_COMP_DATA* CPDF_DIBSource::GetDecodeAndMaskArray(bool& bDefaultDecode,
                                                     bool& bColorKey) {
  if (!m_pColorSpace) {
    return nullptr;
  }
  DIB_COMP_DATA* pCompData = FX_Alloc(DIB_COMP_DATA, m_nComponents);
  int max_data = (1 << m_bpc) - 1;
  CPDF_Array* pDecode = m_pDict->GetArrayFor("Decode");
  if (pDecode) {
    for (uint32_t i = 0; i < m_nComponents; i++) {
      pCompData[i].m_DecodeMin = pDecode->GetNumberAt(i * 2);
      FX_FLOAT max = pDecode->GetNumberAt(i * 2 + 1);
      pCompData[i].m_DecodeStep = (max - pCompData[i].m_DecodeMin) / max_data;
      FX_FLOAT def_value;
      FX_FLOAT def_min;
      FX_FLOAT def_max;
      m_pColorSpace->GetDefaultValue(i, def_value, def_min, def_max);
      if (m_Family == PDFCS_INDEXED) {
        def_max = max_data;
      }
      if (def_min != pCompData[i].m_DecodeMin || def_max != max) {
        bDefaultDecode = false;
      }
    }
  } else {
    for (uint32_t i = 0; i < m_nComponents; i++) {
      FX_FLOAT def_value;
      m_pColorSpace->GetDefaultValue(i, def_value, pCompData[i].m_DecodeMin,
                                     pCompData[i].m_DecodeStep);
      if (m_Family == PDFCS_INDEXED) {
        pCompData[i].m_DecodeStep = max_data;
      }
      pCompData[i].m_DecodeStep =
          (pCompData[i].m_DecodeStep - pCompData[i].m_DecodeMin) / max_data;
    }
  }
  if (!m_pDict->KeyExist("SMask")) {
    CPDF_Object* pMask = m_pDict->GetDirectObjectFor("Mask");
    if (!pMask) {
      return pCompData;
    }
    if (CPDF_Array* pArray = pMask->AsArray()) {
      if (pArray->GetCount() >= m_nComponents * 2) {
        for (uint32_t i = 0; i < m_nComponents; i++) {
          int min_num = pArray->GetIntegerAt(i * 2);
          int max_num = pArray->GetIntegerAt(i * 2 + 1);
          pCompData[i].m_ColorKeyMin = std::max(min_num, 0);
          pCompData[i].m_ColorKeyMax = std::min(max_num, max_data);
        }
      }
      bColorKey = true;
    }
  }
  return pCompData;
}

int CPDF_DIBSource::CreateDecoder() {
  const CFX_ByteString& decoder = m_pStreamAcc->GetImageDecoder();
  if (decoder.IsEmpty())
    return 1;

  if (m_bDoBpcCheck && m_bpc == 0)
    return 0;

  if (decoder == "JPXDecode") {
    LoadJpxBitmap();
    return m_pCachedBitmap ? 1 : 0;
  }
  if (decoder == "JBIG2Decode") {
    m_pCachedBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
    if (!m_pCachedBitmap->Create(
            m_Width, m_Height, m_bImageMask ? FXDIB_1bppMask : FXDIB_1bppRgb)) {
      m_pCachedBitmap.reset();
      return 0;
    }
    m_Status = 1;
    return 2;
  }

  const uint8_t* src_data = m_pStreamAcc->GetData();
  uint32_t src_size = m_pStreamAcc->GetSize();
  const CPDF_Dictionary* pParams = m_pStreamAcc->GetImageParam();
  if (decoder == "CCITTFaxDecode") {
    m_pDecoder = FPDFAPI_CreateFaxDecoder(src_data, src_size, m_Width, m_Height,
                                          pParams);
  } else if (decoder == "FlateDecode") {
    m_pDecoder = FPDFAPI_CreateFlateDecoder(
        src_data, src_size, m_Width, m_Height, m_nComponents, m_bpc, pParams);
  } else if (decoder == "RunLengthDecode") {
    m_pDecoder = CPDF_ModuleMgr::Get()
                     ->GetCodecModule()
                     ->GetBasicModule()
                     ->CreateRunLengthDecoder(src_data, src_size, m_Width,
                                              m_Height, m_nComponents, m_bpc);
  } else if (decoder == "DCTDecode") {
    m_pDecoder = CPDF_ModuleMgr::Get()->GetJpegModule()->CreateDecoder(
        src_data, src_size, m_Width, m_Height, m_nComponents,
        !pParams || pParams->GetIntegerFor("ColorTransform", 1));
    if (!m_pDecoder) {
      bool bTransform = false;
      int comps;
      int bpc;
      CCodec_JpegModule* pJpegModule = CPDF_ModuleMgr::Get()->GetJpegModule();
      if (pJpegModule->LoadInfo(src_data, src_size, &m_Width, &m_Height, &comps,
                                &bpc, &bTransform)) {
        if (m_nComponents != static_cast<uint32_t>(comps)) {
          FX_Free(m_pCompData);
          m_pCompData = nullptr;
          m_nComponents = static_cast<uint32_t>(comps);
          if (m_pColorSpace) {
            switch (m_Family) {
              case PDFCS_DEVICEGRAY:
              case PDFCS_DEVICERGB:
              case PDFCS_DEVICECMYK: {
                uint32_t dwMinComps = ComponentsForFamily(m_Family);
                if (m_pColorSpace->CountComponents() < dwMinComps ||
                    m_nComponents < dwMinComps) {
                  return 0;
                }
                break;
              }
              case PDFCS_LAB: {
                if (m_nComponents != 3 || m_pColorSpace->CountComponents() < 3)
                  return 0;
                break;
              }
              case PDFCS_ICCBASED: {
                if (!IsAllowedICCComponents(m_nComponents) ||
                    !IsAllowedICCComponents(m_pColorSpace->CountComponents()) ||
                    m_pColorSpace->CountComponents() < m_nComponents) {
                  return 0;
                }
                break;
              }
              default: {
                if (m_pColorSpace->CountComponents() != m_nComponents)
                  return 0;
                break;
              }
            }
          } else {
            if (m_Family == PDFCS_LAB && m_nComponents != 3)
              return 0;
          }
          m_pCompData = GetDecodeAndMaskArray(m_bDefaultDecode, m_bColorKey);
          if (!m_pCompData)
            return 0;
        }
        m_bpc = bpc;
        m_pDecoder = CPDF_ModuleMgr::Get()->GetJpegModule()->CreateDecoder(
            src_data, src_size, m_Width, m_Height, m_nComponents, bTransform);
      }
    }
  }
  if (!m_pDecoder)
    return 0;

  FX_SAFE_UINT32 requested_pitch =
      CalculatePitch8(m_bpc, m_nComponents, m_Width);
  if (!requested_pitch.IsValid())
    return 0;
  FX_SAFE_UINT32 provided_pitch = CalculatePitch8(
      m_pDecoder->GetBPC(), m_pDecoder->CountComps(), m_pDecoder->GetWidth());
  if (!provided_pitch.IsValid())
    return 0;
  if (provided_pitch.ValueOrDie() < requested_pitch.ValueOrDie())
    return 0;
  return 1;
}

void CPDF_DIBSource::LoadJpxBitmap() {
  CCodec_JpxModule* pJpxModule = CPDF_ModuleMgr::Get()->GetJpxModule();
  if (!pJpxModule)
    return;

  std::unique_ptr<JpxBitMapContext> context(new JpxBitMapContext(pJpxModule));
  context->set_decoder(pJpxModule->CreateDecoder(
      m_pStreamAcc->GetData(), m_pStreamAcc->GetSize(), m_pColorSpace));
  if (!context->decoder())
    return;

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t components = 0;
  pJpxModule->GetImageInfo(context->decoder(), &width, &height, &components);
  if (static_cast<int>(width) < m_Width || static_cast<int>(height) < m_Height)
    return;

  bool bSwapRGB = false;
  if (m_pColorSpace) {
    if (components != m_pColorSpace->CountComponents())
      return;

    if (m_pColorSpace == CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB)) {
      bSwapRGB = true;
      m_pColorSpace = nullptr;
    }
  } else {
    if (components == 3) {
      bSwapRGB = true;
    } else if (components == 4) {
      m_pColorSpace = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICECMYK);
    }
    m_nComponents = components;
  }

  FXDIB_Format format;
  if (components == 1) {
    format = FXDIB_8bppRgb;
  } else if (components <= 3) {
    format = FXDIB_Rgb;
  } else if (components == 4) {
    format = FXDIB_Rgb32;
  } else {
    width = (width * components + 2) / 3;
    format = FXDIB_Rgb;
  }

  m_pCachedBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!m_pCachedBitmap->Create(width, height, format)) {
    m_pCachedBitmap.reset();
    return;
  }
  m_pCachedBitmap->Clear(0xFFFFFFFF);
  std::vector<uint8_t> output_offsets(components);
  for (uint32_t i = 0; i < components; ++i)
    output_offsets[i] = i;
  if (bSwapRGB) {
    output_offsets[0] = 2;
    output_offsets[2] = 0;
  }
  if (!pJpxModule->Decode(context->decoder(), m_pCachedBitmap->GetBuffer(),
                          m_pCachedBitmap->GetPitch(), output_offsets)) {
    m_pCachedBitmap.reset();
    return;
  }
  if (m_pColorSpace && m_pColorSpace->GetFamily() == PDFCS_INDEXED &&
      m_bpc < 8) {
    int scale = 8 - m_bpc;
    for (uint32_t row = 0; row < height; ++row) {
      uint8_t* scanline =
          const_cast<uint8_t*>(m_pCachedBitmap->GetScanline(row));
      for (uint32_t col = 0; col < width; ++col) {
        *scanline = (*scanline) >> scale;
        ++scanline;
      }
    }
  }
  m_bpc = 8;
}

int CPDF_DIBSource::StratLoadMask() {
  m_MatteColor = 0XFFFFFFFF;
  m_pMaskStream = m_pDict->GetStreamFor("SMask");
  if (m_pMaskStream) {
    CPDF_Array* pMatte = m_pMaskStream->GetDict()->GetArrayFor("Matte");
    if (pMatte && m_pColorSpace &&
        m_pColorSpace->CountComponents() <= m_nComponents) {
      FX_FLOAT R, G, B;
      std::vector<FX_FLOAT> colors(m_nComponents);
      for (uint32_t i = 0; i < m_nComponents; i++) {
        colors[i] = pMatte->GetFloatAt(i);
      }
      m_pColorSpace->GetRGB(colors.data(), R, G, B);
      m_MatteColor = FXARGB_MAKE(0, FXSYS_round(R * 255), FXSYS_round(G * 255),
                                 FXSYS_round(B * 255));
    }
    return StartLoadMaskDIB();
  }

  m_pMaskStream = ToStream(m_pDict->GetDirectObjectFor("Mask"));
  return m_pMaskStream ? StartLoadMaskDIB() : 1;
}

int CPDF_DIBSource::ContinueLoadMaskDIB(IFX_Pause* pPause) {
  if (!m_pMask) {
    return 1;
  }
  int ret = m_pMask->ContinueLoadDIBSource(pPause);
  if (ret == 2) {
    return ret;
  }
  if (m_pColorSpace && m_bStdCS) {
    m_pColorSpace->EnableStdConversion(false);
  }
  if (!ret) {
    delete m_pMask;
    m_pMask = nullptr;
    return ret;
  }
  return 1;
}

CPDF_DIBSource* CPDF_DIBSource::DetachMask() {
  CPDF_DIBSource* pDIBSource = m_pMask;
  m_pMask = nullptr;
  return pDIBSource;
}

int CPDF_DIBSource::StartLoadMaskDIB() {
  m_pMask = new CPDF_DIBSource;
  int ret = m_pMask->StartLoadDIBSource(m_pDocument, m_pMaskStream, false,
                                        nullptr, nullptr, true);
  if (ret == 2) {
    if (m_Status == 0)
      m_Status = 2;
    return 2;
  }
  if (!ret) {
    delete m_pMask;
    m_pMask = nullptr;
    return 1;
  }
  return 1;
}

void CPDF_DIBSource::LoadPalette() {
  if (m_bpc == 0) {
    return;
  }
  if (m_bpc * m_nComponents > 8) {
    return;
  }
  if (!m_pColorSpace) {
    return;
  }
  if (m_bpc * m_nComponents == 1) {
    if (m_bDefaultDecode &&
        (m_Family == PDFCS_DEVICEGRAY || m_Family == PDFCS_DEVICERGB)) {
      return;
    }
    if (m_pColorSpace->CountComponents() > 3) {
      return;
    }
    FX_FLOAT color_values[3];
    color_values[0] = m_pCompData[0].m_DecodeMin;
    color_values[1] = color_values[2] = color_values[0];
    FX_FLOAT R = 0.0f, G = 0.0f, B = 0.0f;
    m_pColorSpace->GetRGB(color_values, R, G, B);
    FX_ARGB argb0 = ArgbEncode(255, FXSYS_round(R * 255), FXSYS_round(G * 255),
                               FXSYS_round(B * 255));
    color_values[0] += m_pCompData[0].m_DecodeStep;
    color_values[1] += m_pCompData[0].m_DecodeStep;
    color_values[2] += m_pCompData[0].m_DecodeStep;
    m_pColorSpace->GetRGB(color_values, R, G, B);
    FX_ARGB argb1 = ArgbEncode(255, FXSYS_round(R * 255), FXSYS_round(G * 255),
                               FXSYS_round(B * 255));
    if (argb0 != 0xFF000000 || argb1 != 0xFFFFFFFF) {
      SetPaletteArgb(0, argb0);
      SetPaletteArgb(1, argb1);
    }
    return;
  }
  if (m_pColorSpace == CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY) &&
      m_bpc == 8 && m_bDefaultDecode) {
  } else {
    int palette_count = 1 << (m_bpc * m_nComponents);
    CFX_FixedBufGrow<FX_FLOAT, 16> color_values(m_nComponents);
    FX_FLOAT* color_value = color_values;
    for (int i = 0; i < palette_count; i++) {
      int color_data = i;
      for (uint32_t j = 0; j < m_nComponents; j++) {
        int encoded_component = color_data % (1 << m_bpc);
        color_data /= 1 << m_bpc;
        color_value[j] = m_pCompData[j].m_DecodeMin +
                         m_pCompData[j].m_DecodeStep * encoded_component;
      }
      FX_FLOAT R = 0, G = 0, B = 0;
      if (m_nComponents == 1 && m_Family == PDFCS_ICCBASED &&
          m_pColorSpace->CountComponents() > 1) {
        int nComponents = m_pColorSpace->CountComponents();
        std::vector<FX_FLOAT> temp_buf(nComponents);
        for (int k = 0; k < nComponents; k++) {
          temp_buf[k] = *color_value;
        }
        m_pColorSpace->GetRGB(temp_buf.data(), R, G, B);
      } else {
        m_pColorSpace->GetRGB(color_value, R, G, B);
      }
      SetPaletteArgb(i, ArgbEncode(255, FXSYS_round(R * 255),
                                   FXSYS_round(G * 255), FXSYS_round(B * 255)));
    }
  }
}

void CPDF_DIBSource::ValidateDictParam() {
  m_bpc = m_bpc_orig;
  CPDF_Object* pFilter = m_pDict->GetDirectObjectFor("Filter");
  if (pFilter) {
    if (pFilter->IsName()) {
      CFX_ByteString filter = pFilter->GetString();
      if (filter == "CCITTFaxDecode" || filter == "JBIG2Decode") {
        m_bpc = 1;
        m_nComponents = 1;
      } else if (filter == "RunLengthDecode") {
        if (m_bpc != 1) {
          m_bpc = 8;
        }
      } else if (filter == "DCTDecode") {
        m_bpc = 8;
      }
    } else if (CPDF_Array* pArray = pFilter->AsArray()) {
      CFX_ByteString filter = pArray->GetStringAt(pArray->GetCount() - 1);
      if (filter == "CCITTFaxDecode" || filter == "JBIG2Decode") {
        m_bpc = 1;
        m_nComponents = 1;
      } else if (filter == "DCTDecode") {
        // Previously, filter == "RunLengthDecode" was checked in the "if"
        // statement as well, but too many documents don't conform to it.
        m_bpc = 8;
      }
    }
  }

  if (!IsAllowedBPCValue(m_bpc))
    m_bpc = 0;
}

void CPDF_DIBSource::TranslateScanline24bpp(uint8_t* dest_scan,
                                            const uint8_t* src_scan) const {
  if (m_bpc == 0) {
    return;
  }
  unsigned int max_data = (1 << m_bpc) - 1;
  if (m_bDefaultDecode) {
    if (m_Family == PDFCS_DEVICERGB || m_Family == PDFCS_CALRGB) {
      if (m_nComponents != 3)
        return;

      const uint8_t* src_pos = src_scan;
      switch (m_bpc) {
        case 16:
          for (int col = 0; col < m_Width; col++) {
            *dest_scan++ = src_pos[4];
            *dest_scan++ = src_pos[2];
            *dest_scan++ = *src_pos;
            src_pos += 6;
          }
          break;
        case 8:
          for (int column = 0; column < m_Width; column++) {
            *dest_scan++ = src_pos[2];
            *dest_scan++ = src_pos[1];
            *dest_scan++ = *src_pos;
            src_pos += 3;
          }
          break;
        default:
          uint64_t src_bit_pos = 0;
          size_t dest_byte_pos = 0;
          for (int column = 0; column < m_Width; column++) {
            unsigned int R = GetBits8(src_scan, src_bit_pos, m_bpc);
            src_bit_pos += m_bpc;
            unsigned int G = GetBits8(src_scan, src_bit_pos, m_bpc);
            src_bit_pos += m_bpc;
            unsigned int B = GetBits8(src_scan, src_bit_pos, m_bpc);
            src_bit_pos += m_bpc;
            R = std::min(R, max_data);
            G = std::min(G, max_data);
            B = std::min(B, max_data);
            dest_scan[dest_byte_pos] = B * 255 / max_data;
            dest_scan[dest_byte_pos + 1] = G * 255 / max_data;
            dest_scan[dest_byte_pos + 2] = R * 255 / max_data;
            dest_byte_pos += 3;
          }
          break;
      }
      return;
    }
    if (m_bpc == 8) {
      if (m_nComponents == m_pColorSpace->CountComponents())
        m_pColorSpace->TranslateImageLine(dest_scan, src_scan, m_Width, m_Width,
                                          m_Height, TransMask());
      return;
    }
  }
  CFX_FixedBufGrow<FX_FLOAT, 16> color_values1(m_nComponents);
  FX_FLOAT* color_values = color_values1;
  FX_FLOAT R = 0.0f, G = 0.0f, B = 0.0f;
  if (m_bpc == 8) {
    uint64_t src_byte_pos = 0;
    size_t dest_byte_pos = 0;
    for (int column = 0; column < m_Width; column++) {
      for (uint32_t color = 0; color < m_nComponents; color++) {
        uint8_t data = src_scan[src_byte_pos++];
        color_values[color] = m_pCompData[color].m_DecodeMin +
                              m_pCompData[color].m_DecodeStep * data;
      }
      if (TransMask()) {
        FX_FLOAT k = 1.0f - color_values[3];
        R = (1.0f - color_values[0]) * k;
        G = (1.0f - color_values[1]) * k;
        B = (1.0f - color_values[2]) * k;
      } else {
        m_pColorSpace->GetRGB(color_values, R, G, B);
      }
      R = ClampValue(R, 1.0f);
      G = ClampValue(G, 1.0f);
      B = ClampValue(B, 1.0f);
      dest_scan[dest_byte_pos] = static_cast<uint8_t>(B * 255);
      dest_scan[dest_byte_pos + 1] = static_cast<uint8_t>(G * 255);
      dest_scan[dest_byte_pos + 2] = static_cast<uint8_t>(R * 255);
      dest_byte_pos += 3;
    }
  } else {
    uint64_t src_bit_pos = 0;
    size_t dest_byte_pos = 0;
    for (int column = 0; column < m_Width; column++) {
      for (uint32_t color = 0; color < m_nComponents; color++) {
        unsigned int data = GetBits8(src_scan, src_bit_pos, m_bpc);
        color_values[color] = m_pCompData[color].m_DecodeMin +
                              m_pCompData[color].m_DecodeStep * data;
        src_bit_pos += m_bpc;
      }
      if (TransMask()) {
        FX_FLOAT k = 1.0f - color_values[3];
        R = (1.0f - color_values[0]) * k;
        G = (1.0f - color_values[1]) * k;
        B = (1.0f - color_values[2]) * k;
      } else {
        m_pColorSpace->GetRGB(color_values, R, G, B);
      }
      R = ClampValue(R, 1.0f);
      G = ClampValue(G, 1.0f);
      B = ClampValue(B, 1.0f);
      dest_scan[dest_byte_pos] = static_cast<uint8_t>(B * 255);
      dest_scan[dest_byte_pos + 1] = static_cast<uint8_t>(G * 255);
      dest_scan[dest_byte_pos + 2] = static_cast<uint8_t>(R * 255);
      dest_byte_pos += 3;
    }
  }
}

uint8_t* CPDF_DIBSource::GetBuffer() const {
  return m_pCachedBitmap ? m_pCachedBitmap->GetBuffer() : nullptr;
}

const uint8_t* CPDF_DIBSource::GetScanline(int line) const {
  if (m_bpc == 0) {
    return nullptr;
  }
  FX_SAFE_UINT32 src_pitch = CalculatePitch8(m_bpc, m_nComponents, m_Width);
  if (!src_pitch.IsValid())
    return nullptr;
  uint32_t src_pitch_value = src_pitch.ValueOrDie();
  const uint8_t* pSrcLine = nullptr;
  if (m_pCachedBitmap && src_pitch_value <= m_pCachedBitmap->GetPitch()) {
    if (line >= m_pCachedBitmap->GetHeight()) {
      line = m_pCachedBitmap->GetHeight() - 1;
    }
    pSrcLine = m_pCachedBitmap->GetScanline(line);
  } else if (m_pDecoder) {
    pSrcLine = m_pDecoder->GetScanline(line);
  } else {
    if (m_pStreamAcc->GetSize() >= (line + 1) * src_pitch_value) {
      pSrcLine = m_pStreamAcc->GetData() + line * src_pitch_value;
    }
  }
  if (!pSrcLine) {
    uint8_t* pLineBuf = m_pMaskedLine ? m_pMaskedLine : m_pLineBuf;
    FXSYS_memset(pLineBuf, 0xFF, m_Pitch);
    return pLineBuf;
  }
  if (m_bpc * m_nComponents == 1) {
    if (m_bImageMask && m_bDefaultDecode) {
      for (uint32_t i = 0; i < src_pitch_value; i++) {
        m_pLineBuf[i] = ~pSrcLine[i];
      }
    } else if (m_bColorKey) {
      uint32_t reset_argb, set_argb;
      reset_argb = m_pPalette ? m_pPalette.get()[0] : 0xFF000000;
      set_argb = m_pPalette ? m_pPalette.get()[1] : 0xFFFFFFFF;
      if (m_pCompData[0].m_ColorKeyMin == 0) {
        reset_argb = 0;
      }
      if (m_pCompData[0].m_ColorKeyMax == 1) {
        set_argb = 0;
      }
      set_argb = FXARGB_TODIB(set_argb);
      reset_argb = FXARGB_TODIB(reset_argb);
      uint32_t* dest_scan = reinterpret_cast<uint32_t*>(m_pMaskedLine);
      for (int col = 0; col < m_Width; col++) {
        if (pSrcLine[col / 8] & (1 << (7 - col % 8))) {
          *dest_scan = set_argb;
        } else {
          *dest_scan = reset_argb;
        }
        dest_scan++;
      }
      return m_pMaskedLine;
    } else {
      FXSYS_memcpy(m_pLineBuf, pSrcLine, src_pitch_value);
    }
    return m_pLineBuf;
  }
  if (m_bpc * m_nComponents <= 8) {
    if (m_bpc == 8) {
      FXSYS_memcpy(m_pLineBuf, pSrcLine, src_pitch_value);
    } else {
      uint64_t src_bit_pos = 0;
      for (int col = 0; col < m_Width; col++) {
        unsigned int color_index = 0;
        for (uint32_t color = 0; color < m_nComponents; color++) {
          unsigned int data = GetBits8(pSrcLine, src_bit_pos, m_bpc);
          color_index |= data << (color * m_bpc);
          src_bit_pos += m_bpc;
        }
        m_pLineBuf[col] = color_index;
      }
    }
    if (m_bColorKey) {
      uint8_t* pDestPixel = m_pMaskedLine;
      const uint8_t* pSrcPixel = m_pLineBuf;
      for (int col = 0; col < m_Width; col++) {
        uint8_t index = *pSrcPixel++;
        if (m_pPalette) {
          *pDestPixel++ = FXARGB_B(m_pPalette.get()[index]);
          *pDestPixel++ = FXARGB_G(m_pPalette.get()[index]);
          *pDestPixel++ = FXARGB_R(m_pPalette.get()[index]);
        } else {
          *pDestPixel++ = index;
          *pDestPixel++ = index;
          *pDestPixel++ = index;
        }
        *pDestPixel = (index < m_pCompData[0].m_ColorKeyMin ||
                       index > m_pCompData[0].m_ColorKeyMax)
                          ? 0xFF
                          : 0;
        pDestPixel++;
      }
      return m_pMaskedLine;
    }
    return m_pLineBuf;
  }
  if (m_bColorKey) {
    if (m_nComponents == 3 && m_bpc == 8) {
      uint8_t* alpha_channel = m_pMaskedLine + 3;
      for (int col = 0; col < m_Width; col++) {
        const uint8_t* pPixel = pSrcLine + col * 3;
        alpha_channel[col * 4] = (pPixel[0] < m_pCompData[0].m_ColorKeyMin ||
                                  pPixel[0] > m_pCompData[0].m_ColorKeyMax ||
                                  pPixel[1] < m_pCompData[1].m_ColorKeyMin ||
                                  pPixel[1] > m_pCompData[1].m_ColorKeyMax ||
                                  pPixel[2] < m_pCompData[2].m_ColorKeyMin ||
                                  pPixel[2] > m_pCompData[2].m_ColorKeyMax)
                                     ? 0xFF
                                     : 0;
      }
    } else {
      FXSYS_memset(m_pMaskedLine, 0xFF, m_Pitch);
    }
  }
  if (m_pColorSpace) {
    TranslateScanline24bpp(m_pLineBuf, pSrcLine);
    pSrcLine = m_pLineBuf;
  }
  if (m_bColorKey) {
    const uint8_t* pSrcPixel = pSrcLine;
    uint8_t* pDestPixel = m_pMaskedLine;
    for (int col = 0; col < m_Width; col++) {
      *pDestPixel++ = *pSrcPixel++;
      *pDestPixel++ = *pSrcPixel++;
      *pDestPixel++ = *pSrcPixel++;
      pDestPixel++;
    }
    return m_pMaskedLine;
  }
  return pSrcLine;
}

bool CPDF_DIBSource::SkipToScanline(int line, IFX_Pause* pPause) const {
  return m_pDecoder && m_pDecoder->SkipToScanline(line, pPause);
}

void CPDF_DIBSource::DownSampleScanline(int line,
                                        uint8_t* dest_scan,
                                        int dest_bpp,
                                        int dest_width,
                                        bool bFlipX,
                                        int clip_left,
                                        int clip_width) const {
  if (line < 0 || !dest_scan || dest_bpp <= 0 || dest_width <= 0 ||
      clip_left < 0 || clip_width <= 0) {
    return;
  }

  uint32_t src_width = m_Width;
  FX_SAFE_UINT32 pitch = CalculatePitch8(m_bpc, m_nComponents, m_Width);
  if (!pitch.IsValid())
    return;

  const uint8_t* pSrcLine = nullptr;
  if (m_pCachedBitmap) {
    pSrcLine = m_pCachedBitmap->GetScanline(line);
  } else if (m_pDecoder) {
    pSrcLine = m_pDecoder->GetScanline(line);
  } else {
    uint32_t src_pitch = pitch.ValueOrDie();
    pitch *= (line + 1);
    if (!pitch.IsValid()) {
      return;
    }

    if (m_pStreamAcc->GetSize() >= pitch.ValueOrDie()) {
      pSrcLine = m_pStreamAcc->GetData() + line * src_pitch;
    }
  }
  int orig_Bpp = m_bpc * m_nComponents / 8;
  int dest_Bpp = dest_bpp / 8;
  if (!pSrcLine) {
    FXSYS_memset(dest_scan, 0xFF, dest_Bpp * clip_width);
    return;
  }

  FX_SAFE_INT32 max_src_x = clip_left;
  max_src_x += clip_width - 1;
  max_src_x *= src_width;
  max_src_x /= dest_width;
  if (!max_src_x.IsValid())
    return;

  if (m_bpc * m_nComponents == 1) {
    DownSampleScanline1Bit(orig_Bpp, dest_Bpp, src_width, pSrcLine, dest_scan,
                           dest_width, bFlipX, clip_left, clip_width);
  } else if (m_bpc * m_nComponents <= 8) {
    DownSampleScanline8Bit(orig_Bpp, dest_Bpp, src_width, pSrcLine, dest_scan,
                           dest_width, bFlipX, clip_left, clip_width);
  } else {
    DownSampleScanline32Bit(orig_Bpp, dest_Bpp, src_width, pSrcLine, dest_scan,
                            dest_width, bFlipX, clip_left, clip_width);
  }
}

void CPDF_DIBSource::DownSampleScanline1Bit(int orig_Bpp,
                                            int dest_Bpp,
                                            uint32_t src_width,
                                            const uint8_t* pSrcLine,
                                            uint8_t* dest_scan,
                                            int dest_width,
                                            bool bFlipX,
                                            int clip_left,
                                            int clip_width) const {
  uint32_t set_argb = (uint32_t)-1;
  uint32_t reset_argb = 0;
  if (m_bImageMask) {
    if (m_bDefaultDecode) {
      set_argb = 0;
      reset_argb = (uint32_t)-1;
    }
  } else if (m_bColorKey) {
    reset_argb = m_pPalette ? m_pPalette.get()[0] : 0xFF000000;
    set_argb = m_pPalette ? m_pPalette.get()[1] : 0xFFFFFFFF;
    if (m_pCompData[0].m_ColorKeyMin == 0) {
      reset_argb = 0;
    }
    if (m_pCompData[0].m_ColorKeyMax == 1) {
      set_argb = 0;
    }
    set_argb = FXARGB_TODIB(set_argb);
    reset_argb = FXARGB_TODIB(reset_argb);
    uint32_t* dest_scan_dword = reinterpret_cast<uint32_t*>(dest_scan);
    for (int i = 0; i < clip_width; i++) {
      uint32_t src_x = (clip_left + i) * src_width / dest_width;
      if (bFlipX) {
        src_x = src_width - src_x - 1;
      }
      src_x %= src_width;
      if (pSrcLine[src_x / 8] & (1 << (7 - src_x % 8))) {
        dest_scan_dword[i] = set_argb;
      } else {
        dest_scan_dword[i] = reset_argb;
      }
    }
    return;
  } else {
    if (dest_Bpp == 1) {
    } else if (m_pPalette) {
      reset_argb = m_pPalette.get()[0];
      set_argb = m_pPalette.get()[1];
    }
  }
  for (int i = 0; i < clip_width; i++) {
    uint32_t src_x = (clip_left + i) * src_width / dest_width;
    if (bFlipX) {
      src_x = src_width - src_x - 1;
    }
    src_x %= src_width;
    int dest_pos = i * dest_Bpp;
    if (pSrcLine[src_x / 8] & (1 << (7 - src_x % 8))) {
      if (dest_Bpp == 1) {
        dest_scan[dest_pos] = static_cast<uint8_t>(set_argb);
      } else if (dest_Bpp == 3) {
        dest_scan[dest_pos] = FXARGB_B(set_argb);
        dest_scan[dest_pos + 1] = FXARGB_G(set_argb);
        dest_scan[dest_pos + 2] = FXARGB_R(set_argb);
      } else {
        *reinterpret_cast<uint32_t*>(dest_scan + dest_pos) = set_argb;
      }
    } else {
      if (dest_Bpp == 1) {
        dest_scan[dest_pos] = static_cast<uint8_t>(reset_argb);
      } else if (dest_Bpp == 3) {
        dest_scan[dest_pos] = FXARGB_B(reset_argb);
        dest_scan[dest_pos + 1] = FXARGB_G(reset_argb);
        dest_scan[dest_pos + 2] = FXARGB_R(reset_argb);
      } else {
        *reinterpret_cast<uint32_t*>(dest_scan + dest_pos) = reset_argb;
      }
    }
  }
}

void CPDF_DIBSource::DownSampleScanline8Bit(int orig_Bpp,
                                            int dest_Bpp,
                                            uint32_t src_width,
                                            const uint8_t* pSrcLine,
                                            uint8_t* dest_scan,
                                            int dest_width,
                                            bool bFlipX,
                                            int clip_left,
                                            int clip_width) const {
  if (m_bpc < 8) {
    uint64_t src_bit_pos = 0;
    for (uint32_t col = 0; col < src_width; col++) {
      unsigned int color_index = 0;
      for (uint32_t color = 0; color < m_nComponents; color++) {
        unsigned int data = GetBits8(pSrcLine, src_bit_pos, m_bpc);
        color_index |= data << (color * m_bpc);
        src_bit_pos += m_bpc;
      }
      m_pLineBuf[col] = color_index;
    }
    pSrcLine = m_pLineBuf;
  }
  if (m_bColorKey) {
    for (int i = 0; i < clip_width; i++) {
      uint32_t src_x = (clip_left + i) * src_width / dest_width;
      if (bFlipX) {
        src_x = src_width - src_x - 1;
      }
      src_x %= src_width;
      uint8_t* pDestPixel = dest_scan + i * 4;
      uint8_t index = pSrcLine[src_x];
      if (m_pPalette) {
        *pDestPixel++ = FXARGB_B(m_pPalette.get()[index]);
        *pDestPixel++ = FXARGB_G(m_pPalette.get()[index]);
        *pDestPixel++ = FXARGB_R(m_pPalette.get()[index]);
      } else {
        *pDestPixel++ = index;
        *pDestPixel++ = index;
        *pDestPixel++ = index;
      }
      *pDestPixel = (index < m_pCompData[0].m_ColorKeyMin ||
                     index > m_pCompData[0].m_ColorKeyMax)
                        ? 0xFF
                        : 0;
    }
    return;
  }
  for (int i = 0; i < clip_width; i++) {
    uint32_t src_x = (clip_left + i) * src_width / dest_width;
    if (bFlipX) {
      src_x = src_width - src_x - 1;
    }
    src_x %= src_width;
    uint8_t index = pSrcLine[src_x];
    if (dest_Bpp == 1) {
      dest_scan[i] = index;
    } else {
      int dest_pos = i * dest_Bpp;
      FX_ARGB argb = m_pPalette.get()[index];
      dest_scan[dest_pos] = FXARGB_B(argb);
      dest_scan[dest_pos + 1] = FXARGB_G(argb);
      dest_scan[dest_pos + 2] = FXARGB_R(argb);
    }
  }
}

void CPDF_DIBSource::DownSampleScanline32Bit(int orig_Bpp,
                                             int dest_Bpp,
                                             uint32_t src_width,
                                             const uint8_t* pSrcLine,
                                             uint8_t* dest_scan,
                                             int dest_width,
                                             bool bFlipX,
                                             int clip_left,
                                             int clip_width) const {
  // last_src_x used to store the last seen src_x position which should be
  // in [0, src_width). Set the initial value to be an invalid src_x value.
  uint32_t last_src_x = src_width;
  FX_ARGB last_argb = FXARGB_MAKE(0xFF, 0xFF, 0xFF, 0xFF);
  FX_FLOAT unit_To8Bpc = 255.0f / ((1 << m_bpc) - 1);
  for (int i = 0; i < clip_width; i++) {
    int dest_x = clip_left + i;
    uint32_t src_x = (bFlipX ? (dest_width - dest_x - 1) : dest_x) *
                     (int64_t)src_width / dest_width;
    src_x %= src_width;

    uint8_t* pDestPixel = dest_scan + i * dest_Bpp;
    FX_ARGB argb;
    if (src_x == last_src_x) {
      argb = last_argb;
    } else {
      CFX_FixedBufGrow<uint8_t, 128> extracted_components(m_nComponents);
      const uint8_t* pSrcPixel = nullptr;
      if (m_bpc % 8 != 0) {
        // No need to check for 32-bit overflow, as |src_x| is bounded by
        // |src_width| and DownSampleScanline() already checked for overflow
        // with the pitch calculation.
        size_t num_bits = src_x * m_bpc * m_nComponents;
        uint64_t src_bit_pos = num_bits % 8;
        pSrcPixel = pSrcLine + num_bits / 8;
        for (uint32_t j = 0; j < m_nComponents; ++j) {
          extracted_components[j] = static_cast<uint8_t>(
              GetBits8(pSrcPixel, src_bit_pos, m_bpc) * unit_To8Bpc);
          src_bit_pos += m_bpc;
        }
        pSrcPixel = extracted_components;
      } else {
        pSrcPixel = pSrcLine + src_x * orig_Bpp;
        if (m_bpc == 16) {
          for (uint32_t j = 0; j < m_nComponents; ++j)
            extracted_components[j] = pSrcPixel[j * 2];
          pSrcPixel = extracted_components;
        }
      }

      if (m_pColorSpace) {
        uint8_t color[4];
        const bool bTransMask = TransMask();
        if (m_bDefaultDecode) {
          m_pColorSpace->TranslateImageLine(color, pSrcPixel, 1, 0, 0,
                                            bTransMask);
        } else {
          for (uint32_t j = 0; j < m_nComponents; ++j) {
            FX_FLOAT component_value =
                static_cast<FX_FLOAT>(extracted_components[j]);
            int color_value = static_cast<int>(
                (m_pCompData[j].m_DecodeMin +
                 m_pCompData[j].m_DecodeStep * component_value) *
                    255.0f +
                0.5f);
            extracted_components[j] =
                color_value > 255 ? 255 : (color_value < 0 ? 0 : color_value);
          }
          m_pColorSpace->TranslateImageLine(color, extracted_components, 1, 0,
                                            0, bTransMask);
        }
        argb = FXARGB_MAKE(0xFF, color[2], color[1], color[0]);
      } else {
        argb = FXARGB_MAKE(0xFF, pSrcPixel[2], pSrcPixel[1], pSrcPixel[0]);
      }
      if (m_bColorKey) {
        int alpha = 0xFF;
        if (m_nComponents == 3 && m_bpc == 8) {
          alpha = (pSrcPixel[0] < m_pCompData[0].m_ColorKeyMin ||
                   pSrcPixel[0] > m_pCompData[0].m_ColorKeyMax ||
                   pSrcPixel[1] < m_pCompData[1].m_ColorKeyMin ||
                   pSrcPixel[1] > m_pCompData[1].m_ColorKeyMax ||
                   pSrcPixel[2] < m_pCompData[2].m_ColorKeyMin ||
                   pSrcPixel[2] > m_pCompData[2].m_ColorKeyMax)
                      ? 0xFF
                      : 0;
        }
        argb &= 0xFFFFFF;
        argb |= alpha << 24;
      }
      last_src_x = src_x;
      last_argb = argb;
    }
    if (dest_Bpp == 4) {
      *reinterpret_cast<uint32_t*>(pDestPixel) = FXARGB_TODIB(argb);
    } else {
      *pDestPixel++ = FXARGB_B(argb);
      *pDestPixel++ = FXARGB_G(argb);
      *pDestPixel = FXARGB_R(argb);
    }
  }
}

bool CPDF_DIBSource::TransMask() const {
  return m_bLoadMask && m_GroupFamily == PDFCS_DEVICECMYK &&
         m_Family == PDFCS_DEVICECMYK;
}
