// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_IMAGECACHEENTRY_H_
#define CORE_FPDFAPI_RENDER_CPDF_IMAGECACHEENTRY_H_

#include <memory>

#include "core/fxcrt/fx_system.h"

class CFX_DIBitmap;
class CFX_DIBSource;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_RenderStatus;
class CPDF_Stream;
class IFX_Pause;

class CPDF_ImageCacheEntry {
 public:
  CPDF_ImageCacheEntry(CPDF_Document* pDoc, CPDF_Stream* pStream);
  ~CPDF_ImageCacheEntry();

  void Reset(const CFX_DIBitmap* pBitmap);
  uint32_t EstimateSize() const { return m_dwCacheSize; }
  uint32_t GetTimeCount() const { return m_dwTimeCount; }
  CPDF_Stream* GetStream() const { return m_pStream; }

  int StartGetCachedBitmap(CPDF_Dictionary* pFormResources,
                           CPDF_Dictionary* pPageResources,
                           bool bStdCS,
                           uint32_t GroupFamily,
                           bool bLoadMask,
                           CPDF_RenderStatus* pRenderStatus,
                           int32_t downsampleWidth,
                           int32_t downsampleHeight);
  int Continue(IFX_Pause* pPause);
  CFX_DIBSource* DetachBitmap();
  CFX_DIBSource* DetachMask();

  int m_dwTimeCount;
  uint32_t m_MatteColor;

 private:
  void ContinueGetCachedBitmap();
  void CalcSize();

  CPDF_RenderStatus* m_pRenderStatus;
  CPDF_Document* m_pDocument;
  CPDF_Stream* m_pStream;
  CFX_DIBSource* m_pCurBitmap;
  CFX_DIBSource* m_pCurMask;
  std::unique_ptr<CFX_DIBSource> m_pCachedBitmap;
  std::unique_ptr<CFX_DIBSource> m_pCachedMask;
  uint32_t m_dwCacheSize;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_IMAGECACHEENTRY_H_
