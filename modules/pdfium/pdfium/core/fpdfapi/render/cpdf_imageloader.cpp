// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_imageloader.h"

#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_imagecacheentry.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"
#include "core/fxcrt/fx_basic.h"

CPDF_ImageLoader::CPDF_ImageLoader()
    : m_pBitmap(nullptr),
      m_pMask(nullptr),
      m_MatteColor(0),
      m_bCached(false),
      m_nDownsampleWidth(0),
      m_nDownsampleHeight(0),
      m_pCache(nullptr),
      m_pImage(nullptr) {}

CPDF_ImageLoader::~CPDF_ImageLoader() {
  if (!m_bCached) {
    delete m_pBitmap;
    delete m_pMask;
  }
}

bool CPDF_ImageLoader::Start(const CPDF_ImageObject* pImage,
                             CPDF_PageRenderCache* pCache,
                             bool bStdCS,
                             uint32_t GroupFamily,
                             bool bLoadMask,
                             CPDF_RenderStatus* pRenderStatus,
                             int32_t nDownsampleWidth,
                             int32_t nDownsampleHeight) {
  m_nDownsampleWidth = nDownsampleWidth;
  m_nDownsampleHeight = nDownsampleHeight;
  m_pCache = pCache;
  m_pImage = const_cast<CPDF_ImageObject*>(pImage);
  bool ret;
  if (pCache) {
    ret = pCache->StartGetCachedBitmap(
        m_pImage->GetImage()->GetStream(), bStdCS, GroupFamily, bLoadMask,
        pRenderStatus, m_nDownsampleWidth, m_nDownsampleHeight);
  } else {
    ret = m_pImage->GetImage()->StartLoadDIBSource(
        pRenderStatus->m_pFormResource, pRenderStatus->m_pPageResource, bStdCS,
        GroupFamily, bLoadMask);
  }
  if (!ret)
    HandleFailure();
  return ret;
}

bool CPDF_ImageLoader::Continue(IFX_Pause* pPause) {
  bool ret = m_pCache ? m_pCache->Continue(pPause)
                      : m_pImage->GetImage()->Continue(pPause);
  if (!ret)
    HandleFailure();
  return ret;
}

void CPDF_ImageLoader::HandleFailure() {
  if (m_pCache) {
    CPDF_ImageCacheEntry* entry = m_pCache->GetCurImageCacheEntry();
    m_bCached = true;
    m_pBitmap = entry->DetachBitmap();
    m_pMask = entry->DetachMask();
    m_MatteColor = entry->m_MatteColor;
    return;
  }
  CPDF_Image* pImage = m_pImage->GetImage();
  m_bCached = false;
  m_pBitmap = pImage->DetachBitmap();
  m_pMask = pImage->DetachMask();
  m_MatteColor = pImage->m_MatteColor;
}
