// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <memory>

#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/ge/cfx_folderfontinfo.h"
#include "core/fxge/ifx_systemfontinfo.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
namespace {

const size_t kLinuxGpNameSize = 6;

const FX_CHAR* const g_LinuxGpFontList[][kLinuxGpNameSize] = {
    {"TakaoPGothic", "VL PGothic", "IPAPGothic", "VL Gothic", "Kochi Gothic",
     "VL Gothic regular"},
    {"TakaoGothic", "VL Gothic", "IPAGothic", "Kochi Gothic", nullptr,
     "VL Gothic regular"},
    {"TakaoPMincho", "IPAPMincho", "VL Gothic", "Kochi Mincho", nullptr,
     "VL Gothic regular"},
    {"TakaoMincho", "IPAMincho", "VL Gothic", "Kochi Mincho", nullptr,
     "VL Gothic regular"},
};

const FX_CHAR* const g_LinuxGbFontList[] = {
    "AR PL UMing CN Light", "WenQuanYi Micro Hei", "AR PL UKai CN",
};

const FX_CHAR* const g_LinuxB5FontList[] = {
    "AR PL UMing TW Light", "WenQuanYi Micro Hei", "AR PL UKai TW",
};

const FX_CHAR* const g_LinuxHGFontList[] = {
    "UnDotum",
};

size_t GetJapanesePreference(const FX_CHAR* facearr,
                             int weight,
                             int pitch_family) {
  CFX_ByteString face = facearr;
  if (face.Find("Gothic") >= 0 ||
      face.Find("\x83\x53\x83\x56\x83\x62\x83\x4e") >= 0) {
    if (face.Find("PGothic") >= 0 ||
        face.Find("\x82\x6f\x83\x53\x83\x56\x83\x62\x83\x4e") >= 0) {
      return 0;
    }
    return 1;
  }
  if (face.Find("Mincho") >= 0 || face.Find("\x96\xbe\x92\xa9") >= 0) {
    if (face.Find("PMincho") >= 0 ||
        face.Find("\x82\x6f\x96\xbe\x92\xa9") >= 0) {
      return 2;
    }
    return 3;
  }
  if (!(pitch_family & FXFONT_FF_ROMAN) && weight > 400)
    return 0;

  return 2;
}

class CFX_LinuxFontInfo : public CFX_FolderFontInfo {
 public:
  CFX_LinuxFontInfo() {}
  ~CFX_LinuxFontInfo() override {}

  void* MapFont(int weight,
                bool bItalic,
                int charset,
                int pitch_family,
                const FX_CHAR* family,
                int& iExact) override;
  bool ParseFontCfg(const char** pUserPaths);
};

void* CFX_LinuxFontInfo::MapFont(int weight,
                                 bool bItalic,
                                 int charset,
                                 int pitch_family,
                                 const FX_CHAR* cstr_face,
                                 int& iExact) {
  void* font = GetSubstFont(cstr_face);
  if (font) {
    iExact = 1;
    return font;
  }
  bool bCJK = true;
  switch (charset) {
    case FXFONT_SHIFTJIS_CHARSET: {
      size_t index = GetJapanesePreference(cstr_face, weight, pitch_family);
      ASSERT(index < FX_ArraySize(g_LinuxGpFontList));
      for (size_t i = 0; i < kLinuxGpNameSize; i++) {
        auto it = m_FontList.find(g_LinuxGpFontList[index][i]);
        if (it != m_FontList.end())
          return it->second;
      }
      break;
    }
    case FXFONT_GB2312_CHARSET: {
      for (size_t i = 0; i < FX_ArraySize(g_LinuxGbFontList); ++i) {
        auto it = m_FontList.find(g_LinuxGbFontList[i]);
        if (it != m_FontList.end())
          return it->second;
      }
      break;
    }
    case FXFONT_CHINESEBIG5_CHARSET: {
      for (size_t i = 0; i < FX_ArraySize(g_LinuxB5FontList); ++i) {
        auto it = m_FontList.find(g_LinuxB5FontList[i]);
        if (it != m_FontList.end())
          return it->second;
      }
      break;
    }
    case FXFONT_HANGUL_CHARSET: {
      for (size_t i = 0; i < FX_ArraySize(g_LinuxHGFontList); ++i) {
        auto it = m_FontList.find(g_LinuxHGFontList[i]);
        if (it != m_FontList.end())
          return it->second;
      }
      break;
    }
    default:
      bCJK = false;
      break;
  }
  return FindFont(weight, bItalic, charset, pitch_family, cstr_face, !bCJK);
}

bool CFX_LinuxFontInfo::ParseFontCfg(const char** pUserPaths) {
  if (!pUserPaths)
    return false;

  for (const char** pPath = pUserPaths; *pPath; ++pPath)
    AddPath(*pPath);
  return true;
}

}  // namespace

std::unique_ptr<IFX_SystemFontInfo> IFX_SystemFontInfo::CreateDefault(
    const char** pUserPaths) {
  CFX_LinuxFontInfo* pInfo = new CFX_LinuxFontInfo;
  if (!pInfo->ParseFontCfg(pUserPaths)) {
    pInfo->AddPath("/usr/share/fonts");
    pInfo->AddPath("/usr/share/X11/fonts/Type1");
    pInfo->AddPath("/usr/share/X11/fonts/TTF");
    pInfo->AddPath("/usr/local/share/fonts");
  }
  return std::unique_ptr<IFX_SystemFontInfo>(pInfo);
}

void CFX_GEModule::InitPlatform() {
  m_pFontMgr->SetSystemFontInfo(
      IFX_SystemFontInfo::CreateDefault(m_pUserFontPaths));
}

void CFX_GEModule::DestroyPlatform() {}
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
