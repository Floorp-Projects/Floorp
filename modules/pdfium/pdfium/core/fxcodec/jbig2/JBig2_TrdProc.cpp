// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_TrdProc.h"

#include <memory>

#include "core/fxcodec/jbig2/JBig2_ArithDecoder.h"
#include "core/fxcodec/jbig2/JBig2_ArithIntDecoder.h"
#include "core/fxcodec/jbig2/JBig2_GrrdProc.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanDecoder.h"
#include "third_party/base/ptr_util.h"

CJBig2_Image* CJBig2_TRDProc::decode_Huffman(CJBig2_BitStream* pStream,
                                             JBig2ArithCtx* grContext) {
  std::unique_ptr<CJBig2_HuffmanDecoder> pHuffmanDecoder(
      new CJBig2_HuffmanDecoder(pStream));
  std::unique_ptr<CJBig2_Image> SBREG(new CJBig2_Image(SBW, SBH));
  SBREG->fill(SBDEFPIXEL);
  int32_t STRIPT;
  if (pHuffmanDecoder->decodeAValue(SBHUFFDT, &STRIPT) != 0)
    return nullptr;

  STRIPT *= SBSTRIPS;
  STRIPT = -STRIPT;
  int32_t FIRSTS = 0;
  uint32_t NINSTANCES = 0;
  while (NINSTANCES < SBNUMINSTANCES) {
    int32_t DT;
    if (pHuffmanDecoder->decodeAValue(SBHUFFDT, &DT) != 0)
      return nullptr;

    DT *= SBSTRIPS;
    STRIPT = STRIPT + DT;
    bool bFirst = true;
    int32_t CURS = 0;
    for (;;) {
      if (bFirst) {
        int32_t DFS;
        if (pHuffmanDecoder->decodeAValue(SBHUFFFS, &DFS) != 0)
          return nullptr;

        FIRSTS = FIRSTS + DFS;
        CURS = FIRSTS;
        bFirst = false;
      } else {
        int32_t IDS;
        int32_t nVal = pHuffmanDecoder->decodeAValue(SBHUFFDS, &IDS);
        if (nVal == JBIG2_OOB) {
          break;
        } else if (nVal != 0) {
          return nullptr;
        } else {
          CURS = CURS + IDS + SBDSOFFSET;
        }
      }
      uint8_t CURT = 0;
      if (SBSTRIPS != 1) {
        uint32_t nTmp = 1;
        while ((uint32_t)(1 << nTmp) < SBSTRIPS) {
          nTmp++;
        }
        int32_t nVal;
        if (pStream->readNBits(nTmp, &nVal) != 0)
          return nullptr;

        CURT = nVal;
      }
      int32_t TI = STRIPT + CURT;
      int32_t nVal = 0;
      int32_t nBits = 0;
      uint32_t IDI;
      for (;;) {
        uint32_t nTmp;
        if (pStream->read1Bit(&nTmp) != 0)
          return nullptr;

        nVal = (nVal << 1) | nTmp;
        nBits++;
        for (IDI = 0; IDI < SBNUMSYMS; IDI++) {
          if ((nBits == SBSYMCODES[IDI].codelen) &&
              (nVal == SBSYMCODES[IDI].code)) {
            break;
          }
        }
        if (IDI < SBNUMSYMS) {
          break;
        }
      }
      bool RI = 0;
      if (SBREFINE != 0 && pStream->read1Bit(&RI) != 0) {
        return nullptr;
      }
      CJBig2_Image* IBI = nullptr;
      if (RI == 0) {
        IBI = SBSYMS[IDI];
      } else {
        int32_t RDWI;
        int32_t RDHI;
        int32_t RDXI;
        int32_t RDYI;
        if ((pHuffmanDecoder->decodeAValue(SBHUFFRDW, &RDWI) != 0) ||
            (pHuffmanDecoder->decodeAValue(SBHUFFRDH, &RDHI) != 0) ||
            (pHuffmanDecoder->decodeAValue(SBHUFFRDX, &RDXI) != 0) ||
            (pHuffmanDecoder->decodeAValue(SBHUFFRDY, &RDYI) != 0) ||
            (pHuffmanDecoder->decodeAValue(SBHUFFRSIZE, &nVal) != 0)) {
          return nullptr;
        }
        pStream->alignByte();
        uint32_t nTmp = pStream->getOffset();
        CJBig2_Image* IBOI = SBSYMS[IDI];
        if (!IBOI)
          return nullptr;

        uint32_t WOI = IBOI->width();
        uint32_t HOI = IBOI->height();
        if ((int)(WOI + RDWI) < 0 || (int)(HOI + RDHI) < 0)
          return nullptr;

        std::unique_ptr<CJBig2_GRRDProc> pGRRD(new CJBig2_GRRDProc());
        pGRRD->GRW = WOI + RDWI;
        pGRRD->GRH = HOI + RDHI;
        pGRRD->GRTEMPLATE = SBRTEMPLATE;
        pGRRD->GRREFERENCE = IBOI;
        pGRRD->GRREFERENCEDX = (RDWI >> 2) + RDXI;
        pGRRD->GRREFERENCEDY = (RDHI >> 2) + RDYI;
        pGRRD->TPGRON = 0;
        pGRRD->GRAT[0] = SBRAT[0];
        pGRRD->GRAT[1] = SBRAT[1];
        pGRRD->GRAT[2] = SBRAT[2];
        pGRRD->GRAT[3] = SBRAT[3];

        {
          std::unique_ptr<CJBig2_ArithDecoder> pArithDecoder(
              new CJBig2_ArithDecoder(pStream));
          IBI = pGRRD->decode(pArithDecoder.get(), grContext);
          if (!IBI)
            return nullptr;
        }

        pStream->alignByte();
        pStream->offset(2);
        if ((uint32_t)nVal != (pStream->getOffset() - nTmp)) {
          delete IBI;
          return nullptr;
        }
      }
      if (!IBI) {
        continue;
      }
      uint32_t WI = IBI->width();
      uint32_t HI = IBI->height();
      if (TRANSPOSED == 0 && ((REFCORNER == JBIG2_CORNER_TOPRIGHT) ||
                              (REFCORNER == JBIG2_CORNER_BOTTOMRIGHT))) {
        CURS = CURS + WI - 1;
      } else if (TRANSPOSED == 1 && ((REFCORNER == JBIG2_CORNER_BOTTOMLEFT) ||
                                     (REFCORNER == JBIG2_CORNER_BOTTOMRIGHT))) {
        CURS = CURS + HI - 1;
      }
      int32_t SI = CURS;
      if (TRANSPOSED == 0) {
        switch (REFCORNER) {
          case JBIG2_CORNER_TOPLEFT:
            SBREG->composeFrom(SI, TI, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_TOPRIGHT:
            SBREG->composeFrom(SI - WI + 1, TI, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMLEFT:
            SBREG->composeFrom(SI, TI - HI + 1, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMRIGHT:
            SBREG->composeFrom(SI - WI + 1, TI - HI + 1, IBI, SBCOMBOP);
            break;
        }
      } else {
        switch (REFCORNER) {
          case JBIG2_CORNER_TOPLEFT:
            SBREG->composeFrom(TI, SI, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_TOPRIGHT:
            SBREG->composeFrom(TI - WI + 1, SI, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMLEFT:
            SBREG->composeFrom(TI, SI - HI + 1, IBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMRIGHT:
            SBREG->composeFrom(TI - WI + 1, SI - HI + 1, IBI, SBCOMBOP);
            break;
        }
      }
      if (RI != 0) {
        delete IBI;
      }
      if (TRANSPOSED == 0 && ((REFCORNER == JBIG2_CORNER_TOPLEFT) ||
                              (REFCORNER == JBIG2_CORNER_BOTTOMLEFT))) {
        CURS = CURS + WI - 1;
      } else if (TRANSPOSED == 1 && ((REFCORNER == JBIG2_CORNER_TOPLEFT) ||
                                     (REFCORNER == JBIG2_CORNER_TOPRIGHT))) {
        CURS = CURS + HI - 1;
      }
      NINSTANCES = NINSTANCES + 1;
    }
  }
  return SBREG.release();
}

CJBig2_Image* CJBig2_TRDProc::decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                                           JBig2ArithCtx* grContext,
                                           JBig2IntDecoderState* pIDS) {
  std::unique_ptr<CJBig2_ArithIntDecoder> IADT;
  std::unique_ptr<CJBig2_ArithIntDecoder> IAFS;
  std::unique_ptr<CJBig2_ArithIntDecoder> IADS;
  std::unique_ptr<CJBig2_ArithIntDecoder> IAIT;
  std::unique_ptr<CJBig2_ArithIntDecoder> IARI;
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDW;
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDH;
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDX;
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDY;
  std::unique_ptr<CJBig2_ArithIaidDecoder> IAID;
  CJBig2_ArithIntDecoder* pIADT;
  CJBig2_ArithIntDecoder* pIAFS;
  CJBig2_ArithIntDecoder* pIADS;
  CJBig2_ArithIntDecoder* pIAIT;
  CJBig2_ArithIntDecoder* pIARI;
  CJBig2_ArithIntDecoder* pIARDW;
  CJBig2_ArithIntDecoder* pIARDH;
  CJBig2_ArithIntDecoder* pIARDX;
  CJBig2_ArithIntDecoder* pIARDY;
  CJBig2_ArithIaidDecoder* pIAID;
  if (pIDS) {
    pIADT = pIDS->IADT;
    pIAFS = pIDS->IAFS;
    pIADS = pIDS->IADS;
    pIAIT = pIDS->IAIT;
    pIARI = pIDS->IARI;
    pIARDW = pIDS->IARDW;
    pIARDH = pIDS->IARDH;
    pIARDX = pIDS->IARDX;
    pIARDY = pIDS->IARDY;
    pIAID = pIDS->IAID;
  } else {
    IADT = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IAFS = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IADS = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IAIT = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IARI = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IARDW = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IARDH = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IARDX = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IARDY = pdfium::MakeUnique<CJBig2_ArithIntDecoder>();
    IAID = pdfium::MakeUnique<CJBig2_ArithIaidDecoder>(SBSYMCODELEN);
    pIADT = IADT.get();
    pIAFS = IAFS.get();
    pIADS = IADS.get();
    pIAIT = IAIT.get();
    pIARI = IARI.get();
    pIARDW = IARDW.get();
    pIARDH = IARDH.get();
    pIARDX = IARDX.get();
    pIARDY = IARDY.get();
    pIAID = IAID.get();
  }
  std::unique_ptr<CJBig2_Image> SBREG(new CJBig2_Image(SBW, SBH));
  SBREG->fill(SBDEFPIXEL);
  int32_t STRIPT;
  if (!pIADT->decode(pArithDecoder, &STRIPT))
    return nullptr;
  STRIPT *= SBSTRIPS;
  STRIPT = -STRIPT;
  int32_t FIRSTS = 0;
  uint32_t NINSTANCES = 0;
  while (NINSTANCES < SBNUMINSTANCES) {
    int32_t CURS = 0;
    int32_t DT;
    if (!pIADT->decode(pArithDecoder, &DT))
      return nullptr;
    DT *= SBSTRIPS;
    STRIPT += DT;
    bool bFirst = true;
    for (;;) {
      if (bFirst) {
        int32_t DFS;
        pIAFS->decode(pArithDecoder, &DFS);
        FIRSTS += DFS;
        CURS = FIRSTS;
        bFirst = false;
      } else {
        int32_t IDS;
        if (!pIADS->decode(pArithDecoder, &IDS))
          break;
        CURS += IDS + SBDSOFFSET;
      }
      if (NINSTANCES >= SBNUMINSTANCES) {
        break;
      }
      int CURT = 0;
      if (SBSTRIPS != 1)
        pIAIT->decode(pArithDecoder, &CURT);

      int32_t TI = STRIPT + CURT;
      uint32_t IDI;
      pIAID->decode(pArithDecoder, &IDI);
      if (IDI >= SBNUMSYMS)
        return nullptr;

      int RI;
      if (SBREFINE == 0)
        RI = 0;
      else
        pIARI->decode(pArithDecoder, &RI);

      std::unique_ptr<CJBig2_Image> IBI;
      CJBig2_Image* pIBI;
      if (RI == 0) {
        pIBI = SBSYMS[IDI];
      } else {
        int32_t RDWI;
        int32_t RDHI;
        int32_t RDXI;
        int32_t RDYI;
        pIARDW->decode(pArithDecoder, &RDWI);
        pIARDH->decode(pArithDecoder, &RDHI);
        pIARDX->decode(pArithDecoder, &RDXI);
        pIARDY->decode(pArithDecoder, &RDYI);
        CJBig2_Image* IBOI = SBSYMS[IDI];
        if (!IBOI)
          return nullptr;

        uint32_t WOI = IBOI->width();
        uint32_t HOI = IBOI->height();
        if ((int)(WOI + RDWI) < 0 || (int)(HOI + RDHI) < 0)
          return nullptr;

        std::unique_ptr<CJBig2_GRRDProc> pGRRD(new CJBig2_GRRDProc());
        pGRRD->GRW = WOI + RDWI;
        pGRRD->GRH = HOI + RDHI;
        pGRRD->GRTEMPLATE = SBRTEMPLATE;
        pGRRD->GRREFERENCE = IBOI;
        pGRRD->GRREFERENCEDX = (RDWI >> 1) + RDXI;
        pGRRD->GRREFERENCEDY = (RDHI >> 1) + RDYI;
        pGRRD->TPGRON = 0;
        pGRRD->GRAT[0] = SBRAT[0];
        pGRRD->GRAT[1] = SBRAT[1];
        pGRRD->GRAT[2] = SBRAT[2];
        pGRRD->GRAT[3] = SBRAT[3];
        IBI.reset(pGRRD->decode(pArithDecoder, grContext));
        pIBI = IBI.get();
      }
      if (!pIBI)
        return nullptr;

      uint32_t WI = pIBI->width();
      uint32_t HI = pIBI->height();
      if (TRANSPOSED == 0 && ((REFCORNER == JBIG2_CORNER_TOPRIGHT) ||
                              (REFCORNER == JBIG2_CORNER_BOTTOMRIGHT))) {
        CURS += WI - 1;
      } else if (TRANSPOSED == 1 && ((REFCORNER == JBIG2_CORNER_BOTTOMLEFT) ||
                                     (REFCORNER == JBIG2_CORNER_BOTTOMRIGHT))) {
        CURS += HI - 1;
      }
      int32_t SI = CURS;
      if (TRANSPOSED == 0) {
        switch (REFCORNER) {
          case JBIG2_CORNER_TOPLEFT:
            SBREG->composeFrom(SI, TI, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_TOPRIGHT:
            SBREG->composeFrom(SI - WI + 1, TI, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMLEFT:
            SBREG->composeFrom(SI, TI - HI + 1, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMRIGHT:
            SBREG->composeFrom(SI - WI + 1, TI - HI + 1, pIBI, SBCOMBOP);
            break;
        }
      } else {
        switch (REFCORNER) {
          case JBIG2_CORNER_TOPLEFT:
            SBREG->composeFrom(TI, SI, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_TOPRIGHT:
            SBREG->composeFrom(TI - WI + 1, SI, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMLEFT:
            SBREG->composeFrom(TI, SI - HI + 1, pIBI, SBCOMBOP);
            break;
          case JBIG2_CORNER_BOTTOMRIGHT:
            SBREG->composeFrom(TI - WI + 1, SI - HI + 1, pIBI, SBCOMBOP);
            break;
        }
      }
      if (TRANSPOSED == 0 && ((REFCORNER == JBIG2_CORNER_TOPLEFT) ||
                              (REFCORNER == JBIG2_CORNER_BOTTOMLEFT))) {
        CURS += WI - 1;
      } else if (TRANSPOSED == 1 && ((REFCORNER == JBIG2_CORNER_TOPLEFT) ||
                                     (REFCORNER == JBIG2_CORNER_TOPRIGHT))) {
        CURS += HI - 1;
      }
      ++NINSTANCES;
    }
  }
  return SBREG.release();
}
