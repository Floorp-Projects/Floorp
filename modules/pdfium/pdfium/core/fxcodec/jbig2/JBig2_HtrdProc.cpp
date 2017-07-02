// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_HtrdProc.h"

#include <memory>

#include "core/fxcodec/jbig2/JBig2_GsidProc.h"
#include "core/fxcrt/fx_basic.h"
#include "third_party/base/ptr_util.h"

CJBig2_Image* CJBig2_HTRDProc::decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                                            JBig2ArithCtx* gbContext,
                                            IFX_Pause* pPause) {
  uint32_t ng, mg;
  int32_t x, y;
  uint32_t HBPP;
  uint32_t* GI;
  std::unique_ptr<CJBig2_Image> HSKIP;
  std::unique_ptr<CJBig2_Image> HTREG(new CJBig2_Image(HBW, HBH));
  HTREG->fill(HDEFPIXEL);
  if (HENABLESKIP == 1) {
    HSKIP = pdfium::MakeUnique<CJBig2_Image>(HGW, HGH);
    for (mg = 0; mg < HGH; mg++) {
      for (ng = 0; ng < HGW; ng++) {
        x = (HGX + mg * HRY + ng * HRX) >> 8;
        y = (HGY + mg * HRX - ng * HRY) >> 8;
        if ((x + HPW <= 0) | (x >= (int32_t)HBW) | (y + HPH <= 0) |
            (y >= (int32_t)HPH)) {
          HSKIP->setPixel(ng, mg, 1);
        } else {
          HSKIP->setPixel(ng, mg, 0);
        }
      }
    }
  }
  HBPP = 1;
  while ((uint32_t)(1 << HBPP) < HNUMPATS) {
    HBPP++;
  }
  std::unique_ptr<CJBig2_GSIDProc> pGID(new CJBig2_GSIDProc());
  pGID->GSMMR = HMMR;
  pGID->GSW = HGW;
  pGID->GSH = HGH;
  pGID->GSBPP = (uint8_t)HBPP;
  pGID->GSUSESKIP = HENABLESKIP;
  pGID->GSKIP = HSKIP.get();
  pGID->GSTEMPLATE = HTEMPLATE;
  GI = pGID->decode_Arith(pArithDecoder, gbContext, pPause);
  if (!GI)
    return nullptr;

  for (mg = 0; mg < HGH; mg++) {
    for (ng = 0; ng < HGW; ng++) {
      x = (HGX + mg * HRY + ng * HRX) >> 8;
      y = (HGY + mg * HRX - ng * HRY) >> 8;
      uint32_t pat_index = GI[mg * HGW + ng];
      if (pat_index >= HNUMPATS) {
        pat_index = HNUMPATS - 1;
      }
      HTREG->composeFrom(x, y, HPATS[pat_index], HCOMBOP);
    }
  }
  FX_Free(GI);
  return HTREG.release();
}

CJBig2_Image* CJBig2_HTRDProc::decode_MMR(CJBig2_BitStream* pStream,
                                          IFX_Pause* pPause) {
  uint32_t ng, mg;
  int32_t x, y;
  uint32_t* GI;
  std::unique_ptr<CJBig2_Image> HTREG(new CJBig2_Image(HBW, HBH));
  HTREG->fill(HDEFPIXEL);
  uint32_t HBPP = 1;
  while ((uint32_t)(1 << HBPP) < HNUMPATS) {
    HBPP++;
  }
  std::unique_ptr<CJBig2_GSIDProc> pGID(new CJBig2_GSIDProc());
  pGID->GSMMR = HMMR;
  pGID->GSW = HGW;
  pGID->GSH = HGH;
  pGID->GSBPP = (uint8_t)HBPP;
  pGID->GSUSESKIP = 0;
  GI = pGID->decode_MMR(pStream, pPause);
  if (!GI)
    return nullptr;

  for (mg = 0; mg < HGH; mg++) {
    for (ng = 0; ng < HGW; ng++) {
      x = (HGX + mg * HRY + ng * HRX) >> 8;
      y = (HGY + mg * HRX - ng * HRY) >> 8;
      uint32_t pat_index = GI[mg * HGW + ng];
      if (pat_index >= HNUMPATS) {
        pat_index = HNUMPATS - 1;
      }
      HTREG->composeFrom(x, y, HPATS[pat_index], HCOMBOP);
    }
  }
  FX_Free(GI);
  return HTREG.release();
}
