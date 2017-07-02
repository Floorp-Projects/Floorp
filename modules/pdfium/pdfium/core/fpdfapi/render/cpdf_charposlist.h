// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_CHARPOSLIST_H_
#define CORE_FPDFAPI_RENDER_CPDF_CHARPOSLIST_H_

#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_renderdevice.h"

class CPDF_Font;

class CPDF_CharPosList {
 public:
  CPDF_CharPosList();
  ~CPDF_CharPosList();
  void Load(const std::vector<uint32_t>& charCodes,
            const std::vector<FX_FLOAT>& charPos,
            CPDF_Font* pFont,
            FX_FLOAT font_size);
  FXTEXT_CHARPOS* m_pCharPos;
  uint32_t m_nChars;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_CHARPOSLIST_H_
