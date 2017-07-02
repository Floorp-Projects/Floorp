// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONTCACHE_H_
#define CORE_FXGE_CFX_FONTCACHE_H_

#include <map>
#include <memory>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/fx_freetype.h"

class CFX_FaceCache;

class CFX_FontCache {
 public:
  CFX_FontCache();
  ~CFX_FontCache();
  CFX_FaceCache* GetCachedFace(const CFX_Font* pFont);
  void ReleaseCachedFace(const CFX_Font* pFont);
#ifdef _SKIA_SUPPORT_
  CFX_TypeFace* GetDeviceCache(const CFX_Font* pFont);
#endif

 private:
  struct CountedFaceCache {
    CountedFaceCache();
    ~CountedFaceCache();
    std::unique_ptr<CFX_FaceCache> m_Obj;
    uint32_t m_nCount;
  };

  using CFX_FTCacheMap = std::map<FXFT_Face, std::unique_ptr<CountedFaceCache>>;
  CFX_FTCacheMap m_FTFaceMap;
  CFX_FTCacheMap m_ExtFaceMap;
};

#endif  // CORE_FXGE_CFX_FONTCACHE_H_
