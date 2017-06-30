// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_fontmgr.h"

#include <memory>
#include <utility>

#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_substfont.h"
#include "core/fxge/fontdata/chromefontdata/chromefontdata.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/ge/cttfontdesc.h"
#include "core/fxge/ifx_systemfontinfo.h"
#include "third_party/base/ptr_util.h"

namespace {

struct BuiltinFont {
  const uint8_t* m_pFontData;
  uint32_t m_dwSize;
};

const BuiltinFont g_FoxitFonts[14] = {
    {g_FoxitFixedFontData, 17597},
    {g_FoxitFixedBoldFontData, 18055},
    {g_FoxitFixedBoldItalicFontData, 19151},
    {g_FoxitFixedItalicFontData, 18746},
    {g_FoxitSansFontData, 15025},
    {g_FoxitSansBoldFontData, 16344},
    {g_FoxitSansBoldItalicFontData, 16418},
    {g_FoxitSansItalicFontData, 16339},
    {g_FoxitSerifFontData, 19469},
    {g_FoxitSerifBoldFontData, 19395},
    {g_FoxitSerifBoldItalicFontData, 20733},
    {g_FoxitSerifItalicFontData, 21227},
    {g_FoxitSymbolFontData, 16729},
    {g_FoxitDingbatsFontData, 29513},
};

const BuiltinFont g_MMFonts[2] = {
    {g_FoxitSerifMMFontData, 113417},
    {g_FoxitSansMMFontData, 66919},
};

CFX_ByteString KeyNameFromFace(const CFX_ByteString& face_name,
                               int weight,
                               bool bItalic) {
  CFX_ByteString key(face_name);
  key += ',';
  key += CFX_ByteString::FormatInteger(weight);
  key += bItalic ? 'I' : 'N';
  return key;
}

CFX_ByteString KeyNameFromSize(int ttc_size, uint32_t checksum) {
  CFX_ByteString key;
  key.Format("%d:%d", ttc_size, checksum);
  return key;
}

int GetTTCIndex(const uint8_t* pFontData,
                uint32_t ttc_size,
                uint32_t font_offset) {
  int face_index = 0;
  const uint8_t* p = pFontData + 8;
  uint32_t nfont = GET_TT_LONG(p);
  uint32_t index;
  for (index = 0; index < nfont; index++) {
    p = pFontData + 12 + index * 4;
    if (GET_TT_LONG(p) == font_offset)
      break;
  }
  if (index >= nfont)
    face_index = 0;
  else
    face_index = index;
  return face_index;
}

}  // namespace

CFX_FontMgr::CFX_FontMgr()
    : m_FTLibrary(nullptr), m_FTLibrarySupportsHinting(false) {
  m_pBuiltinMapper = pdfium::MakeUnique<CFX_FontMapper>(this);
}

CFX_FontMgr::~CFX_FontMgr() {
  for (const auto& pair : m_FaceMap)
    delete pair.second;

  // |m_pBuiltinMapper| references |m_FTLibrary|, so it has to be destroyed
  // first.
  m_pBuiltinMapper.reset();
  FXFT_Done_FreeType(m_FTLibrary);
}

void CFX_FontMgr::InitFTLibrary() {
  if (m_FTLibrary)
    return;
  FXFT_Init_FreeType(&m_FTLibrary);
  m_FTLibrarySupportsHinting =
      FXFT_Library_SetLcdFilter(m_FTLibrary, FT_LCD_FILTER_DEFAULT) !=
      FT_Err_Unimplemented_Feature;
}

void CFX_FontMgr::SetSystemFontInfo(
    std::unique_ptr<IFX_SystemFontInfo> pFontInfo) {
  m_pBuiltinMapper->SetSystemFontInfo(std::move(pFontInfo));
}

FXFT_Face CFX_FontMgr::FindSubstFont(const CFX_ByteString& face_name,
                                     bool bTrueType,
                                     uint32_t flags,
                                     int weight,
                                     int italic_angle,
                                     int CharsetCP,
                                     CFX_SubstFont* pSubstFont) {
  InitFTLibrary();
  return m_pBuiltinMapper->FindSubstFont(face_name, bTrueType, flags, weight,
                                         italic_angle, CharsetCP, pSubstFont);
}

FXFT_Face CFX_FontMgr::GetCachedFace(const CFX_ByteString& face_name,
                                     int weight,
                                     bool bItalic,
                                     uint8_t*& pFontData) {
  auto it = m_FaceMap.find(KeyNameFromFace(face_name, weight, bItalic));
  if (it == m_FaceMap.end())
    return nullptr;

  CTTFontDesc* pFontDesc = it->second;
  pFontData = pFontDesc->m_pFontData;
  pFontDesc->m_RefCount++;
  return pFontDesc->m_SingleFace;
}

FXFT_Face CFX_FontMgr::AddCachedFace(const CFX_ByteString& face_name,
                                     int weight,
                                     bool bItalic,
                                     uint8_t* pData,
                                     uint32_t size,
                                     int face_index) {
  CTTFontDesc* pFontDesc = new CTTFontDesc;
  pFontDesc->m_Type = 1;
  pFontDesc->m_SingleFace = nullptr;
  pFontDesc->m_pFontData = pData;
  pFontDesc->m_RefCount = 1;

  InitFTLibrary();
  FXFT_Library library = m_FTLibrary;
  int ret = FXFT_New_Memory_Face(library, pData, size, face_index,
                                 &pFontDesc->m_SingleFace);
  if (ret) {
    delete pFontDesc;
    return nullptr;
  }
  ret = FXFT_Set_Pixel_Sizes(pFontDesc->m_SingleFace, 64, 64);
  if (ret) {
    delete pFontDesc;
    return nullptr;
  }
  m_FaceMap[KeyNameFromFace(face_name, weight, bItalic)] = pFontDesc;
  return pFontDesc->m_SingleFace;
}

FXFT_Face CFX_FontMgr::GetCachedTTCFace(int ttc_size,
                                        uint32_t checksum,
                                        int font_offset,
                                        uint8_t*& pFontData) {
  auto it = m_FaceMap.find(KeyNameFromSize(ttc_size, checksum));
  if (it == m_FaceMap.end())
    return nullptr;

  CTTFontDesc* pFontDesc = it->second;
  pFontData = pFontDesc->m_pFontData;
  pFontDesc->m_RefCount++;
  int face_index = GetTTCIndex(pFontDesc->m_pFontData, ttc_size, font_offset);
  if (!pFontDesc->m_TTCFaces[face_index]) {
    pFontDesc->m_TTCFaces[face_index] =
        GetFixedFace(pFontDesc->m_pFontData, ttc_size, face_index);
  }
  return pFontDesc->m_TTCFaces[face_index];
}

FXFT_Face CFX_FontMgr::AddCachedTTCFace(int ttc_size,
                                        uint32_t checksum,
                                        uint8_t* pData,
                                        uint32_t size,
                                        int font_offset) {
  CTTFontDesc* pFontDesc = new CTTFontDesc;
  pFontDesc->m_Type = 2;
  pFontDesc->m_pFontData = pData;
  for (int i = 0; i < 16; i++)
    pFontDesc->m_TTCFaces[i] = nullptr;
  pFontDesc->m_RefCount++;
  m_FaceMap[KeyNameFromSize(ttc_size, checksum)] = pFontDesc;
  int face_index = GetTTCIndex(pFontDesc->m_pFontData, ttc_size, font_offset);
  pFontDesc->m_TTCFaces[face_index] =
      GetFixedFace(pFontDesc->m_pFontData, ttc_size, face_index);
  return pFontDesc->m_TTCFaces[face_index];
}

FXFT_Face CFX_FontMgr::GetFixedFace(const uint8_t* pData,
                                    uint32_t size,
                                    int face_index) {
  InitFTLibrary();
  FXFT_Library library = m_FTLibrary;
  FXFT_Face face = nullptr;
  if (FXFT_New_Memory_Face(library, pData, size, face_index, &face))
    return nullptr;
  return FXFT_Set_Pixel_Sizes(face, 64, 64) ? nullptr : face;
}

FXFT_Face CFX_FontMgr::GetFileFace(const FX_CHAR* filename, int face_index) {
  InitFTLibrary();
  FXFT_Library library = m_FTLibrary;
  FXFT_Face face = nullptr;
  if (FXFT_New_Face(library, filename, face_index, &face))
    return nullptr;
  return FXFT_Set_Pixel_Sizes(face, 64, 64) ? nullptr : face;
}

void CFX_FontMgr::ReleaseFace(FXFT_Face face) {
  if (!face)
    return;
  bool bNeedFaceDone = true;
  auto it = m_FaceMap.begin();
  while (it != m_FaceMap.end()) {
    auto temp = it++;
    int nRet = temp->second->ReleaseFace(face);
    if (nRet == -1)
      continue;
    bNeedFaceDone = false;
    if (nRet == 0)
      m_FaceMap.erase(temp);
    break;
  }
  if (bNeedFaceDone && !m_pBuiltinMapper->IsBuiltinFace(face))
    FXFT_Done_Face(face);
}

bool CFX_FontMgr::GetBuiltinFont(size_t index,
                                 const uint8_t** pFontData,
                                 uint32_t* size) {
  if (index < FX_ArraySize(g_FoxitFonts)) {
    *pFontData = g_FoxitFonts[index].m_pFontData;
    *size = g_FoxitFonts[index].m_dwSize;
    return true;
  }
  index -= FX_ArraySize(g_FoxitFonts);
  if (index < FX_ArraySize(g_MMFonts)) {
    *pFontData = g_MMFonts[index].m_pFontData;
    *size = g_MMFonts[index].m_dwSize;
    return true;
  }
  return false;
}
