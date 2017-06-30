// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <memory>

#include "core/fxge/apple/apple_int.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/ge/cfx_folderfontinfo.h"
#include "core/fxge/ifx_systemfontinfo.h"

namespace {

const struct {
  const FX_CHAR* m_pName;
  const FX_CHAR* m_pSubstName;
} g_Base14Substs[] = {
    {"Courier", "Courier New"},
    {"Courier-Bold", "Courier New Bold"},
    {"Courier-BoldOblique", "Courier New Bold Italic"},
    {"Courier-Oblique", "Courier New Italic"},
    {"Helvetica", "Arial"},
    {"Helvetica-Bold", "Arial Bold"},
    {"Helvetica-BoldOblique", "Arial Bold Italic"},
    {"Helvetica-Oblique", "Arial Italic"},
    {"Times-Roman", "Times New Roman"},
    {"Times-Bold", "Times New Roman Bold"},
    {"Times-BoldItalic", "Times New Roman Bold Italic"},
    {"Times-Italic", "Times New Roman Italic"},
};

class CFX_MacFontInfo : public CFX_FolderFontInfo {
 public:
  CFX_MacFontInfo() {}
  ~CFX_MacFontInfo() override {}

  // CFX_FolderFontInfo
  void* MapFont(int weight,
                bool bItalic,
                int charset,
                int pitch_family,
                const FX_CHAR* family,
                int& iExact) override;
};

const char JAPAN_GOTHIC[] = "Hiragino Kaku Gothic Pro W6";
const char JAPAN_MINCHO[] = "Hiragino Mincho Pro W6";

void GetJapanesePreference(CFX_ByteString* face, int weight, int pitch_family) {
  if (face->Find("Gothic") >= 0) {
    *face = JAPAN_GOTHIC;
    return;
  }
  *face = ((pitch_family & FXFONT_FF_ROMAN) || weight <= 400) ? JAPAN_MINCHO
                                                              : JAPAN_GOTHIC;
}

void* CFX_MacFontInfo::MapFont(int weight,
                               bool bItalic,
                               int charset,
                               int pitch_family,
                               const FX_CHAR* cstr_face,
                               int& iExact) {
  CFX_ByteString face = cstr_face;
  for (size_t i = 0; i < FX_ArraySize(g_Base14Substs); ++i) {
    if (face == CFX_ByteStringC(g_Base14Substs[i].m_pName)) {
      face = g_Base14Substs[i].m_pSubstName;
      iExact = true;
      return GetFont(face.c_str());
    }
  }

  // The request may not ask for the bold and/or italic version of a font by
  // name. So try to construct the appropriate name. This is not 100% foolproof
  // as there are fonts that have "Oblique" or "BoldOblique" or "Heavy" in their
  // names instead. But this at least works for common fonts like Arial and
  // Times New Roman. A more sophisticated approach would be to find all the
  // fonts in |m_FontList| with |face| in the name, and examine the fonts to
  // see which best matches the requested characteristics.
  if (face.Find("Bold") == -1 && face.Find("Italic") == -1) {
    CFX_ByteString new_face = face;
    if (weight > 400)
      new_face += " Bold";
    if (bItalic)
      new_face += " Italic";
    auto it = m_FontList.find(new_face);
    if (it != m_FontList.end())
      return it->second;
  }

  auto it = m_FontList.find(face);
  if (it != m_FontList.end())
    return it->second;

  if (charset == FXFONT_ANSI_CHARSET && (pitch_family & FXFONT_FF_FIXEDPITCH))
    return GetFont("Courier New");

  if (charset == FXFONT_ANSI_CHARSET || charset == FXFONT_SYMBOL_CHARSET)
    return nullptr;

  switch (charset) {
    case FXFONT_SHIFTJIS_CHARSET:
      GetJapanesePreference(&face, weight, pitch_family);
      break;
    case FXFONT_GB2312_CHARSET:
      face = "STSong";
      break;
    case FXFONT_HANGUL_CHARSET:
      face = "AppleMyungjo";
      break;
    case FXFONT_CHINESEBIG5_CHARSET:
      face = "LiSong Pro Light";
  }
  it = m_FontList.find(face);
  return it != m_FontList.end() ? it->second : nullptr;
}

}  // namespace

std::unique_ptr<IFX_SystemFontInfo> IFX_SystemFontInfo::CreateDefault(
    const char** pUnused) {
  CFX_MacFontInfo* pInfo(new CFX_MacFontInfo);
  pInfo->AddPath("~/Library/Fonts");
  pInfo->AddPath("/Library/Fonts");
  pInfo->AddPath("/System/Library/Fonts");
  return std::unique_ptr<CFX_MacFontInfo>(pInfo);
}

void CFX_GEModule::InitPlatform() {
  m_pPlatformData = new CApplePlatform;
  m_pFontMgr->SetSystemFontInfo(IFX_SystemFontInfo::CreateDefault(nullptr));
}

void CFX_GEModule::DestroyPlatform() {
  delete reinterpret_cast<CApplePlatform*>(m_pPlatformData);
  m_pPlatformData = nullptr;
}
