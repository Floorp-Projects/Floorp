// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/android/cfpf_skiafontmgr.h"

#define FPF_SKIAMATCHWEIGHT_NAME1 62
#define FPF_SKIAMATCHWEIGHT_NAME2 60
#define FPF_SKIAMATCHWEIGHT_1 16
#define FPF_SKIAMATCHWEIGHT_2 8

#include <algorithm>

#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/android/cfpf_skiafont.h"
#include "core/fxge/android/cfpf_skiafontdescriptor.h"
#include "core/fxge/android/cfpf_skiapathfont.h"
#include "core/fxge/fx_freetype.h"

#ifdef __cplusplus
extern "C" {
#endif
static unsigned long FPF_SkiaStream_Read(FXFT_Stream stream,
                                         unsigned long offset,
                                         unsigned char* buffer,
                                         unsigned long count) {
  if (count == 0)
    return 0;

  IFX_SeekableReadStream* pFileRead =
      static_cast<IFX_SeekableReadStream*>(stream->descriptor.pointer);
  if (!pFileRead)
    return 0;

  if (!pFileRead->ReadBlock(buffer, (FX_FILESIZE)offset, (size_t)count))
    return 0;

  return count;
}

static void FPF_SkiaStream_Close(FXFT_Stream stream) {}
#ifdef __cplusplus
};
#endif

namespace {

struct FPF_SKIAFONTMAP {
  uint32_t dwFamily;
  uint32_t dwSubSt;
};

const FPF_SKIAFONTMAP g_SkiaFontmap[] = {
    {0x58c5083, 0xc8d2e345},  {0x5dfade2, 0xe1633081},
    {0x684317d, 0xe1633081},  {0x14ee2d13, 0xc8d2e345},
    {0x3918fe2d, 0xbbeeec72}, {0x3b98b31c, 0xe1633081},
    {0x3d49f40e, 0xe1633081}, {0x432c41c5, 0xe1633081},
    {0x491b6ad0, 0xe1633081}, {0x5612cab1, 0x59b9f8f1},
    {0x779ce19d, 0xc8d2e345}, {0x7cc9510b, 0x59b9f8f1},
    {0x83746053, 0xbbeeec72}, {0xaaa60c03, 0xbbeeec72},
    {0xbf85ff26, 0xe1633081}, {0xc04fe601, 0xbbeeec72},
    {0xca3812d5, 0x59b9f8f1}, {0xca383e15, 0x59b9f8f1},
    {0xcad5eaf6, 0x59b9f8f1}, {0xcb7a04c8, 0xc8d2e345},
    {0xfb4ce0de, 0xe1633081},
};

const FPF_SKIAFONTMAP g_SkiaSansFontMap[] = {
    {0x58c5083, 0xd5b8d10f},  {0x14ee2d13, 0xd5b8d10f},
    {0x779ce19d, 0xd5b8d10f}, {0xcb7a04c8, 0xd5b8d10f},
    {0xfb4ce0de, 0xd5b8d10f},
};

uint32_t FPF_SkiaGetSubstFont(uint32_t dwHash,
                              const FPF_SKIAFONTMAP* skFontMap,
                              size_t length) {
  const FPF_SKIAFONTMAP* pEnd = skFontMap + length;
  const FPF_SKIAFONTMAP* pFontMap = std::lower_bound(
      skFontMap, pEnd, dwHash, [](const FPF_SKIAFONTMAP& item, uint32_t hash) {
        return item.dwFamily < hash;
      });
  if (pFontMap < pEnd && pFontMap->dwFamily == dwHash)
    return pFontMap->dwSubSt;
  return 0;
}

uint32_t FPF_GetHashCode_StringA(const FX_CHAR* pStr, int32_t iLength) {
  if (!pStr)
    return 0;
  if (iLength < 0)
    iLength = FXSYS_strlen(pStr);
  const FX_CHAR* pStrEnd = pStr + iLength;
  uint32_t uHashCode = 0;
  while (pStr < pStrEnd)
    uHashCode = 31 * uHashCode + FXSYS_tolower(*pStr++);
  return uHashCode;
}

enum FPF_SKIACHARSET {
  FPF_SKIACHARSET_Ansi = 1 << 0,
  FPF_SKIACHARSET_Default = 1 << 1,
  FPF_SKIACHARSET_Symbol = 1 << 2,
  FPF_SKIACHARSET_ShiftJIS = 1 << 3,
  FPF_SKIACHARSET_Korean = 1 << 4,
  FPF_SKIACHARSET_Johab = 1 << 5,
  FPF_SKIACHARSET_GB2312 = 1 << 6,
  FPF_SKIACHARSET_BIG5 = 1 << 7,
  FPF_SKIACHARSET_Greek = 1 << 8,
  FPF_SKIACHARSET_Turkish = 1 << 9,
  FPF_SKIACHARSET_Vietnamese = 1 << 10,
  FPF_SKIACHARSET_Hebrew = 1 << 11,
  FPF_SKIACHARSET_Arabic = 1 << 12,
  FPF_SKIACHARSET_Baltic = 1 << 13,
  FPF_SKIACHARSET_Cyrillic = 1 << 14,
  FPF_SKIACHARSET_Thai = 1 << 15,
  FPF_SKIACHARSET_EeasternEuropean = 1 << 16,
  FPF_SKIACHARSET_PC = 1 << 17,
  FPF_SKIACHARSET_OEM = 1 << 18,
};

uint32_t FPF_SkiaGetCharset(uint8_t uCharset) {
  switch (uCharset) {
    case FXFONT_ANSI_CHARSET:
      return FPF_SKIACHARSET_Ansi;
    case FXFONT_DEFAULT_CHARSET:
      return FPF_SKIACHARSET_Default;
    case FXFONT_SYMBOL_CHARSET:
      return FPF_SKIACHARSET_Symbol;
    case FXFONT_SHIFTJIS_CHARSET:
      return FPF_SKIACHARSET_ShiftJIS;
    case FXFONT_HANGUL_CHARSET:
      return FPF_SKIACHARSET_Korean;
    case FXFONT_GB2312_CHARSET:
      return FPF_SKIACHARSET_GB2312;
    case FXFONT_CHINESEBIG5_CHARSET:
      return FPF_SKIACHARSET_BIG5;
    case FXFONT_GREEK_CHARSET:
      return FPF_SKIACHARSET_Greek;
    case FXFONT_TURKISH_CHARSET:
      return FPF_SKIACHARSET_Turkish;
    case FXFONT_HEBREW_CHARSET:
      return FPF_SKIACHARSET_Hebrew;
    case FXFONT_ARABIC_CHARSET:
      return FPF_SKIACHARSET_Arabic;
    case FXFONT_BALTIC_CHARSET:
      return FPF_SKIACHARSET_Baltic;
    case FXFONT_RUSSIAN_CHARSET:
      return FPF_SKIACHARSET_Cyrillic;
    case FXFONT_THAI_CHARSET:
      return FPF_SKIACHARSET_Thai;
    case FXFONT_EASTEUROPE_CHARSET:
      return FPF_SKIACHARSET_EeasternEuropean;
  }
  return FPF_SKIACHARSET_Default;
}

uint32_t FPF_SKIANormalizeFontName(const CFX_ByteStringC& bsfamily) {
  uint32_t dwHash = 0;
  int32_t iLength = bsfamily.GetLength();
  const FX_CHAR* pBuffer = bsfamily.c_str();
  for (int32_t i = 0; i < iLength; i++) {
    FX_CHAR ch = pBuffer[i];
    if (ch == ' ' || ch == '-' || ch == ',')
      continue;
    dwHash = 31 * dwHash + FXSYS_tolower(ch);
  }
  return dwHash;
}

uint32_t FPF_SKIAGetFamilyHash(const CFX_ByteStringC& bsFamily,
                               uint32_t dwStyle,
                               uint8_t uCharset) {
  CFX_ByteString bsFont(bsFamily);
  if (dwStyle & FXFONT_BOLD)
    bsFont += "Bold";
  if (dwStyle & FXFONT_ITALIC)
    bsFont += "Italic";
  if (dwStyle & FXFONT_SERIF)
    bsFont += "Serif";
  bsFont += uCharset;
  return FPF_GetHashCode_StringA(bsFont.c_str(), bsFont.GetLength());
}

bool FPF_SkiaIsCJK(uint8_t uCharset) {
  return (uCharset == FXFONT_GB2312_CHARSET) ||
         (uCharset == FXFONT_CHINESEBIG5_CHARSET) ||
         (uCharset == FXFONT_HANGUL_CHARSET) ||
         (uCharset == FXFONT_SHIFTJIS_CHARSET);
}

bool FPF_SkiaMaybeSymbol(const CFX_ByteStringC& bsFacename) {
  CFX_ByteString bsName(bsFacename);
  bsName.MakeLower();
  return bsName.Find("symbol") > -1;
}

bool FPF_SkiaMaybeArabic(const CFX_ByteStringC& bsFacename) {
  CFX_ByteString bsName(bsFacename);
  bsName.MakeLower();
  return bsName.Find("arabic") > -1;
}

const uint32_t g_FPFSkiaFontCharsets[] = {
    FPF_SKIACHARSET_Ansi,
    FPF_SKIACHARSET_EeasternEuropean,
    FPF_SKIACHARSET_Cyrillic,
    FPF_SKIACHARSET_Greek,
    FPF_SKIACHARSET_Turkish,
    FPF_SKIACHARSET_Hebrew,
    FPF_SKIACHARSET_Arabic,
    FPF_SKIACHARSET_Baltic,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    FPF_SKIACHARSET_Thai,
    FPF_SKIACHARSET_ShiftJIS,
    FPF_SKIACHARSET_GB2312,
    FPF_SKIACHARSET_Korean,
    FPF_SKIACHARSET_BIG5,
    FPF_SKIACHARSET_Johab,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    FPF_SKIACHARSET_OEM,
    FPF_SKIACHARSET_Symbol,
};

uint32_t FPF_SkiaGetFaceCharset(TT_OS2* pOS2) {
  uint32_t dwCharset = 0;
  if (pOS2) {
    for (int32_t i = 0; i < 32; i++) {
      if (pOS2->ulCodePageRange1 & (1 << i))
        dwCharset |= g_FPFSkiaFontCharsets[i];
    }
  }
  dwCharset |= FPF_SKIACHARSET_Default;
  return dwCharset;
}

}  // namespace

CFPF_SkiaFontMgr::CFPF_SkiaFontMgr() : m_bLoaded(false), m_FTLibrary(nullptr) {}

CFPF_SkiaFontMgr::~CFPF_SkiaFontMgr() {
  for (const auto& pair : m_FamilyFonts) {
    if (pair.second)
      pair.second->Release();
  }
  m_FamilyFonts.clear();
  for (auto it = m_FontFaces.begin(); it != m_FontFaces.end(); ++it)
    delete *it;
  m_FontFaces.clear();
  if (m_FTLibrary)
    FXFT_Done_FreeType(m_FTLibrary);
}

bool CFPF_SkiaFontMgr::InitFTLibrary() {
  if (!m_FTLibrary)
    FXFT_Init_FreeType(&m_FTLibrary);
  return !!m_FTLibrary;
}

void CFPF_SkiaFontMgr::LoadSystemFonts() {
  if (m_bLoaded)
    return;
  ScanPath("/system/fonts");
  m_bLoaded = true;
}

CFPF_SkiaFont* CFPF_SkiaFontMgr::CreateFont(const CFX_ByteStringC& bsFamilyname,
                                            uint8_t uCharset,
                                            uint32_t dwStyle,
                                            uint32_t dwMatch) {
  uint32_t dwHash = FPF_SKIAGetFamilyHash(bsFamilyname, dwStyle, uCharset);
  auto it = m_FamilyFonts.find(dwHash);
  if (it != m_FamilyFonts.end() && it->second)
    return it->second->Retain();

  uint32_t dwFaceName = FPF_SKIANormalizeFontName(bsFamilyname);
  uint32_t dwSubst = FPF_SkiaGetSubstFont(dwFaceName, g_SkiaFontmap,
                                          FX_ArraySize(g_SkiaFontmap));
  uint32_t dwSubstSans = FPF_SkiaGetSubstFont(dwFaceName, g_SkiaSansFontMap,
                                              FX_ArraySize(g_SkiaSansFontMap));
  bool bMaybeSymbol = FPF_SkiaMaybeSymbol(bsFamilyname);
  if (uCharset != FXFONT_ARABIC_CHARSET && FPF_SkiaMaybeArabic(bsFamilyname)) {
    uCharset = FXFONT_ARABIC_CHARSET;
  } else if (uCharset == FXFONT_ANSI_CHARSET &&
             (dwMatch & FPF_MATCHFONT_REPLACEANSI)) {
    uCharset = FXFONT_DEFAULT_CHARSET;
  }
  int32_t nExpectVal = FPF_SKIAMATCHWEIGHT_NAME1 + FPF_SKIAMATCHWEIGHT_1 * 3 +
                       FPF_SKIAMATCHWEIGHT_2 * 2;
  CFPF_SkiaFontDescriptor* pBestFontDes = nullptr;
  int32_t nMax = -1;
  int32_t nGlyphNum = 0;
  for (auto it = m_FontFaces.rbegin(); it != m_FontFaces.rend(); ++it) {
    CFPF_SkiaPathFont* pFontDes = static_cast<CFPF_SkiaPathFont*>(*it);
    if (!(pFontDes->m_dwCharsets & FPF_SkiaGetCharset(uCharset)))
      continue;
    int32_t nFind = 0;
    uint32_t dwSysFontName = FPF_SKIANormalizeFontName(pFontDes->m_pFamily);
    if (dwFaceName == dwSysFontName)
      nFind += FPF_SKIAMATCHWEIGHT_NAME1;
    bool bMatchedName = (nFind == FPF_SKIAMATCHWEIGHT_NAME1);
    if ((dwStyle & FXFONT_BOLD) == (pFontDes->m_dwStyle & FXFONT_BOLD))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if ((dwStyle & FXFONT_ITALIC) == (pFontDes->m_dwStyle & FXFONT_ITALIC))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if ((dwStyle & FXFONT_FIXED_PITCH) ==
        (pFontDes->m_dwStyle & FXFONT_FIXED_PITCH)) {
      nFind += FPF_SKIAMATCHWEIGHT_2;
    }
    if ((dwStyle & FXFONT_SERIF) == (pFontDes->m_dwStyle & FXFONT_SERIF))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if ((dwStyle & FXFONT_SCRIPT) == (pFontDes->m_dwStyle & FXFONT_SCRIPT))
      nFind += FPF_SKIAMATCHWEIGHT_2;
    if (dwSubst == dwSysFontName || dwSubstSans == dwSysFontName) {
      nFind += FPF_SKIAMATCHWEIGHT_NAME2;
      bMatchedName = true;
    }
    if (uCharset == FXFONT_DEFAULT_CHARSET || bMaybeSymbol) {
      if (nFind > nMax && bMatchedName) {
        nMax = nFind;
        pBestFontDes = *it;
      }
    } else if (FPF_SkiaIsCJK(uCharset)) {
      if (bMatchedName || pFontDes->m_iGlyphNum > nGlyphNum) {
        pBestFontDes = *it;
        nGlyphNum = pFontDes->m_iGlyphNum;
      }
    } else if (nFind > nMax) {
      nMax = nFind;
      pBestFontDes = *it;
    }
    if (nExpectVal <= nFind) {
      pBestFontDes = *it;
      break;
    }
  }
  if (pBestFontDes) {
    CFPF_SkiaFont* pFont = new CFPF_SkiaFont;
    if (pFont->InitFont(this, pBestFontDes, bsFamilyname, dwStyle, uCharset)) {
      m_FamilyFonts[dwHash] = pFont;
      return pFont->Retain();
    }
    pFont->Release();
  }
  return nullptr;
}

FXFT_Face CFPF_SkiaFontMgr::GetFontFace(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead,
    int32_t iFaceIndex) {
  if (!pFileRead)
    return nullptr;
  if (pFileRead->GetSize() == 0)
    return nullptr;
  if (iFaceIndex < 0)
    return nullptr;
  FXFT_StreamRec streamRec;
  FXSYS_memset(&streamRec, 0, sizeof(FXFT_StreamRec));
  streamRec.size = pFileRead->GetSize();
  streamRec.descriptor.pointer = static_cast<void*>(pFileRead.Get());
  streamRec.read = FPF_SkiaStream_Read;
  streamRec.close = FPF_SkiaStream_Close;
  FXFT_Open_Args args;
  args.flags = FT_OPEN_STREAM;
  args.stream = &streamRec;
  FXFT_Face face;
  if (FXFT_Open_Face(m_FTLibrary, &args, iFaceIndex, &face))
    return nullptr;
  FXFT_Set_Pixel_Sizes(face, 0, 64);
  return face;
}

FXFT_Face CFPF_SkiaFontMgr::GetFontFace(const CFX_ByteStringC& bsFile,
                                        int32_t iFaceIndex) {
  if (bsFile.IsEmpty())
    return nullptr;
  if (iFaceIndex < 0)
    return nullptr;
  FXFT_Open_Args args;
  args.flags = FT_OPEN_PATHNAME;
  args.pathname = const_cast<FT_String*>(bsFile.c_str());
  FXFT_Face face;
  if (FXFT_Open_Face(m_FTLibrary, &args, iFaceIndex, &face))
    return nullptr;
  FXFT_Set_Pixel_Sizes(face, 0, 64);
  return face;
}

FXFT_Face CFPF_SkiaFontMgr::GetFontFace(const uint8_t* pBuffer,
                                        size_t szBuffer,
                                        int32_t iFaceIndex) {
  if (!pBuffer || szBuffer < 1)
    return nullptr;
  if (iFaceIndex < 0)
    return nullptr;
  FXFT_Open_Args args;
  args.flags = FT_OPEN_MEMORY;
  args.memory_base = pBuffer;
  args.memory_size = szBuffer;
  FXFT_Face face;
  if (FXFT_Open_Face(m_FTLibrary, &args, iFaceIndex, &face))
    return nullptr;
  FXFT_Set_Pixel_Sizes(face, 0, 64);
  return face;
}

void CFPF_SkiaFontMgr::ScanPath(const CFX_ByteString& path) {
  DIR* handle = FX_OpenFolder(path.c_str());
  if (!handle)
    return;
  CFX_ByteString filename;
  bool bFolder = false;
  while (FX_GetNextFile(handle, &filename, &bFolder)) {
    if (bFolder) {
      if (filename == "." || filename == "..")
        continue;
    } else {
      CFX_ByteString ext = filename.Right(4);
      ext.MakeLower();
      if (ext != ".ttf" && ext != ".ttc" && ext != ".otf")
        continue;
    }
    CFX_ByteString fullpath(path);
    fullpath += "/";
    fullpath += filename;
    if (bFolder)
      ScanPath(fullpath);
    else
      ScanFile(fullpath);
  }
  FX_CloseFolder(handle);
}

void CFPF_SkiaFontMgr::ScanFile(const CFX_ByteString& file) {
  FXFT_Face face = GetFontFace(file.AsStringC());
  if (!face)
    return;
  CFPF_SkiaPathFont* pFontDesc = new CFPF_SkiaPathFont;
  pFontDesc->SetPath(file.c_str());
  ReportFace(face, pFontDesc);
  m_FontFaces.push_back(pFontDesc);
  FXFT_Done_Face(face);
}

void CFPF_SkiaFontMgr::ReportFace(FXFT_Face face,
                                  CFPF_SkiaFontDescriptor* pFontDesc) {
  if (!face || !pFontDesc)
    return;
  pFontDesc->SetFamily(FXFT_Get_Face_Family_Name(face));
  if (FXFT_Is_Face_Bold(face))
    pFontDesc->m_dwStyle |= FXFONT_BOLD;
  if (FXFT_Is_Face_Italic(face))
    pFontDesc->m_dwStyle |= FXFONT_ITALIC;
  if (FT_IS_FIXED_WIDTH(face))
    pFontDesc->m_dwStyle |= FXFONT_FIXED_PITCH;
  TT_OS2* pOS2 = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
  if (pOS2) {
    if (pOS2->ulCodePageRange1 & (1 << 31))
      pFontDesc->m_dwStyle |= FXFONT_SYMBOLIC;
    if (pOS2->panose[0] == 2) {
      uint8_t uSerif = pOS2->panose[1];
      if ((uSerif > 1 && uSerif < 10) || uSerif > 13)
        pFontDesc->m_dwStyle |= FXFONT_SERIF;
    }
  }
  if (pOS2 && (pOS2->ulCodePageRange1 & (1 << 31)))
    pFontDesc->m_dwStyle |= FXFONT_SYMBOLIC;
  pFontDesc->m_dwCharsets = FPF_SkiaGetFaceCharset(pOS2);
  pFontDesc->m_iFaceIndex = face->face_index;
  pFontDesc->m_iGlyphNum = face->num_glyphs;
}
