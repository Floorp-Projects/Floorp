// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_fontcache.h"

#include <memory>
#include <utility>

#include "core/fxge/cfx_facecache.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/fx_freetype.h"

CFX_FontCache::CountedFaceCache::CountedFaceCache() {}

CFX_FontCache::CountedFaceCache::~CountedFaceCache() {}

CFX_FontCache::CFX_FontCache() {}

CFX_FontCache::~CFX_FontCache() {
  ASSERT(m_ExtFaceMap.empty());
  ASSERT(m_FTFaceMap.empty());
}

CFX_FaceCache* CFX_FontCache::GetCachedFace(const CFX_Font* pFont) {
  FXFT_Face face = pFont->GetFace();
  const bool bExternal = !face;
  CFX_FTCacheMap& map = bExternal ? m_ExtFaceMap : m_FTFaceMap;
  auto it = map.find(face);
  if (it != map.end()) {
    CountedFaceCache* counted_face_cache = it->second.get();
    counted_face_cache->m_nCount++;
    return counted_face_cache->m_Obj.get();
  }

  std::unique_ptr<CountedFaceCache> counted_face_cache(new CountedFaceCache);
  counted_face_cache->m_nCount = 2;
  CFX_FaceCache* face_cache = new CFX_FaceCache(bExternal ? nullptr : face);
  counted_face_cache->m_Obj.reset(face_cache);
  map[face] = std::move(counted_face_cache);
  return face_cache;
}

#ifdef _SKIA_SUPPORT_
CFX_TypeFace* CFX_FontCache::GetDeviceCache(const CFX_Font* pFont) {
  return GetCachedFace(pFont)->GetDeviceCache(pFont);
}
#endif

void CFX_FontCache::ReleaseCachedFace(const CFX_Font* pFont) {
  FXFT_Face face = pFont->GetFace();
  const bool bExternal = !face;
  CFX_FTCacheMap& map = bExternal ? m_ExtFaceMap : m_FTFaceMap;

  auto it = map.find(face);
  if (it == map.end())
    return;

  CountedFaceCache* counted_face_cache = it->second.get();
  if (counted_face_cache->m_nCount > 2) {
    counted_face_cache->m_nCount--;
  } else {
    map.erase(it);
  }
}
