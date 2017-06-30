// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_GEMODULE_H_
#define CORE_FXGE_CFX_GEMODULE_H_

#include <memory>

#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/fx_font.h"

class CCodec_ModuleMgr;
class CFX_FontCache;
class CFX_FontMgr;

class CFX_GEModule {
 public:
  static CFX_GEModule* Get();
  static void Destroy();

  void Init(const char** pUserFontPaths, CCodec_ModuleMgr* pCodecModule);
  CFX_FontCache* GetFontCache();
  CFX_FontMgr* GetFontMgr() { return m_pFontMgr.get(); }
  void SetTextGamma(FX_FLOAT gammaValue);
  const uint8_t* GetTextGammaTable() const;

  CCodec_ModuleMgr* GetCodecModule() { return m_pCodecModule; }
  void* GetPlatformData() { return m_pPlatformData; }

  FXFT_Library m_FTLibrary;

 private:
  CFX_GEModule();
  ~CFX_GEModule();

  void InitPlatform();
  void DestroyPlatform();

  uint8_t m_GammaValue[256];
  CFX_FontCache* m_pFontCache;
  std::unique_ptr<CFX_FontMgr> m_pFontMgr;
  CCodec_ModuleMgr* m_pCodecModule;
  void* m_pPlatformData;
  const char** m_pUserFontPaths;
};

#endif  // CORE_FXGE_CFX_GEMODULE_H_
