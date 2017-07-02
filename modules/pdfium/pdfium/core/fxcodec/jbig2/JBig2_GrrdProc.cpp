// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_GrrdProc.h"

#include <memory>

#include "core/fxcodec/jbig2/JBig2_ArithDecoder.h"
#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcodec/jbig2/JBig2_Image.h"

CJBig2_Image* CJBig2_GRRDProc::decode(CJBig2_ArithDecoder* pArithDecoder,
                                      JBig2ArithCtx* grContext) {
  if (GRW == 0 || GRH == 0)
    return new CJBig2_Image(GRW, GRH);

  if (!GRTEMPLATE) {
    if ((GRAT[0] == -1) && (GRAT[1] == -1) && (GRAT[2] == -1) &&
        (GRAT[3] == -1) && (GRREFERENCEDX == 0) &&
        (GRW == (uint32_t)GRREFERENCE->width())) {
      return decode_Template0_opt(pArithDecoder, grContext);
    }
    return decode_Template0_unopt(pArithDecoder, grContext);
  }

  if ((GRREFERENCEDX == 0) && (GRW == (uint32_t)GRREFERENCE->width()))
    return decode_Template1_opt(pArithDecoder, grContext);
  return decode_Template1_unopt(pArithDecoder, grContext);
}

CJBig2_Image* CJBig2_GRRDProc::decode_Template0_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* grContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GRREG(new CJBig2_Image(GRW, GRH));
  GRREG->fill(0);
  for (uint32_t h = 0; h < GRH; h++) {
    if (TPGRON)
      LTP = LTP ^ pArithDecoder->DECODE(&grContext[0x0010]);
    if (!LTP) {
      uint32_t line1 = GRREG->getPixel(1, h - 1);
      line1 |= GRREG->getPixel(0, h - 1) << 1;
      uint32_t line2 = 0;
      uint32_t line3 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY - 1);
      line3 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY - 1)
               << 1;
      uint32_t line4 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY);
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY) << 1;
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY)
               << 2;
      uint32_t line5 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY + 1);
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY + 1)
               << 1;
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY + 1)
               << 2;
      for (uint32_t w = 0; w < GRW; w++) {
        uint32_t CONTEXT = line5;
        CONTEXT |= line4 << 3;
        CONTEXT |= line3 << 6;
        CONTEXT |= GRREFERENCE->getPixel(w - GRREFERENCEDX + GRAT[2],
                                         h - GRREFERENCEDY + GRAT[3])
                   << 8;
        CONTEXT |= line2 << 9;
        CONTEXT |= line1 << 10;
        CONTEXT |= GRREG->getPixel(w + GRAT[0], h + GRAT[1]) << 12;
        int bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
        GRREG->setPixel(w, h, bVal);
        line1 = ((line1 << 1) | GRREG->getPixel(w + 2, h - 1)) & 0x03;
        line2 = ((line2 << 1) | bVal) & 0x01;
        line3 = ((line3 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY - 1)) &
                0x03;
        line4 =
            ((line4 << 1) |
             GRREFERENCE->getPixel(w - GRREFERENCEDX + 2, h - GRREFERENCEDY)) &
            0x07;
        line5 = ((line5 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY + 1)) &
                0x07;
      }
    } else {
      uint32_t line1 = GRREG->getPixel(1, h - 1);
      line1 |= GRREG->getPixel(0, h - 1) << 1;
      uint32_t line2 = 0;
      uint32_t line3 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY - 1);
      line3 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY - 1)
               << 1;
      uint32_t line4 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY);
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY) << 1;
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY)
               << 2;
      uint32_t line5 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY + 1);
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY + 1)
               << 1;
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY + 1)
               << 2;
      for (uint32_t w = 0; w < GRW; w++) {
        int bVal = GRREFERENCE->getPixel(w, h);
        if (!(TPGRON && (bVal == GRREFERENCE->getPixel(w - 1, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w - 1, h)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h)) &&
              (bVal == GRREFERENCE->getPixel(w - 1, h + 1)) &&
              (bVal == GRREFERENCE->getPixel(w, h + 1)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h + 1)))) {
          uint32_t CONTEXT = line5;
          CONTEXT |= line4 << 3;
          CONTEXT |= line3 << 6;
          CONTEXT |= GRREFERENCE->getPixel(w - GRREFERENCEDX + GRAT[2],
                                           h - GRREFERENCEDY + GRAT[3])
                     << 8;
          CONTEXT |= line2 << 9;
          CONTEXT |= line1 << 10;
          CONTEXT |= GRREG->getPixel(w + GRAT[0], h + GRAT[1]) << 12;
          bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
        }
        GRREG->setPixel(w, h, bVal);
        line1 = ((line1 << 1) | GRREG->getPixel(w + 2, h - 1)) & 0x03;
        line2 = ((line2 << 1) | bVal) & 0x01;
        line3 = ((line3 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY - 1)) &
                0x03;
        line4 =
            ((line4 << 1) |
             GRREFERENCE->getPixel(w - GRREFERENCEDX + 2, h - GRREFERENCEDY)) &
            0x07;
        line5 = ((line5 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY + 1)) &
                0x07;
      }
    }
  }
  return GRREG.release();
}

CJBig2_Image* CJBig2_GRRDProc::decode_Template0_opt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* grContext) {
  if (!GRREFERENCE->m_pData)
    return nullptr;

  int32_t iGRW = static_cast<int32_t>(GRW);
  int32_t iGRH = static_cast<int32_t>(GRH);
  std::unique_ptr<CJBig2_Image> GRREG(new CJBig2_Image(iGRW, iGRH));
  if (!GRREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GRREG->m_pData;
  uint8_t* pLineR = GRREFERENCE->m_pData;
  intptr_t nStride = GRREG->stride();
  intptr_t nStrideR = GRREFERENCE->stride();
  int32_t GRWR = GRREFERENCE->width();
  int32_t GRHR = GRREFERENCE->height();
  if (GRREFERENCEDY < -GRHR + 1 || GRREFERENCEDY > GRHR - 1)
    GRREFERENCEDY = 0;
  intptr_t nOffset = -GRREFERENCEDY * nStrideR;
  for (int32_t h = 0; h < iGRH; h++) {
    if (TPGRON)
      LTP = LTP ^ pArithDecoder->DECODE(&grContext[0x0010]);
    uint32_t line1 = (h > 0) ? pLine[-nStride] << 4 : 0;
    int32_t reference_h = h - GRREFERENCEDY;
    bool line1_r_ok = (reference_h > 0 && reference_h < GRHR + 1);
    bool line2_r_ok = (reference_h > -1 && reference_h < GRHR);
    bool line3_r_ok = (reference_h > -2 && reference_h < GRHR - 1);
    uint32_t line1_r = line1_r_ok ? pLineR[nOffset - nStrideR] : 0;
    uint32_t line2_r = line2_r_ok ? pLineR[nOffset] : 0;
    uint32_t line3_r = line3_r_ok ? pLineR[nOffset + nStrideR] : 0;
    if (!LTP) {
      uint32_t CONTEXT = (line1 & 0x1c00) | (line1_r & 0x01c0) |
                         ((line2_r >> 3) & 0x0038) | ((line3_r >> 6) & 0x0007);
      for (int32_t w = 0; w < iGRW; w += 8) {
        int32_t nBits = iGRW - w > 8 ? 8 : iGRW - w;
        if (h > 0) {
          line1 = (line1 << 8) |
                  (w + 8 < iGRW ? pLine[-nStride + (w >> 3) + 1] << 4 : 0);
        }
        if (h > GRHR + GRREFERENCEDY + 1) {
          line1_r = 0;
          line2_r = 0;
          line3_r = 0;
        } else {
          if (line1_r_ok) {
            line1_r =
                (line1_r << 8) |
                (w + 8 < GRWR ? pLineR[nOffset - nStrideR + (w >> 3) + 1] : 0);
          }
          if (line2_r_ok) {
            line2_r = (line2_r << 8) |
                      (w + 8 < GRWR ? pLineR[nOffset + (w >> 3) + 1] : 0);
          }
          if (line3_r_ok) {
            line3_r =
                (line3_r << 8) |
                (w + 8 < GRWR ? pLineR[nOffset + nStrideR + (w >> 3) + 1] : 0);
          } else {
            line3_r = 0;
          }
        }
        uint8_t cVal = 0;
        for (int32_t k = 0; k < nBits; k++) {
          int bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
          cVal |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x0cdb) << 1) | (bVal << 9) |
                    ((line1 >> (7 - k)) & 0x0400) |
                    ((line1_r >> (7 - k)) & 0x0040) |
                    ((line2_r >> (10 - k)) & 0x0008) |
                    ((line3_r >> (13 - k)) & 0x0001);
        }
        pLine[w >> 3] = cVal;
      }
    } else {
      uint32_t CONTEXT = (line1 & 0x1c00) | (line1_r & 0x01c0) |
                         ((line2_r >> 3) & 0x0038) | ((line3_r >> 6) & 0x0007);
      for (int32_t w = 0; w < iGRW; w += 8) {
        int32_t nBits = iGRW - w > 8 ? 8 : iGRW - w;
        if (h > 0) {
          line1 = (line1 << 8) |
                  (w + 8 < iGRW ? pLine[-nStride + (w >> 3) + 1] << 4 : 0);
        }
        if (line1_r_ok) {
          line1_r =
              (line1_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset - nStrideR + (w >> 3) + 1] : 0);
        }
        if (line2_r_ok) {
          line2_r = (line2_r << 8) |
                    (w + 8 < GRWR ? pLineR[nOffset + (w >> 3) + 1] : 0);
        }
        if (line3_r_ok) {
          line3_r =
              (line3_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset + nStrideR + (w >> 3) + 1] : 0);
        } else {
          line3_r = 0;
        }
        uint8_t cVal = 0;
        for (int32_t k = 0; k < nBits; k++) {
          int bVal = GRREFERENCE->getPixel(w + k, h);
          if (!(TPGRON && (bVal == GRREFERENCE->getPixel(w + k - 1, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k - 1, h)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h)) &&
                (bVal == GRREFERENCE->getPixel(w + k - 1, h + 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k, h + 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h + 1)))) {
            bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
          }
          cVal |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x0cdb) << 1) | (bVal << 9) |
                    ((line1 >> (7 - k)) & 0x0400) |
                    ((line1_r >> (7 - k)) & 0x0040) |
                    ((line2_r >> (10 - k)) & 0x0008) |
                    ((line3_r >> (13 - k)) & 0x0001);
        }
        pLine[w >> 3] = cVal;
      }
    }
    pLine += nStride;
    if (h < GRHR + GRREFERENCEDY) {
      pLineR += nStrideR;
    }
  }
  return GRREG.release();
}

CJBig2_Image* CJBig2_GRRDProc::decode_Template1_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* grContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GRREG(new CJBig2_Image(GRW, GRH));
  GRREG->fill(0);
  for (uint32_t h = 0; h < GRH; h++) {
    if (TPGRON)
      LTP = LTP ^ pArithDecoder->DECODE(&grContext[0x0008]);
    if (!LTP) {
      uint32_t line1 = GRREG->getPixel(1, h - 1);
      line1 |= GRREG->getPixel(0, h - 1) << 1;
      line1 |= GRREG->getPixel(-1, h - 1) << 2;
      uint32_t line2 = 0;
      uint32_t line3 =
          GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY - 1);
      uint32_t line4 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY);
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY) << 1;
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY)
               << 2;
      uint32_t line5 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY + 1);
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY + 1)
               << 1;
      for (uint32_t w = 0; w < GRW; w++) {
        uint32_t CONTEXT = line5;
        CONTEXT |= line4 << 2;
        CONTEXT |= line3 << 5;
        CONTEXT |= line2 << 6;
        CONTEXT |= line1 << 7;
        int bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
        GRREG->setPixel(w, h, bVal);
        line1 = ((line1 << 1) | GRREG->getPixel(w + 2, h - 1)) & 0x07;
        line2 = ((line2 << 1) | bVal) & 0x01;
        line3 = ((line3 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 1,
                                       h - GRREFERENCEDY - 1)) &
                0x01;
        line4 =
            ((line4 << 1) |
             GRREFERENCE->getPixel(w - GRREFERENCEDX + 2, h - GRREFERENCEDY)) &
            0x07;
        line5 = ((line5 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY + 1)) &
                0x03;
      }
    } else {
      uint32_t line1 = GRREG->getPixel(1, h - 1);
      line1 |= GRREG->getPixel(0, h - 1) << 1;
      line1 |= GRREG->getPixel(-1, h - 1) << 2;
      uint32_t line2 = 0;
      uint32_t line3 =
          GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY - 1);
      uint32_t line4 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY);
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY) << 1;
      line4 |= GRREFERENCE->getPixel(-GRREFERENCEDX - 1, h - GRREFERENCEDY)
               << 2;
      uint32_t line5 =
          GRREFERENCE->getPixel(-GRREFERENCEDX + 1, h - GRREFERENCEDY + 1);
      line5 |= GRREFERENCE->getPixel(-GRREFERENCEDX, h - GRREFERENCEDY + 1)
               << 1;
      for (uint32_t w = 0; w < GRW; w++) {
        int bVal = GRREFERENCE->getPixel(w, h);
        if (!(TPGRON && (bVal == GRREFERENCE->getPixel(w - 1, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h - 1)) &&
              (bVal == GRREFERENCE->getPixel(w - 1, h)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h)) &&
              (bVal == GRREFERENCE->getPixel(w - 1, h + 1)) &&
              (bVal == GRREFERENCE->getPixel(w, h + 1)) &&
              (bVal == GRREFERENCE->getPixel(w + 1, h + 1)))) {
          uint32_t CONTEXT = line5;
          CONTEXT |= line4 << 2;
          CONTEXT |= line3 << 5;
          CONTEXT |= line2 << 6;
          CONTEXT |= line1 << 7;
          bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
        }
        GRREG->setPixel(w, h, bVal);
        line1 = ((line1 << 1) | GRREG->getPixel(w + 2, h - 1)) & 0x07;
        line2 = ((line2 << 1) | bVal) & 0x01;
        line3 = ((line3 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 1,
                                       h - GRREFERENCEDY - 1)) &
                0x01;
        line4 =
            ((line4 << 1) |
             GRREFERENCE->getPixel(w - GRREFERENCEDX + 2, h - GRREFERENCEDY)) &
            0x07;
        line5 = ((line5 << 1) |
                 GRREFERENCE->getPixel(w - GRREFERENCEDX + 2,
                                       h - GRREFERENCEDY + 1)) &
                0x03;
      }
    }
  }
  return GRREG.release();
}

CJBig2_Image* CJBig2_GRRDProc::decode_Template1_opt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* grContext) {
  if (!GRREFERENCE->m_pData)
    return nullptr;

  int32_t iGRW = static_cast<int32_t>(GRW);
  int32_t iGRH = static_cast<int32_t>(GRH);
  std::unique_ptr<CJBig2_Image> GRREG(new CJBig2_Image(iGRW, iGRH));
  if (!GRREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GRREG->m_pData;
  uint8_t* pLineR = GRREFERENCE->m_pData;
  intptr_t nStride = GRREG->stride();
  intptr_t nStrideR = GRREFERENCE->stride();
  int32_t GRWR = GRREFERENCE->width();
  int32_t GRHR = GRREFERENCE->height();
  if (GRREFERENCEDY < -GRHR + 1 || GRREFERENCEDY > GRHR - 1) {
    GRREFERENCEDY = 0;
  }
  intptr_t nOffset = -GRREFERENCEDY * nStrideR;
  for (int32_t h = 0; h < iGRH; h++) {
    if (TPGRON)
      LTP = LTP ^ pArithDecoder->DECODE(&grContext[0x0008]);
    uint32_t line1 = (h > 0) ? pLine[-nStride] << 1 : 0;
    int32_t reference_h = h - GRREFERENCEDY;
    bool line1_r_ok = (reference_h > 0 && reference_h < GRHR + 1);
    bool line2_r_ok = (reference_h > -1 && reference_h < GRHR);
    bool line3_r_ok = (reference_h > -2 && reference_h < GRHR - 1);
    uint32_t line1_r = line1_r_ok ? pLineR[nOffset - nStrideR] : 0;
    uint32_t line2_r = line2_r_ok ? pLineR[nOffset] : 0;
    uint32_t line3_r = line3_r_ok ? pLineR[nOffset + nStrideR] : 0;
    if (!LTP) {
      uint32_t CONTEXT = (line1 & 0x0380) | ((line1_r >> 2) & 0x0020) |
                         ((line2_r >> 4) & 0x001c) | ((line3_r >> 6) & 0x0003);
      for (int32_t w = 0; w < iGRW; w += 8) {
        int32_t nBits = iGRW - w > 8 ? 8 : iGRW - w;
        if (h > 0)
          line1 = (line1 << 8) |
                  (w + 8 < iGRW ? pLine[-nStride + (w >> 3) + 1] << 1 : 0);
        if (line1_r_ok)
          line1_r =
              (line1_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset - nStrideR + (w >> 3) + 1] : 0);
        if (line2_r_ok)
          line2_r = (line2_r << 8) |
                    (w + 8 < GRWR ? pLineR[nOffset + (w >> 3) + 1] : 0);
        if (line3_r_ok) {
          line3_r =
              (line3_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset + nStrideR + (w >> 3) + 1] : 0);
        } else {
          line3_r = 0;
        }
        uint8_t cVal = 0;
        for (int32_t k = 0; k < nBits; k++) {
          int bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
          cVal |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x018d) << 1) | (bVal << 6) |
                    ((line1 >> (7 - k)) & 0x0080) |
                    ((line1_r >> (9 - k)) & 0x0020) |
                    ((line2_r >> (11 - k)) & 0x0004) |
                    ((line3_r >> (13 - k)) & 0x0001);
        }
        pLine[w >> 3] = cVal;
      }
    } else {
      uint32_t CONTEXT = (line1 & 0x0380) | ((line1_r >> 2) & 0x0020) |
                         ((line2_r >> 4) & 0x001c) | ((line3_r >> 6) & 0x0003);
      for (int32_t w = 0; w < iGRW; w += 8) {
        int32_t nBits = iGRW - w > 8 ? 8 : iGRW - w;
        if (h > 0)
          line1 = (line1 << 8) |
                  (w + 8 < iGRW ? pLine[-nStride + (w >> 3) + 1] << 1 : 0);
        if (line1_r_ok)
          line1_r =
              (line1_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset - nStrideR + (w >> 3) + 1] : 0);
        if (line2_r_ok)
          line2_r = (line2_r << 8) |
                    (w + 8 < GRWR ? pLineR[nOffset + (w >> 3) + 1] : 0);
        if (line3_r_ok) {
          line3_r =
              (line3_r << 8) |
              (w + 8 < GRWR ? pLineR[nOffset + nStrideR + (w >> 3) + 1] : 0);
        } else {
          line3_r = 0;
        }
        uint8_t cVal = 0;
        for (int32_t k = 0; k < nBits; k++) {
          int bVal = GRREFERENCE->getPixel(w + k, h);
          if (!(TPGRON && (bVal == GRREFERENCE->getPixel(w + k - 1, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h - 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k - 1, h)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h)) &&
                (bVal == GRREFERENCE->getPixel(w + k - 1, h + 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k, h + 1)) &&
                (bVal == GRREFERENCE->getPixel(w + k + 1, h + 1)))) {
            bVal = pArithDecoder->DECODE(&grContext[CONTEXT]);
          }
          cVal |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x018d) << 1) | (bVal << 6) |
                    ((line1 >> (7 - k)) & 0x0080) |
                    ((line1_r >> (9 - k)) & 0x0020) |
                    ((line2_r >> (11 - k)) & 0x0004) |
                    ((line3_r >> (13 - k)) & 0x0001);
        }
        pLine[w >> 3] = cVal;
      }
    }
    pLine += nStride;
    if (h < GRHR + GRREFERENCEDY) {
      pLineR += nStrideR;
    }
  }
  return GRREG.release();
}
