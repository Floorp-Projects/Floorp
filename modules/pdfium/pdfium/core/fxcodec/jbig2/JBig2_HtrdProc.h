// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_HTRDPROC_H_
#define CORE_FXCODEC_JBIG2_JBIG2_HTRDPROC_H_

#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_system.h"

class CJBig2_ArithDecoder;
class CJBig2_BitStream;
class IFX_Pause;
struct JBig2ArithCtx;

class CJBig2_HTRDProc {
 public:
  CJBig2_Image* decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                             JBig2ArithCtx* gbContext,
                             IFX_Pause* pPause);

  CJBig2_Image* decode_MMR(CJBig2_BitStream* pStream, IFX_Pause* pPause);

 public:
  uint32_t HBW;
  uint32_t HBH;
  bool HMMR;
  uint8_t HTEMPLATE;
  uint32_t HNUMPATS;
  CJBig2_Image** HPATS;
  bool HDEFPIXEL;
  JBig2ComposeOp HCOMBOP;
  bool HENABLESKIP;
  uint32_t HGW;
  uint32_t HGH;
  int32_t HGX;
  int32_t HGY;
  uint16_t HRX;
  uint16_t HRY;
  uint8_t HPW;
  uint8_t HPH;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_HTRDPROC_H_
