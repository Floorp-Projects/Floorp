// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFX_ANDROIDFONTINFO_H_
#define CORE_FXGE_ANDROID_CFX_ANDROIDFONTINFO_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/ifx_systemfontinfo.h"

class CFPF_SkiaFontMgr;

class CFX_AndroidFontInfo : public IFX_SystemFontInfo {
 public:
  CFX_AndroidFontInfo();
  ~CFX_AndroidFontInfo() override;

  bool Init(CFPF_SkiaFontMgr* pFontMgr);

  // IFX_SystemFontInfo:
  bool EnumFontList(CFX_FontMapper* pMapper) override;
  void* MapFont(int weight,
                bool bItalic,
                int charset,
                int pitch_family,
                const FX_CHAR* face,
                int& bExact) override;
  void* GetFont(const FX_CHAR* face) override;
  uint32_t GetFontData(void* hFont,
                       uint32_t table,
                       uint8_t* buffer,
                       uint32_t size) override;
  bool GetFaceName(void* hFont, CFX_ByteString& name) override;
  bool GetFontCharset(void* hFont, int& charset) override;
  void DeleteFont(void* hFont) override;

 protected:
  CFPF_SkiaFontMgr* m_pFontMgr;
};

#endif  // CORE_FXGE_ANDROID_CFX_ANDROIDFONTINFO_H_
