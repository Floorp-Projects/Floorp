// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_CMAPS_CMAP_INT_H_
#define CORE_FPDFAPI_CMAPS_CMAP_INT_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

struct FXCMAP_CMap {
  enum MapType { None, Single, Range, Reverse };

  const char* m_Name;
  MapType m_WordMapType;
  const uint16_t* m_pWordMap;
  int m_WordCount;
  MapType m_DWordMapType;
  const uint16_t* m_pDWordMap;
  int m_DWordCount;
  int m_UseOffset;
};

void FPDFAPI_FindEmbeddedCMap(const CFX_ByteString& name,
                              int charset,
                              int coding,
                              const FXCMAP_CMap*& pMap);
uint16_t FPDFAPI_CIDFromCharCode(const FXCMAP_CMap* pMap, uint32_t charcode);
uint32_t FPDFAPI_CharCodeFromCID(const FXCMAP_CMap* pMap, uint16_t cid);

#endif  // CORE_FPDFAPI_CMAPS_CMAP_INT_H_
