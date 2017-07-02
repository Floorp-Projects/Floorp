// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_pagerendercache.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/render/cpdf_imagecacheentry.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"

namespace {

struct CACHEINFO {
  uint32_t time;
  CPDF_Stream* pStream;
};

extern "C" {
static int compare(const void* data1, const void* data2) {
  return ((CACHEINFO*)data1)->time - ((CACHEINFO*)data2)->time;
}
}  // extern "C"

}  // namespace

CPDF_PageRenderCache::CPDF_PageRenderCache(CPDF_Page* pPage)
    : m_pPage(pPage),
      m_pCurImageCacheEntry(nullptr),
      m_nTimeCount(0),
      m_nCacheSize(0),
      m_bCurFindCache(false) {}

CPDF_PageRenderCache::~CPDF_PageRenderCache() {
  for (const auto& it : m_ImageCache)
    delete it.second;
}

void CPDF_PageRenderCache::CacheOptimization(int32_t dwLimitCacheSize) {
  if (m_nCacheSize <= (uint32_t)dwLimitCacheSize)
    return;

  size_t nCount = m_ImageCache.size();
  CACHEINFO* pCACHEINFO = FX_Alloc(CACHEINFO, nCount);
  size_t i = 0;
  for (const auto& it : m_ImageCache) {
    pCACHEINFO[i].time = it.second->GetTimeCount();
    pCACHEINFO[i++].pStream = it.second->GetStream();
  }
  FXSYS_qsort(pCACHEINFO, nCount, sizeof(CACHEINFO), compare);
  uint32_t nTimeCount = m_nTimeCount;

  // Check if time value is about to roll over and reset all entries.
  // The comparision is legal because uint32_t is an unsigned type.
  if (nTimeCount + 1 < nTimeCount) {
    for (i = 0; i < nCount; i++)
      m_ImageCache[pCACHEINFO[i].pStream]->m_dwTimeCount = i;
    m_nTimeCount = nCount;
  }

  i = 0;
  while (i + 15 < nCount)
    ClearImageCacheEntry(pCACHEINFO[i++].pStream);

  while (i < nCount && m_nCacheSize > (uint32_t)dwLimitCacheSize)
    ClearImageCacheEntry(pCACHEINFO[i++].pStream);

  FX_Free(pCACHEINFO);
}

void CPDF_PageRenderCache::ClearImageCacheEntry(CPDF_Stream* pStream) {
  auto it = m_ImageCache.find(pStream);
  if (it == m_ImageCache.end())
    return;

  m_nCacheSize -= it->second->EstimateSize();
  delete it->second;
  m_ImageCache.erase(it);
}

bool CPDF_PageRenderCache::StartGetCachedBitmap(
    CPDF_Stream* pStream,
    bool bStdCS,
    uint32_t GroupFamily,
    bool bLoadMask,
    CPDF_RenderStatus* pRenderStatus,
    int32_t downsampleWidth,
    int32_t downsampleHeight) {
  const auto it = m_ImageCache.find(pStream);
  m_bCurFindCache = it != m_ImageCache.end();
  if (m_bCurFindCache) {
    m_pCurImageCacheEntry = it->second;
  } else {
    m_pCurImageCacheEntry =
        new CPDF_ImageCacheEntry(m_pPage->m_pDocument, pStream);
  }
  int ret = m_pCurImageCacheEntry->StartGetCachedBitmap(
      pRenderStatus->m_pFormResource, m_pPage->m_pPageResources, bStdCS,
      GroupFamily, bLoadMask, pRenderStatus, downsampleWidth, downsampleHeight);
  if (ret == 2)
    return true;

  m_nTimeCount++;
  if (!m_bCurFindCache)
    m_ImageCache[pStream] = m_pCurImageCacheEntry;

  if (!ret)
    m_nCacheSize += m_pCurImageCacheEntry->EstimateSize();

  return false;
}

bool CPDF_PageRenderCache::Continue(IFX_Pause* pPause) {
  int ret = m_pCurImageCacheEntry->Continue(pPause);
  if (ret == 2)
    return true;

  m_nTimeCount++;
  if (!m_bCurFindCache)
    m_ImageCache[m_pCurImageCacheEntry->GetStream()] = m_pCurImageCacheEntry;
  if (!ret)
    m_nCacheSize += m_pCurImageCacheEntry->EstimateSize();
  return false;
}

void CPDF_PageRenderCache::ResetBitmap(CPDF_Stream* pStream,
                                       const CFX_DIBitmap* pBitmap) {
  CPDF_ImageCacheEntry* pEntry;
  const auto it = m_ImageCache.find(pStream);
  if (it == m_ImageCache.end()) {
    if (!pBitmap)
      return;

    pEntry = new CPDF_ImageCacheEntry(m_pPage->m_pDocument, pStream);
    m_ImageCache[pStream] = pEntry;
  } else {
    pEntry = it->second;
  }
  m_nCacheSize -= pEntry->EstimateSize();
  pEntry->Reset(pBitmap);
  m_nCacheSize += pEntry->EstimateSize();
}
