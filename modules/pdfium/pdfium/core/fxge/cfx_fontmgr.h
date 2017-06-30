// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONTMGR_H_
#define CORE_FXGE_CFX_FONTMGR_H_

#include <map>
#include <memory>

#include "core/fxge/fx_font.h"

class IFX_SystemFontInfo;
class CFX_FontMapper;
class CFX_SubstFont;
class CTTFontDesc;

class CFX_FontMgr {
 public:
  CFX_FontMgr();
  ~CFX_FontMgr();

  void InitFTLibrary();

  FXFT_Face GetCachedFace(const CFX_ByteString& face_name,
                          int weight,
                          bool bItalic,
                          uint8_t*& pFontData);
  FXFT_Face AddCachedFace(const CFX_ByteString& face_name,
                          int weight,
                          bool bItalic,
                          uint8_t* pData,
                          uint32_t size,
                          int face_index);
  FXFT_Face GetCachedTTCFace(int ttc_size,
                             uint32_t checksum,
                             int font_offset,
                             uint8_t*& pFontData);
  FXFT_Face AddCachedTTCFace(int ttc_size,
                             uint32_t checksum,
                             uint8_t* pData,
                             uint32_t size,
                             int font_offset);
  FXFT_Face GetFileFace(const FX_CHAR* filename, int face_index);
  FXFT_Face GetFixedFace(const uint8_t* pData, uint32_t size, int face_index);
  void ReleaseFace(FXFT_Face face);
  void SetSystemFontInfo(std::unique_ptr<IFX_SystemFontInfo> pFontInfo);
  FXFT_Face FindSubstFont(const CFX_ByteString& face_name,
                          bool bTrueType,
                          uint32_t flags,
                          int weight,
                          int italic_angle,
                          int CharsetCP,
                          CFX_SubstFont* pSubstFont);
  bool GetBuiltinFont(size_t index, const uint8_t** pFontData, uint32_t* size);
  CFX_FontMapper* GetBuiltinMapper() const { return m_pBuiltinMapper.get(); }
  FXFT_Library GetFTLibrary() const { return m_FTLibrary; }
  bool FTLibrarySupportsHinting() const { return m_FTLibrarySupportsHinting; }

 private:
  std::unique_ptr<CFX_FontMapper> m_pBuiltinMapper;
  std::map<CFX_ByteString, CTTFontDesc*> m_FaceMap;
  FXFT_Library m_FTLibrary;
  bool m_FTLibrarySupportsHinting;
};

#endif  // CORE_FXGE_CFX_FONTMGR_H_
