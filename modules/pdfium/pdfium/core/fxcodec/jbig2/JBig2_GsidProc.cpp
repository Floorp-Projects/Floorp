// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_GsidProc.h"

#include <memory>
#include <vector>

#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcodec/jbig2/JBig2_GrdProc.h"
#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_basic.h"

uint32_t* CJBig2_GSIDProc::decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                                        JBig2ArithCtx* gbContext,
                                        IFX_Pause* pPause) {
  std::unique_ptr<CJBig2_GRDProc> pGRD(new CJBig2_GRDProc());
  pGRD->MMR = GSMMR;
  pGRD->GBW = GSW;
  pGRD->GBH = GSH;
  pGRD->GBTEMPLATE = GSTEMPLATE;
  pGRD->TPGDON = 0;
  pGRD->USESKIP = GSUSESKIP;
  pGRD->SKIP = GSKIP;
  if (GSTEMPLATE <= 1) {
    pGRD->GBAT[0] = 3;
  } else {
    pGRD->GBAT[0] = 2;
  }
  pGRD->GBAT[1] = -1;
  if (pGRD->GBTEMPLATE == 0) {
    pGRD->GBAT[2] = -3;
    pGRD->GBAT[3] = -1;
    pGRD->GBAT[4] = 2;
    pGRD->GBAT[5] = -2;
    pGRD->GBAT[6] = -2;
    pGRD->GBAT[7] = -2;
  }

  std::vector<std::unique_ptr<CJBig2_Image>> GSPLANES(GSBPP);
  for (int32_t i = GSBPP - 1; i >= 0; --i) {
    CJBig2_Image* pImage = nullptr;
    FXCODEC_STATUS status =
        pGRD->Start_decode_Arith(&pImage, pArithDecoder, gbContext, nullptr);
    while (status == FXCODEC_STATUS_DECODE_TOBECONTINUE)
      status = pGRD->Continue_decode(pPause);

    if (!pImage)
      return nullptr;

    GSPLANES[i].reset(pImage);
    if (i < GSBPP - 1)
      pImage->composeFrom(0, 0, GSPLANES[i + 1].get(), JBIG2_COMPOSE_XOR);
  }
  std::unique_ptr<uint32_t, FxFreeDeleter> GSVALS(
      FX_Alloc2D(uint32_t, GSW, GSH));
  JBIG2_memset(GSVALS.get(), 0, sizeof(uint32_t) * GSW * GSH);
  for (uint32_t y = 0; y < GSH; ++y) {
    for (uint32_t x = 0; x < GSW; ++x) {
      for (int32_t i = 0; i < GSBPP; ++i)
        GSVALS.get()[y * GSW + x] |= GSPLANES[i]->getPixel(x, y) << i;
    }
  }
  return GSVALS.release();
}

uint32_t* CJBig2_GSIDProc::decode_MMR(CJBig2_BitStream* pStream,
                                      IFX_Pause* pPause) {
  std::unique_ptr<CJBig2_GRDProc> pGRD(new CJBig2_GRDProc());
  pGRD->MMR = GSMMR;
  pGRD->GBW = GSW;
  pGRD->GBH = GSH;

  std::unique_ptr<CJBig2_Image*> GSPLANES(FX_Alloc(CJBig2_Image*, GSBPP));
  JBIG2_memset(GSPLANES.get(), 0, sizeof(CJBig2_Image*) * GSBPP);
  pGRD->Start_decode_MMR(&GSPLANES.get()[GSBPP - 1], pStream, nullptr);
  if (!GSPLANES.get()[GSBPP - 1])
    return nullptr;

  pStream->alignByte();
  pStream->offset(3);
  int32_t J = GSBPP - 2;
  while (J >= 0) {
    pGRD->Start_decode_MMR(&GSPLANES.get()[J], pStream, nullptr);
    if (!GSPLANES.get()[J]) {
      for (int32_t K = GSBPP - 1; K > J; --K)
        delete GSPLANES.get()[K];
      return nullptr;
    }
    pStream->alignByte();
    pStream->offset(3);
    GSPLANES.get()[J]->composeFrom(0, 0, GSPLANES.get()[J + 1],
                                   JBIG2_COMPOSE_XOR);
    J = J - 1;
  }
  std::unique_ptr<uint32_t> GSVALS(FX_Alloc2D(uint32_t, GSW, GSH));
  JBIG2_memset(GSVALS.get(), 0, sizeof(uint32_t) * GSW * GSH);
  for (uint32_t y = 0; y < GSH; ++y) {
    for (uint32_t x = 0; x < GSW; ++x) {
      for (J = 0; J < GSBPP; ++J) {
        GSVALS.get()[y * GSW + x] |= GSPLANES.get()[J]->getPixel(x, y) << J;
      }
    }
  }
  for (J = 0; J < GSBPP; ++J) {
    delete GSPLANES.get()[J];
  }
  return GSVALS.release();
}
