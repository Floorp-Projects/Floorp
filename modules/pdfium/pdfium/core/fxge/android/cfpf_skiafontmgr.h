// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_
#define CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_

#include <map>
#include <vector>

#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxge/fx_font.h"

#define FPF_MATCHFONT_REPLACEANSI 1

class CFPF_SkiaFont;
class CFPF_SkiaFontDescriptor;

class CFPF_SkiaFontMgr {
 public:
  CFPF_SkiaFontMgr();
  ~CFPF_SkiaFontMgr();

  void LoadSystemFonts();
  CFPF_SkiaFont* CreateFont(const CFX_ByteStringC& bsFamilyname,
                            uint8_t uCharset,
                            uint32_t dwStyle,
                            uint32_t dwMatch = 0);

  bool InitFTLibrary();
  FXFT_Face GetFontFace(const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead,
                        int32_t iFaceIndex = 0);
  FXFT_Face GetFontFace(const CFX_ByteStringC& bsFile, int32_t iFaceIndex = 0);
  FXFT_Face GetFontFace(const uint8_t* pBuffer,
                        size_t szBuffer,
                        int32_t iFaceIndex = 0);

 private:
  void ScanPath(const CFX_ByteString& path);
  void ScanFile(const CFX_ByteString& file);
  void ReportFace(FXFT_Face face, CFPF_SkiaFontDescriptor* pFontDesc);

  bool m_bLoaded;
  FXFT_Library m_FTLibrary;
  std::vector<CFPF_SkiaFontDescriptor*> m_FontFaces;
  std::map<uint32_t, CFPF_SkiaFont*> m_FamilyFonts;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_
