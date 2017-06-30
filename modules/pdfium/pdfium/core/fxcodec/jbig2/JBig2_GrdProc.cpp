// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_GrdProc.h"

#include <memory>

#include "core/fxcodec/fx_codec.h"
#include "core/fxcodec/jbig2/JBig2_ArithDecoder.h"
#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcodec/jbig2/JBig2_Image.h"

CJBig2_GRDProc::CJBig2_GRDProc()
    : m_loopIndex(0),
      m_pLine(nullptr),
      m_pPause(nullptr),
      m_DecodeType(0),
      m_LTP(0) {
  m_ReplaceRect.left = 0;
  m_ReplaceRect.bottom = 0;
  m_ReplaceRect.top = 0;
  m_ReplaceRect.right = 0;
}

bool CJBig2_GRDProc::UseTemplate0Opt3() const {
  return (GBAT[0] == 3) && (GBAT[1] == -1) && (GBAT[2] == -3) &&
         (GBAT[3] == -1) && (GBAT[4] == 2) && (GBAT[5] == -2) &&
         (GBAT[6] == -2) && (GBAT[7] == -2);
}

bool CJBig2_GRDProc::UseTemplate1Opt3() const {
  return (GBAT[0] == 3) && (GBAT[1] == -1);
}

bool CJBig2_GRDProc::UseTemplate23Opt3() const {
  return (GBAT[0] == 2) && (GBAT[1] == -1);
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                                           JBig2ArithCtx* gbContext) {
  if (GBW == 0 || GBH == 0)
    return new CJBig2_Image(GBW, GBH);

  if (GBTEMPLATE == 0) {
    if (UseTemplate0Opt3())
      return decode_Arith_Template0_opt3(pArithDecoder, gbContext);
    return decode_Arith_Template0_unopt(pArithDecoder, gbContext);
  } else if (GBTEMPLATE == 1) {
    if (UseTemplate1Opt3())
      return decode_Arith_Template1_opt3(pArithDecoder, gbContext);
    return decode_Arith_Template1_unopt(pArithDecoder, gbContext);
  } else if (GBTEMPLATE == 2) {
    if (UseTemplate23Opt3())
      return decode_Arith_Template2_opt3(pArithDecoder, gbContext);
    return decode_Arith_Template2_unopt(pArithDecoder, gbContext);
  } else {
    if (UseTemplate23Opt3())
      return decode_Arith_Template3_opt3(pArithDecoder, gbContext);
    return decode_Arith_Template3_unopt(pArithDecoder, gbContext);
  }
}
CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template0_opt3(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  if (!GBREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GBREG->m_pData;
  int32_t nStride = GBREG->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  uint32_t height = GBH & 0x7fffffff;
  for (uint32_t h = 0; h < height; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x9b25]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      if (h > 1) {
        uint8_t* pLine1 = pLine - nStride2;
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line1 = (*pLine1++) << 6;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = ((line1 & 0xf800) | (line2 & 0x07f0));
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 6);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = (((CONTEXT & 0x7bf7) << 1) | bVal |
                       ((line1 >> k) & 0x0800) | ((line2 >> k) & 0x0010));
          }
          pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              (((CONTEXT & 0x7bf7) << 1) | bVal |
               ((line1 >> (7 - k)) & 0x0800) | ((line2 >> (7 - k)) & 0x0010));
        }
        pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line2 = (h & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 & 0x07f0);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (h & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT =
                (((CONTEXT & 0x7bf7) << 1) | bVal | ((line2 >> k) & 0x0010));
          }
          pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = (((CONTEXT & 0x7bf7) << 1) | bVal |
                     (((line2 >> (7 - k))) & 0x0010));
        }
        pLine[nLineBytes] = cVal1;
      }
    }
    pLine += nStride;
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template0_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  GBREG->fill(0);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x9b25]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      uint32_t line1 = GBREG->getPixel(1, h - 2);
      line1 |= GBREG->getPixel(0, h - 2) << 1;
      uint32_t line2 = GBREG->getPixel(2, h - 1);
      line2 |= GBREG->getPixel(1, h - 1) << 1;
      line2 |= GBREG->getPixel(0, h - 1) << 2;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, h)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= GBREG->getPixel(w + GBAT[0], h + GBAT[1]) << 4;
          CONTEXT |= line2 << 5;
          CONTEXT |= GBREG->getPixel(w + GBAT[2], h + GBAT[3]) << 10;
          CONTEXT |= GBREG->getPixel(w + GBAT[4], h + GBAT[5]) << 11;
          CONTEXT |= line1 << 12;
          CONTEXT |= GBREG->getPixel(w + GBAT[6], h + GBAT[7]) << 15;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          GBREG->setPixel(w, h, bVal);
        }
        line1 = ((line1 << 1) | GBREG->getPixel(w + 2, h - 2)) & 0x07;
        line2 = ((line2 << 1) | GBREG->getPixel(w + 3, h - 1)) & 0x1f;
        line3 = ((line3 << 1) | bVal) & 0x0f;
      }
    }
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template1_opt3(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  if (!GBREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GBREG->m_pData;
  int32_t nStride = GBREG->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x0795]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      if (h > 1) {
        uint8_t* pLine1 = pLine - nStride2;
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line1 = (*pLine1++) << 4;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = (line1 & 0x1e00) | ((line2 >> 1) & 0x01f8);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 4);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                      ((line1 >> k) & 0x0200) | ((line2 >> (k + 1)) & 0x0008);
          }
          pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                    ((line1 >> (7 - k)) & 0x0200) |
                    ((line2 >> (8 - k)) & 0x0008);
        }
        pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line2 = (h & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 >> 1) & 0x01f8;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (h & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                      ((line2 >> (k + 1)) & 0x0008);
          }
          pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              ((CONTEXT & 0x0efb) << 1) | bVal | ((line2 >> (8 - k)) & 0x0008);
        }
        pLine[nLineBytes] = cVal1;
      }
    }
    pLine += nStride;
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template1_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  GBREG->fill(0);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x0795]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      uint32_t line1 = GBREG->getPixel(2, h - 2);
      line1 |= GBREG->getPixel(1, h - 2) << 1;
      line1 |= GBREG->getPixel(0, h - 2) << 2;
      uint32_t line2 = GBREG->getPixel(2, h - 1);
      line2 |= GBREG->getPixel(1, h - 1) << 1;
      line2 |= GBREG->getPixel(0, h - 1) << 2;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, h)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= GBREG->getPixel(w + GBAT[0], h + GBAT[1]) << 3;
          CONTEXT |= line2 << 4;
          CONTEXT |= line1 << 9;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          GBREG->setPixel(w, h, bVal);
        }
        line1 = ((line1 << 1) | GBREG->getPixel(w + 3, h - 2)) & 0x0f;
        line2 = ((line2 << 1) | GBREG->getPixel(w + 3, h - 1)) & 0x1f;
        line3 = ((line3 << 1) | bVal) & 0x07;
      }
    }
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template2_opt3(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  if (!GBREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GBREG->m_pData;
  int32_t nStride = GBREG->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x00e5]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      if (h > 1) {
        uint8_t* pLine1 = pLine - nStride2;
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line1 = (*pLine1++) << 1;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = (line1 & 0x0380) | ((line2 >> 3) & 0x007c);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 1);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                      ((line1 >> k) & 0x0080) | ((line2 >> (k + 3)) & 0x0004);
          }
          pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                    ((line1 >> (7 - k)) & 0x0080) |
                    ((line2 >> (10 - k)) & 0x0004);
        }
        pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = pLine - nStride;
        uint32_t line2 = (h & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 >> 3) & 0x007c;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (h & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                      ((line2 >> (k + 3)) & 0x0004);
          }
          pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                    (((line2 >> (10 - k))) & 0x0004);
        }
        pLine[nLineBytes] = cVal1;
      }
    }
    pLine += nStride;
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template2_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  GBREG->fill(0);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x00e5]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      uint32_t line1 = GBREG->getPixel(1, h - 2);
      line1 |= GBREG->getPixel(0, h - 2) << 1;
      uint32_t line2 = GBREG->getPixel(1, h - 1);
      line2 |= GBREG->getPixel(0, h - 1) << 1;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, h)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= GBREG->getPixel(w + GBAT[0], h + GBAT[1]) << 2;
          CONTEXT |= line2 << 3;
          CONTEXT |= line1 << 7;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          GBREG->setPixel(w, h, bVal);
        }
        line1 = ((line1 << 1) | GBREG->getPixel(w + 2, h - 2)) & 0x07;
        line2 = ((line2 << 1) | GBREG->getPixel(w + 2, h - 1)) & 0x0f;
        line3 = ((line3 << 1) | bVal) & 0x03;
      }
    }
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template3_opt3(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  if (!GBREG->m_pData)
    return nullptr;

  int LTP = 0;
  uint8_t* pLine = GBREG->m_pData;
  int32_t nStride = GBREG->stride();
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x0195]);
    if (LTP) {
      GBREG->copyLine(h, h - 1);
    } else {
      if (h > 0) {
        uint8_t* pLine1 = pLine - nStride;
        uint32_t line1 = *pLine1++;
        uint32_t CONTEXT = (line1 >> 1) & 0x03f0;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | (*pLine1++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal |
                      ((line1 >> (k + 1)) & 0x0010);
          }
          pLine[cc] = cVal;
        }
        line1 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              ((CONTEXT & 0x01f7) << 1) | bVal | ((line1 >> (8 - k)) & 0x0010);
        }
        pLine[nLineBytes] = cVal1;
      } else {
        uint32_t CONTEXT = 0;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal;
          }
          pLine[cc] = cVal;
        }
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal;
        }
        pLine[nLineBytes] = cVal1;
      }
    }
    pLine += nStride;
  }
  return GBREG.release();
}

CJBig2_Image* CJBig2_GRDProc::decode_Arith_Template3_unopt(
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext) {
  int LTP = 0;
  std::unique_ptr<CJBig2_Image> GBREG(new CJBig2_Image(GBW, GBH));
  GBREG->fill(0);
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      LTP = LTP ^ pArithDecoder->DECODE(&gbContext[0x0195]);
    if (LTP == 1) {
      GBREG->copyLine(h, h - 1);
    } else {
      uint32_t line1 = GBREG->getPixel(1, h - 1);
      line1 |= GBREG->getPixel(0, h - 1) << 1;
      uint32_t line2 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, h)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line2;
          CONTEXT |= GBREG->getPixel(w + GBAT[0], h + GBAT[1]) << 4;
          CONTEXT |= line1 << 5;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          GBREG->setPixel(w, h, bVal);
        }
        line1 = ((line1 << 1) | GBREG->getPixel(w + 2, h - 1)) & 0x1f;
        line2 = ((line2 << 1) | bVal) & 0x0f;
      }
    }
  }
  return GBREG.release();
}

FXCODEC_STATUS CJBig2_GRDProc::Start_decode_Arith(
    CJBig2_Image** pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  if (GBW == 0 || GBH == 0) {
    m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
    return FXCODEC_STATUS_DECODE_FINISH;
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_READY;
  m_pPause = pPause;
  if (!*pImage)
    *pImage = new CJBig2_Image(GBW, GBH);
  if (!(*pImage)->m_pData) {
    delete *pImage;
    *pImage = nullptr;
    m_ProssiveStatus = FXCODEC_STATUS_ERROR;
    return FXCODEC_STATUS_ERROR;
  }
  m_DecodeType = 1;
  m_pImage = pImage;
  (*m_pImage)->fill(0);
  m_pArithDecoder = pArithDecoder;
  m_gbContext = gbContext;
  m_LTP = 0;
  m_pLine = nullptr;
  m_loopIndex = 0;
  return decode_Arith(pPause);
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith(IFX_Pause* pPause) {
  int iline = m_loopIndex;
  CJBig2_Image* pImage = *m_pImage;
  if (GBTEMPLATE == 0) {
    if (UseTemplate0Opt3()) {
      m_ProssiveStatus = decode_Arith_Template0_opt3(pImage, m_pArithDecoder,
                                                     m_gbContext, pPause);
    } else {
      m_ProssiveStatus = decode_Arith_Template0_unopt(pImage, m_pArithDecoder,
                                                      m_gbContext, pPause);
    }
  } else if (GBTEMPLATE == 1) {
    if (UseTemplate1Opt3()) {
      m_ProssiveStatus = decode_Arith_Template1_opt3(pImage, m_pArithDecoder,
                                                     m_gbContext, pPause);
    } else {
      m_ProssiveStatus = decode_Arith_Template1_unopt(pImage, m_pArithDecoder,
                                                      m_gbContext, pPause);
    }
  } else if (GBTEMPLATE == 2) {
    if (UseTemplate23Opt3()) {
      m_ProssiveStatus = decode_Arith_Template2_opt3(pImage, m_pArithDecoder,
                                                     m_gbContext, pPause);
    } else {
      m_ProssiveStatus = decode_Arith_Template2_unopt(pImage, m_pArithDecoder,
                                                      m_gbContext, pPause);
    }
  } else {
    if (UseTemplate23Opt3()) {
      m_ProssiveStatus = decode_Arith_Template3_opt3(pImage, m_pArithDecoder,
                                                     m_gbContext, pPause);
    } else {
      m_ProssiveStatus = decode_Arith_Template3_unopt(pImage, m_pArithDecoder,
                                                      m_gbContext, pPause);
    }
  }
  m_ReplaceRect.left = 0;
  m_ReplaceRect.right = pImage->width();
  m_ReplaceRect.top = iline;
  m_ReplaceRect.bottom = m_loopIndex;
  if (m_ProssiveStatus == FXCODEC_STATUS_DECODE_FINISH) {
    m_loopIndex = 0;
  }
  return m_ProssiveStatus;
}

FXCODEC_STATUS CJBig2_GRDProc::Start_decode_MMR(CJBig2_Image** pImage,
                                                CJBig2_BitStream* pStream,
                                                IFX_Pause* pPause) {
  int bitpos, i;
  *pImage = new CJBig2_Image(GBW, GBH);
  if (!(*pImage)->m_pData) {
    delete (*pImage);
    (*pImage) = nullptr;
    m_ProssiveStatus = FXCODEC_STATUS_ERROR;
    return m_ProssiveStatus;
  }
  bitpos = (int)pStream->getBitPos();
  FaxG4Decode(pStream->getBuf(), pStream->getLength(), &bitpos,
              (*pImage)->m_pData, GBW, GBH, (*pImage)->stride());
  pStream->setBitPos(bitpos);
  for (i = 0; (uint32_t)i < (*pImage)->stride() * GBH; i++) {
    (*pImage)->m_pData[i] = ~(*pImage)->m_pData[i];
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return m_ProssiveStatus;
}

FXCODEC_STATUS CJBig2_GRDProc::Continue_decode(IFX_Pause* pPause) {
  if (m_ProssiveStatus != FXCODEC_STATUS_DECODE_TOBECONTINUE)
    return m_ProssiveStatus;

  if (m_DecodeType != 1) {
    m_ProssiveStatus = FXCODEC_STATUS_ERROR;
    return m_ProssiveStatus;
  }

  return decode_Arith(pPause);
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template0_opt3(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  if (!m_pLine) {
    m_pLine = pImage->m_pData;
  }
  int32_t nStride = pImage->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  uint32_t height = GBH & 0x7fffffff;
  for (; m_loopIndex < height; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x9b25]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      if (m_loopIndex > 1) {
        uint8_t* pLine1 = m_pLine - nStride2;
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line1 = (*pLine1++) << 6;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = ((line1 & 0xf800) | (line2 & 0x07f0));
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 6);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = (((CONTEXT & 0x7bf7) << 1) | bVal |
                       ((line1 >> k) & 0x0800) | ((line2 >> k) & 0x0010));
          }
          m_pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              (((CONTEXT & 0x7bf7) << 1) | bVal |
               ((line1 >> (7 - k)) & 0x0800) | ((line2 >> (7 - k)) & 0x0010));
        }
        m_pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line2 = (m_loopIndex & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 & 0x07f0);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (m_loopIndex & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT =
                (((CONTEXT & 0x7bf7) << 1) | bVal | ((line2 >> k) & 0x0010));
          }
          m_pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = (((CONTEXT & 0x7bf7) << 1) | bVal |
                     ((line2 >> (7 - k)) & 0x0010));
        }
        m_pLine[nLineBytes] = cVal1;
      }
    }
    m_pLine += nStride;
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template0_unopt(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x9b25]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      uint32_t line1 = pImage->getPixel(1, m_loopIndex - 2);
      line1 |= pImage->getPixel(0, m_loopIndex - 2) << 1;
      uint32_t line2 = pImage->getPixel(2, m_loopIndex - 1);
      line2 |= pImage->getPixel(1, m_loopIndex - 1) << 1;
      line2 |= pImage->getPixel(0, m_loopIndex - 1) << 2;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, m_loopIndex)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= pImage->getPixel(w + GBAT[0], m_loopIndex + GBAT[1]) << 4;
          CONTEXT |= line2 << 5;
          CONTEXT |= pImage->getPixel(w + GBAT[2], m_loopIndex + GBAT[3]) << 10;
          CONTEXT |= pImage->getPixel(w + GBAT[4], m_loopIndex + GBAT[5]) << 11;
          CONTEXT |= line1 << 12;
          CONTEXT |= pImage->getPixel(w + GBAT[6], m_loopIndex + GBAT[7]) << 15;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          pImage->setPixel(w, m_loopIndex, bVal);
        }
        line1 =
            ((line1 << 1) | pImage->getPixel(w + 2, m_loopIndex - 2)) & 0x07;
        line2 =
            ((line2 << 1) | pImage->getPixel(w + 3, m_loopIndex - 1)) & 0x1f;
        line3 = ((line3 << 1) | bVal) & 0x0f;
      }
    }
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template1_opt3(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  if (!m_pLine) {
    m_pLine = pImage->m_pData;
  }
  int32_t nStride = pImage->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x0795]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      if (m_loopIndex > 1) {
        uint8_t* pLine1 = m_pLine - nStride2;
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line1 = (*pLine1++) << 4;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = (line1 & 0x1e00) | ((line2 >> 1) & 0x01f8);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 4);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                      ((line1 >> k) & 0x0200) | ((line2 >> (k + 1)) & 0x0008);
          }
          m_pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                    ((line1 >> (7 - k)) & 0x0200) |
                    ((line2 >> (8 - k)) & 0x0008);
        }
        m_pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line2 = (m_loopIndex & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 >> 1) & 0x01f8;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (m_loopIndex & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x0efb) << 1) | bVal |
                      ((line2 >> (k + 1)) & 0x0008);
          }
          m_pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              ((CONTEXT & 0x0efb) << 1) | bVal | ((line2 >> (8 - k)) & 0x0008);
        }
        m_pLine[nLineBytes] = cVal1;
      }
    }
    m_pLine += nStride;
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template1_unopt(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  for (uint32_t h = 0; h < GBH; h++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x0795]);
    if (m_LTP) {
      pImage->copyLine(h, h - 1);
    } else {
      uint32_t line1 = pImage->getPixel(2, h - 2);
      line1 |= pImage->getPixel(1, h - 2) << 1;
      line1 |= pImage->getPixel(0, h - 2) << 2;
      uint32_t line2 = pImage->getPixel(2, h - 1);
      line2 |= pImage->getPixel(1, h - 1) << 1;
      line2 |= pImage->getPixel(0, h - 1) << 2;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, h)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= pImage->getPixel(w + GBAT[0], h + GBAT[1]) << 3;
          CONTEXT |= line2 << 4;
          CONTEXT |= line1 << 9;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          pImage->setPixel(w, h, bVal);
        }
        line1 = ((line1 << 1) | pImage->getPixel(w + 3, h - 2)) & 0x0f;
        line2 = ((line2 << 1) | pImage->getPixel(w + 3, h - 1)) & 0x1f;
        line3 = ((line3 << 1) | bVal) & 0x07;
      }
    }
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template2_opt3(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  if (!m_pLine) {
    m_pLine = pImage->m_pData;
  }
  int32_t nStride = pImage->stride();
  int32_t nStride2 = nStride << 1;
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x00e5]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      if (m_loopIndex > 1) {
        uint8_t* pLine1 = m_pLine - nStride2;
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line1 = (*pLine1++) << 1;
        uint32_t line2 = *pLine2++;
        uint32_t CONTEXT = (line1 & 0x0380) | ((line2 >> 3) & 0x007c);
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | ((*pLine1++) << 1);
          line2 = (line2 << 8) | (*pLine2++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                      ((line1 >> k) & 0x0080) | ((line2 >> (k + 3)) & 0x0004);
          }
          m_pLine[cc] = cVal;
        }
        line1 <<= 8;
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                    ((line1 >> (7 - k)) & 0x0080) |
                    ((line2 >> (10 - k)) & 0x0004);
        }
        m_pLine[nLineBytes] = cVal1;
      } else {
        uint8_t* pLine2 = m_pLine - nStride;
        uint32_t line2 = (m_loopIndex & 1) ? (*pLine2++) : 0;
        uint32_t CONTEXT = (line2 >> 3) & 0x007c;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          if (m_loopIndex & 1) {
            line2 = (line2 << 8) | (*pLine2++);
          }
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                      ((line2 >> (k + 3)) & 0x0004);
          }
          m_pLine[cc] = cVal;
        }
        line2 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01bd) << 1) | bVal |
                    (((line2 >> (10 - k))) & 0x0004);
        }
        m_pLine[nLineBytes] = cVal1;
      }
    }
    m_pLine += nStride;
    if (pPause && m_loopIndex % 50 == 0 && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template2_unopt(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x00e5]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      uint32_t line1 = pImage->getPixel(1, m_loopIndex - 2);
      line1 |= pImage->getPixel(0, m_loopIndex - 2) << 1;
      uint32_t line2 = pImage->getPixel(1, m_loopIndex - 1);
      line2 |= pImage->getPixel(0, m_loopIndex - 1) << 1;
      uint32_t line3 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, m_loopIndex)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line3;
          CONTEXT |= pImage->getPixel(w + GBAT[0], m_loopIndex + GBAT[1]) << 2;
          CONTEXT |= line2 << 3;
          CONTEXT |= line1 << 7;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          pImage->setPixel(w, m_loopIndex, bVal);
        }
        line1 =
            ((line1 << 1) | pImage->getPixel(w + 2, m_loopIndex - 2)) & 0x07;
        line2 =
            ((line2 << 1) | pImage->getPixel(w + 2, m_loopIndex - 1)) & 0x0f;
        line3 = ((line3 << 1) | bVal) & 0x03;
      }
    }
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template3_opt3(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  if (!m_pLine)
    m_pLine = pImage->m_pData;

  int32_t nStride = pImage->stride();
  int32_t nLineBytes = ((GBW + 7) >> 3) - 1;
  int32_t nBitsLeft = GBW - (nLineBytes << 3);
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x0195]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      if (m_loopIndex > 0) {
        uint8_t* pLine1 = m_pLine - nStride;
        uint32_t line1 = *pLine1++;
        uint32_t CONTEXT = (line1 >> 1) & 0x03f0;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          line1 = (line1 << 8) | (*pLine1++);
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal |
                      ((line1 >> (k + 1)) & 0x0010);
          }
          m_pLine[cc] = cVal;
        }
        line1 <<= 8;
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT =
              ((CONTEXT & 0x01f7) << 1) | bVal | ((line1 >> (8 - k)) & 0x0010);
        }
        m_pLine[nLineBytes] = cVal1;
      } else {
        uint32_t CONTEXT = 0;
        for (int32_t cc = 0; cc < nLineBytes; cc++) {
          uint8_t cVal = 0;
          for (int32_t k = 7; k >= 0; k--) {
            int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
            cVal |= bVal << k;
            CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal;
          }
          m_pLine[cc] = cVal;
        }
        uint8_t cVal1 = 0;
        for (int32_t k = 0; k < nBitsLeft; k++) {
          int bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
          cVal1 |= bVal << (7 - k);
          CONTEXT = ((CONTEXT & 0x01f7) << 1) | bVal;
        }
        m_pLine[nLineBytes] = cVal1;
      }
    }
    m_pLine += nStride;
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}

FXCODEC_STATUS CJBig2_GRDProc::decode_Arith_Template3_unopt(
    CJBig2_Image* pImage,
    CJBig2_ArithDecoder* pArithDecoder,
    JBig2ArithCtx* gbContext,
    IFX_Pause* pPause) {
  for (; m_loopIndex < GBH; m_loopIndex++) {
    if (TPGDON)
      m_LTP = m_LTP ^ pArithDecoder->DECODE(&gbContext[0x0195]);
    if (m_LTP) {
      pImage->copyLine(m_loopIndex, m_loopIndex - 1);
    } else {
      uint32_t line1 = pImage->getPixel(1, m_loopIndex - 1);
      line1 |= pImage->getPixel(0, m_loopIndex - 1) << 1;
      uint32_t line2 = 0;
      for (uint32_t w = 0; w < GBW; w++) {
        int bVal;
        if (USESKIP && SKIP->getPixel(w, m_loopIndex)) {
          bVal = 0;
        } else {
          uint32_t CONTEXT = line2;
          CONTEXT |= pImage->getPixel(w + GBAT[0], m_loopIndex + GBAT[1]) << 4;
          CONTEXT |= line1 << 5;
          bVal = pArithDecoder->DECODE(&gbContext[CONTEXT]);
        }
        if (bVal) {
          pImage->setPixel(w, m_loopIndex, bVal);
        }
        line1 =
            ((line1 << 1) | pImage->getPixel(w + 2, m_loopIndex - 1)) & 0x1f;
        line2 = ((line2 << 1) | bVal) & 0x0f;
      }
    }
    if (pPause && pPause->NeedToPauseNow()) {
      m_loopIndex++;
      m_ProssiveStatus = FXCODEC_STATUS_DECODE_TOBECONTINUE;
      return FXCODEC_STATUS_DECODE_TOBECONTINUE;
    }
  }
  m_ProssiveStatus = FXCODEC_STATUS_DECODE_FINISH;
  return FXCODEC_STATUS_DECODE_FINISH;
}
