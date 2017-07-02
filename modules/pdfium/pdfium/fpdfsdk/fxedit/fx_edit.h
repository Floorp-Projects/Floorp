// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FXEDIT_FX_EDIT_H_
#define FPDFSDK_FXEDIT_FX_EDIT_H_

#include "core/fxcrt/fx_basic.h"

class IPVT_FontMap;

#define FX_EDIT_ISLATINWORD(u)                  \
  (u == 0x2D || (u <= 0x005A && u >= 0x0041) || \
   (u <= 0x007A && u >= 0x0061) || (u <= 0x02AF && u >= 0x00C0))

CFX_ByteString GetPDFWordString(IPVT_FontMap* pFontMap,
                                int32_t nFontIndex,
                                uint16_t Word,
                                uint16_t SubWord);

#endif  // FPDFSDK_FXEDIT_FX_EDIT_H_
