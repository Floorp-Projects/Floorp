// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_SddProc.h"

#include <memory>
#include <vector>

#include "core/fxcodec/jbig2/JBig2_ArithIntDecoder.h"
#include "core/fxcodec/jbig2/JBig2_GrdProc.h"
#include "core/fxcodec/jbig2/JBig2_GrrdProc.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanDecoder.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanTable.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanTable_Standard.h"
#include "core/fxcodec/jbig2/JBig2_SymbolDict.h"
#include "core/fxcodec/jbig2/JBig2_TrdProc.h"
#include "core/fxcrt/fx_basic.h"
#include "third_party/base/ptr_util.h"

CJBig2_SymbolDict* CJBig2_SDDProc::decode_Arith(
    CJBig2_ArithDecoder* pArithDecoder,
    std::vector<JBig2ArithCtx>* gbContext,
    std::vector<JBig2ArithCtx>* grContext) {
  CJBig2_Image** SDNEWSYMS;
  uint32_t HCHEIGHT, NSYMSDECODED;
  int32_t HCDH;
  uint32_t SYMWIDTH, TOTWIDTH;
  int32_t DW;
  CJBig2_Image* BS;
  uint32_t I, J, REFAGGNINST;
  bool* EXFLAGS;
  uint32_t EXINDEX;
  bool CUREXFLAG;
  uint32_t EXRUNLENGTH;
  uint32_t nTmp;
  uint32_t SBNUMSYMS;
  uint8_t SBSYMCODELEN;
  int32_t RDXI, RDYI;
  uint32_t num_ex_syms;
  CJBig2_Image** SBSYMS;
  std::unique_ptr<CJBig2_ArithIaidDecoder> IAID;
  std::unique_ptr<CJBig2_SymbolDict> pDict;
  std::unique_ptr<CJBig2_ArithIntDecoder> IADH(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IADW(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IAAI(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDX(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDY(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IAEX(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IADT(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IAFS(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IADS(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IAIT(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IARI(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDW(new CJBig2_ArithIntDecoder);
  std::unique_ptr<CJBig2_ArithIntDecoder> IARDH(new CJBig2_ArithIntDecoder);
  nTmp = 0;
  while ((uint32_t)(1 << nTmp) < (SDNUMINSYMS + SDNUMNEWSYMS)) {
    nTmp++;
  }
  IAID = pdfium::MakeUnique<CJBig2_ArithIaidDecoder>((uint8_t)nTmp);
  SDNEWSYMS = FX_Alloc(CJBig2_Image*, SDNUMNEWSYMS);
  FXSYS_memset(SDNEWSYMS, 0, SDNUMNEWSYMS * sizeof(CJBig2_Image*));

  HCHEIGHT = 0;
  NSYMSDECODED = 0;
  while (NSYMSDECODED < SDNUMNEWSYMS) {
    BS = nullptr;
    IADH->decode(pArithDecoder, &HCDH);
    HCHEIGHT = HCHEIGHT + HCDH;
    if ((int)HCHEIGHT < 0 || (int)HCHEIGHT > JBIG2_MAX_IMAGE_SIZE) {
      goto failed;
    }
    SYMWIDTH = 0;
    TOTWIDTH = 0;
    for (;;) {
      if (!IADW->decode(pArithDecoder, &DW))
        break;

      if (NSYMSDECODED >= SDNUMNEWSYMS)
        goto failed;

      SYMWIDTH = SYMWIDTH + DW;
      if ((int)SYMWIDTH < 0 || (int)SYMWIDTH > JBIG2_MAX_IMAGE_SIZE)
        goto failed;

      if (HCHEIGHT == 0 || SYMWIDTH == 0) {
        TOTWIDTH = TOTWIDTH + SYMWIDTH;
        SDNEWSYMS[NSYMSDECODED] = nullptr;
        NSYMSDECODED = NSYMSDECODED + 1;
        continue;
      }
      TOTWIDTH = TOTWIDTH + SYMWIDTH;
      if (SDREFAGG == 0) {
        std::unique_ptr<CJBig2_GRDProc> pGRD(new CJBig2_GRDProc());
        pGRD->MMR = 0;
        pGRD->GBW = SYMWIDTH;
        pGRD->GBH = HCHEIGHT;
        pGRD->GBTEMPLATE = SDTEMPLATE;
        pGRD->TPGDON = 0;
        pGRD->USESKIP = 0;
        pGRD->GBAT[0] = SDAT[0];
        pGRD->GBAT[1] = SDAT[1];
        pGRD->GBAT[2] = SDAT[2];
        pGRD->GBAT[3] = SDAT[3];
        pGRD->GBAT[4] = SDAT[4];
        pGRD->GBAT[5] = SDAT[5];
        pGRD->GBAT[6] = SDAT[6];
        pGRD->GBAT[7] = SDAT[7];
        BS = pGRD->decode_Arith(pArithDecoder, gbContext->data());
        if (!BS) {
          goto failed;
        }
      } else {
        IAAI->decode(pArithDecoder, (int*)&REFAGGNINST);
        if (REFAGGNINST > 1) {
          std::unique_ptr<CJBig2_TRDProc> pDecoder(new CJBig2_TRDProc());
          pDecoder->SBHUFF = SDHUFF;
          pDecoder->SBREFINE = 1;
          pDecoder->SBW = SYMWIDTH;
          pDecoder->SBH = HCHEIGHT;
          pDecoder->SBNUMINSTANCES = REFAGGNINST;
          pDecoder->SBSTRIPS = 1;
          pDecoder->SBNUMSYMS = SDNUMINSYMS + NSYMSDECODED;
          SBNUMSYMS = pDecoder->SBNUMSYMS;
          nTmp = 0;
          while ((uint32_t)(1 << nTmp) < SBNUMSYMS) {
            nTmp++;
          }
          SBSYMCODELEN = (uint8_t)nTmp;
          pDecoder->SBSYMCODELEN = SBSYMCODELEN;
          SBSYMS = FX_Alloc(CJBig2_Image*, SBNUMSYMS);
          JBIG2_memcpy(SBSYMS, SDINSYMS, SDNUMINSYMS * sizeof(CJBig2_Image*));
          JBIG2_memcpy(SBSYMS + SDNUMINSYMS, SDNEWSYMS,
                       NSYMSDECODED * sizeof(CJBig2_Image*));
          pDecoder->SBSYMS = SBSYMS;
          pDecoder->SBDEFPIXEL = 0;
          pDecoder->SBCOMBOP = JBIG2_COMPOSE_OR;
          pDecoder->TRANSPOSED = 0;
          pDecoder->REFCORNER = JBIG2_CORNER_TOPLEFT;
          pDecoder->SBDSOFFSET = 0;
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFFS(new CJBig2_HuffmanTable(
              HuffmanTable_B6, HuffmanTable_B6_Size, HuffmanTable_HTOOB_B6));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFDS(new CJBig2_HuffmanTable(
              HuffmanTable_B8, HuffmanTable_B8_Size, HuffmanTable_HTOOB_B8));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFDT(new CJBig2_HuffmanTable(
              HuffmanTable_B11, HuffmanTable_B11_Size, HuffmanTable_HTOOB_B11));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDW(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDH(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDX(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDY(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRSIZE(
              new CJBig2_HuffmanTable(HuffmanTable_B1, HuffmanTable_B1_Size,
                                      HuffmanTable_HTOOB_B1));
          pDecoder->SBHUFFFS = SBHUFFFS.get();
          pDecoder->SBHUFFDS = SBHUFFDS.get();
          pDecoder->SBHUFFDT = SBHUFFDT.get();
          pDecoder->SBHUFFRDW = SBHUFFRDW.get();
          pDecoder->SBHUFFRDH = SBHUFFRDH.get();
          pDecoder->SBHUFFRDX = SBHUFFRDX.get();
          pDecoder->SBHUFFRDY = SBHUFFRDY.get();
          pDecoder->SBHUFFRSIZE = SBHUFFRSIZE.get();
          pDecoder->SBRTEMPLATE = SDRTEMPLATE;
          pDecoder->SBRAT[0] = SDRAT[0];
          pDecoder->SBRAT[1] = SDRAT[1];
          pDecoder->SBRAT[2] = SDRAT[2];
          pDecoder->SBRAT[3] = SDRAT[3];
          JBig2IntDecoderState ids;
          ids.IADT = IADT.get();
          ids.IAFS = IAFS.get();
          ids.IADS = IADS.get();
          ids.IAIT = IAIT.get();
          ids.IARI = IARI.get();
          ids.IARDW = IARDW.get();
          ids.IARDH = IARDH.get();
          ids.IARDX = IARDX.get();
          ids.IARDY = IARDY.get();
          ids.IAID = IAID.get();
          BS = pDecoder->decode_Arith(pArithDecoder, grContext->data(), &ids);
          if (!BS) {
            FX_Free(SBSYMS);
            goto failed;
          }
          FX_Free(SBSYMS);
        } else if (REFAGGNINST == 1) {
          SBNUMSYMS = SDNUMINSYMS + NSYMSDECODED;
          uint32_t IDI;
          IAID->decode(pArithDecoder, &IDI);
          IARDX->decode(pArithDecoder, &RDXI);
          IARDY->decode(pArithDecoder, &RDYI);
          if (IDI >= SBNUMSYMS) {
            goto failed;
          }
          SBSYMS = FX_Alloc(CJBig2_Image*, SBNUMSYMS);
          JBIG2_memcpy(SBSYMS, SDINSYMS, SDNUMINSYMS * sizeof(CJBig2_Image*));
          JBIG2_memcpy(SBSYMS + SDNUMINSYMS, SDNEWSYMS,
                       NSYMSDECODED * sizeof(CJBig2_Image*));
          if (!SBSYMS[IDI]) {
            FX_Free(SBSYMS);
            goto failed;
          }
          std::unique_ptr<CJBig2_GRRDProc> pGRRD(new CJBig2_GRRDProc());
          pGRRD->GRW = SYMWIDTH;
          pGRRD->GRH = HCHEIGHT;
          pGRRD->GRTEMPLATE = SDRTEMPLATE;
          pGRRD->GRREFERENCE = SBSYMS[IDI];
          pGRRD->GRREFERENCEDX = RDXI;
          pGRRD->GRREFERENCEDY = RDYI;
          pGRRD->TPGRON = 0;
          pGRRD->GRAT[0] = SDRAT[0];
          pGRRD->GRAT[1] = SDRAT[1];
          pGRRD->GRAT[2] = SDRAT[2];
          pGRRD->GRAT[3] = SDRAT[3];
          BS = pGRRD->decode(pArithDecoder, grContext->data());
          if (!BS) {
            FX_Free(SBSYMS);
            goto failed;
          }
          FX_Free(SBSYMS);
        }
      }
      SDNEWSYMS[NSYMSDECODED] = BS;
      BS = nullptr;
      NSYMSDECODED = NSYMSDECODED + 1;
    }
  }
  EXINDEX = 0;
  CUREXFLAG = 0;
  EXFLAGS = FX_Alloc(bool, SDNUMINSYMS + SDNUMNEWSYMS);
  num_ex_syms = 0;
  while (EXINDEX < SDNUMINSYMS + SDNUMNEWSYMS) {
    IAEX->decode(pArithDecoder, (int*)&EXRUNLENGTH);
    if (EXINDEX + EXRUNLENGTH > SDNUMINSYMS + SDNUMNEWSYMS) {
      FX_Free(EXFLAGS);
      goto failed;
    }
    if (EXRUNLENGTH != 0) {
      for (I = EXINDEX; I < EXINDEX + EXRUNLENGTH; I++) {
        if (CUREXFLAG)
          num_ex_syms++;
        EXFLAGS[I] = CUREXFLAG;
      }
    }
    EXINDEX = EXINDEX + EXRUNLENGTH;
    CUREXFLAG = !CUREXFLAG;
  }
  if (num_ex_syms > SDNUMEXSYMS) {
    FX_Free(EXFLAGS);
    goto failed;
  }

  pDict = pdfium::MakeUnique<CJBig2_SymbolDict>();
  I = J = 0;
  for (I = 0; I < SDNUMINSYMS + SDNUMNEWSYMS; I++) {
    if (EXFLAGS[I] && J < SDNUMEXSYMS) {
      if (I < SDNUMINSYMS) {
        pDict->AddImage(SDINSYMS[I]
                            ? pdfium::MakeUnique<CJBig2_Image>(*SDINSYMS[I])
                            : nullptr);
      } else {
        pDict->AddImage(pdfium::WrapUnique(SDNEWSYMS[I - SDNUMINSYMS]));
      }
      ++J;
    } else if (!EXFLAGS[I] && I >= SDNUMINSYMS) {
      delete SDNEWSYMS[I - SDNUMINSYMS];
    }
  }
  FX_Free(EXFLAGS);
  FX_Free(SDNEWSYMS);
  return pDict.release();
failed:
  for (I = 0; I < NSYMSDECODED; I++) {
    if (SDNEWSYMS[I]) {
      delete SDNEWSYMS[I];
      SDNEWSYMS[I] = nullptr;
    }
  }
  FX_Free(SDNEWSYMS);
  return nullptr;
}

CJBig2_SymbolDict* CJBig2_SDDProc::decode_Huffman(
    CJBig2_BitStream* pStream,
    std::vector<JBig2ArithCtx>* gbContext,
    std::vector<JBig2ArithCtx>* grContext,
    IFX_Pause* pPause) {
  CJBig2_Image** SDNEWSYMS;
  uint32_t* SDNEWSYMWIDTHS;
  uint32_t HCHEIGHT, NSYMSDECODED;
  int32_t HCDH;
  uint32_t SYMWIDTH, TOTWIDTH, HCFIRSTSYM;
  int32_t DW;
  CJBig2_Image *BS, *BHC;
  uint32_t I, J, REFAGGNINST;
  bool* EXFLAGS;
  uint32_t EXINDEX;
  bool CUREXFLAG;
  uint32_t EXRUNLENGTH;
  int32_t nVal, nBits;
  uint32_t nTmp;
  uint32_t SBNUMSYMS;
  uint8_t SBSYMCODELEN;
  JBig2HuffmanCode* SBSYMCODES;
  uint32_t IDI;
  int32_t RDXI, RDYI;
  uint32_t BMSIZE;
  uint32_t stride;
  uint32_t num_ex_syms;
  CJBig2_Image** SBSYMS;
  std::unique_ptr<CJBig2_HuffmanDecoder> pHuffmanDecoder(
      new CJBig2_HuffmanDecoder(pStream));
  SDNEWSYMS = FX_Alloc(CJBig2_Image*, SDNUMNEWSYMS);
  FXSYS_memset(SDNEWSYMS, 0, SDNUMNEWSYMS * sizeof(CJBig2_Image*));
  SDNEWSYMWIDTHS = nullptr;
  BHC = nullptr;
  if (SDREFAGG == 0) {
    SDNEWSYMWIDTHS = FX_Alloc(uint32_t, SDNUMNEWSYMS);
    FXSYS_memset(SDNEWSYMWIDTHS, 0, SDNUMNEWSYMS * sizeof(uint32_t));
  }
  std::unique_ptr<CJBig2_SymbolDict> pDict(new CJBig2_SymbolDict());
  std::unique_ptr<CJBig2_HuffmanTable> pTable;

  HCHEIGHT = 0;
  NSYMSDECODED = 0;
  BS = nullptr;
  while (NSYMSDECODED < SDNUMNEWSYMS) {
    if (pHuffmanDecoder->decodeAValue(SDHUFFDH, &HCDH) != 0) {
      goto failed;
    }
    HCHEIGHT = HCHEIGHT + HCDH;
    if ((int)HCHEIGHT < 0 || (int)HCHEIGHT > JBIG2_MAX_IMAGE_SIZE) {
      goto failed;
    }
    SYMWIDTH = 0;
    TOTWIDTH = 0;
    HCFIRSTSYM = NSYMSDECODED;
    for (;;) {
      nVal = pHuffmanDecoder->decodeAValue(SDHUFFDW, &DW);
      if (nVal == JBIG2_OOB) {
        break;
      } else if (nVal != 0) {
        goto failed;
      } else {
        if (NSYMSDECODED >= SDNUMNEWSYMS) {
          goto failed;
        }
        SYMWIDTH = SYMWIDTH + DW;
        if ((int)SYMWIDTH < 0 || (int)SYMWIDTH > JBIG2_MAX_IMAGE_SIZE) {
          goto failed;
        } else if (HCHEIGHT == 0 || SYMWIDTH == 0) {
          TOTWIDTH = TOTWIDTH + SYMWIDTH;
          SDNEWSYMS[NSYMSDECODED] = nullptr;
          NSYMSDECODED = NSYMSDECODED + 1;
          continue;
        }
        TOTWIDTH = TOTWIDTH + SYMWIDTH;
      }
      if (SDREFAGG == 1) {
        if (pHuffmanDecoder->decodeAValue(SDHUFFAGGINST, (int*)&REFAGGNINST) !=
            0) {
          goto failed;
        }
        BS = nullptr;
        if (REFAGGNINST > 1) {
          std::unique_ptr<CJBig2_TRDProc> pDecoder(new CJBig2_TRDProc());
          pDecoder->SBHUFF = SDHUFF;
          pDecoder->SBREFINE = 1;
          pDecoder->SBW = SYMWIDTH;
          pDecoder->SBH = HCHEIGHT;
          pDecoder->SBNUMINSTANCES = REFAGGNINST;
          pDecoder->SBSTRIPS = 1;
          pDecoder->SBNUMSYMS = SDNUMINSYMS + NSYMSDECODED;
          SBNUMSYMS = pDecoder->SBNUMSYMS;
          SBSYMCODES = FX_Alloc(JBig2HuffmanCode, SBNUMSYMS);
          nTmp = 1;
          while ((uint32_t)(1 << nTmp) < SBNUMSYMS) {
            nTmp++;
          }
          for (I = 0; I < SBNUMSYMS; I++) {
            SBSYMCODES[I].codelen = nTmp;
            SBSYMCODES[I].code = I;
          }
          pDecoder->SBSYMCODES = SBSYMCODES;
          SBSYMS = FX_Alloc(CJBig2_Image*, SBNUMSYMS);
          JBIG2_memcpy(SBSYMS, SDINSYMS, SDNUMINSYMS * sizeof(CJBig2_Image*));
          JBIG2_memcpy(SBSYMS + SDNUMINSYMS, SDNEWSYMS,
                       NSYMSDECODED * sizeof(CJBig2_Image*));
          pDecoder->SBSYMS = SBSYMS;
          pDecoder->SBDEFPIXEL = 0;
          pDecoder->SBCOMBOP = JBIG2_COMPOSE_OR;
          pDecoder->TRANSPOSED = 0;
          pDecoder->REFCORNER = JBIG2_CORNER_TOPLEFT;
          pDecoder->SBDSOFFSET = 0;
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFFS(new CJBig2_HuffmanTable(
              HuffmanTable_B6, HuffmanTable_B6_Size, HuffmanTable_HTOOB_B6));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFDS(new CJBig2_HuffmanTable(
              HuffmanTable_B8, HuffmanTable_B8_Size, HuffmanTable_HTOOB_B8));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFDT(new CJBig2_HuffmanTable(
              HuffmanTable_B11, HuffmanTable_B11_Size, HuffmanTable_HTOOB_B11));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDW(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDH(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDX(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDY(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRSIZE(
              new CJBig2_HuffmanTable(HuffmanTable_B1, HuffmanTable_B1_Size,
                                      HuffmanTable_HTOOB_B1));
          pDecoder->SBHUFFFS = SBHUFFFS.get();
          pDecoder->SBHUFFDS = SBHUFFDS.get();
          pDecoder->SBHUFFDT = SBHUFFDT.get();
          pDecoder->SBHUFFRDW = SBHUFFRDW.get();
          pDecoder->SBHUFFRDH = SBHUFFRDH.get();
          pDecoder->SBHUFFRDX = SBHUFFRDX.get();
          pDecoder->SBHUFFRDY = SBHUFFRDY.get();
          pDecoder->SBHUFFRSIZE = SBHUFFRSIZE.get();
          pDecoder->SBRTEMPLATE = SDRTEMPLATE;
          pDecoder->SBRAT[0] = SDRAT[0];
          pDecoder->SBRAT[1] = SDRAT[1];
          pDecoder->SBRAT[2] = SDRAT[2];
          pDecoder->SBRAT[3] = SDRAT[3];
          BS = pDecoder->decode_Huffman(pStream, grContext->data());
          if (!BS) {
            FX_Free(SBSYMCODES);
            FX_Free(SBSYMS);
            goto failed;
          }
          FX_Free(SBSYMCODES);
          FX_Free(SBSYMS);
        } else if (REFAGGNINST == 1) {
          SBNUMSYMS = SDNUMINSYMS + SDNUMNEWSYMS;
          nTmp = 1;
          while ((uint32_t)(1 << nTmp) < SBNUMSYMS) {
            nTmp++;
          }
          SBSYMCODELEN = (uint8_t)nTmp;
          SBSYMCODES = FX_Alloc(JBig2HuffmanCode, SBNUMSYMS);
          for (I = 0; I < SBNUMSYMS; I++) {
            SBSYMCODES[I].codelen = SBSYMCODELEN;
            SBSYMCODES[I].code = I;
          }
          nVal = 0;
          nBits = 0;
          for (;;) {
            if (pStream->read1Bit(&nTmp) != 0) {
              FX_Free(SBSYMCODES);
              goto failed;
            }
            nVal = (nVal << 1) | nTmp;
            for (IDI = 0; IDI < SBNUMSYMS; IDI++) {
              if ((nVal == SBSYMCODES[IDI].code) &&
                  (nBits == SBSYMCODES[IDI].codelen)) {
                break;
              }
            }
            if (IDI < SBNUMSYMS) {
              break;
            }
          }
          FX_Free(SBSYMCODES);
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRDX(
              new CJBig2_HuffmanTable(HuffmanTable_B15, HuffmanTable_B15_Size,
                                      HuffmanTable_HTOOB_B15));
          std::unique_ptr<CJBig2_HuffmanTable> SBHUFFRSIZE(
              new CJBig2_HuffmanTable(HuffmanTable_B1, HuffmanTable_B1_Size,
                                      HuffmanTable_HTOOB_B1));
          if ((pHuffmanDecoder->decodeAValue(SBHUFFRDX.get(), &RDXI) != 0) ||
              (pHuffmanDecoder->decodeAValue(SBHUFFRDX.get(), &RDYI) != 0) ||
              (pHuffmanDecoder->decodeAValue(SBHUFFRSIZE.get(), &nVal) != 0)) {
            goto failed;
          }
          pStream->alignByte();
          nTmp = pStream->getOffset();
          SBSYMS = FX_Alloc(CJBig2_Image*, SBNUMSYMS);
          JBIG2_memcpy(SBSYMS, SDINSYMS, SDNUMINSYMS * sizeof(CJBig2_Image*));
          JBIG2_memcpy(SBSYMS + SDNUMINSYMS, SDNEWSYMS,
                       NSYMSDECODED * sizeof(CJBig2_Image*));
          std::unique_ptr<CJBig2_GRRDProc> pGRRD(new CJBig2_GRRDProc());
          pGRRD->GRW = SYMWIDTH;
          pGRRD->GRH = HCHEIGHT;
          pGRRD->GRTEMPLATE = SDRTEMPLATE;
          pGRRD->GRREFERENCE = SBSYMS[IDI];
          pGRRD->GRREFERENCEDX = RDXI;
          pGRRD->GRREFERENCEDY = RDYI;
          pGRRD->TPGRON = 0;
          pGRRD->GRAT[0] = SDRAT[0];
          pGRRD->GRAT[1] = SDRAT[1];
          pGRRD->GRAT[2] = SDRAT[2];
          pGRRD->GRAT[3] = SDRAT[3];
          std::unique_ptr<CJBig2_ArithDecoder> pArithDecoder(
              new CJBig2_ArithDecoder(pStream));
          BS = pGRRD->decode(pArithDecoder.get(), grContext->data());
          if (!BS) {
            FX_Free(SBSYMS);
            goto failed;
          }
          pStream->alignByte();
          pStream->offset(2);
          if ((uint32_t)nVal != (pStream->getOffset() - nTmp)) {
            delete BS;
            FX_Free(SBSYMS);
            goto failed;
          }
          FX_Free(SBSYMS);
        }
        SDNEWSYMS[NSYMSDECODED] = BS;
      }
      if (SDREFAGG == 0) {
        SDNEWSYMWIDTHS[NSYMSDECODED] = SYMWIDTH;
      }
      NSYMSDECODED = NSYMSDECODED + 1;
    }
    if (SDREFAGG == 0) {
      if (pHuffmanDecoder->decodeAValue(SDHUFFBMSIZE, (int32_t*)&BMSIZE) != 0) {
        goto failed;
      }
      pStream->alignByte();
      if (BMSIZE == 0) {
        stride = (TOTWIDTH + 7) >> 3;
        if (pStream->getByteLeft() >= stride * HCHEIGHT) {
          BHC = new CJBig2_Image(TOTWIDTH, HCHEIGHT);
          for (I = 0; I < HCHEIGHT; I++) {
            JBIG2_memcpy(BHC->m_pData + I * BHC->stride(),
                         pStream->getPointer(), stride);
            pStream->offset(stride);
          }
        } else {
          goto failed;
        }
      } else {
        std::unique_ptr<CJBig2_GRDProc> pGRD(new CJBig2_GRDProc());
        pGRD->MMR = 1;
        pGRD->GBW = TOTWIDTH;
        pGRD->GBH = HCHEIGHT;
        pGRD->Start_decode_MMR(&BHC, pStream, nullptr);
        pStream->alignByte();
      }
      nTmp = 0;
      if (!BHC) {
        continue;
      }
      for (I = HCFIRSTSYM; I < NSYMSDECODED; I++) {
        SDNEWSYMS[I] = BHC->subImage(nTmp, 0, SDNEWSYMWIDTHS[I], HCHEIGHT);
        nTmp += SDNEWSYMWIDTHS[I];
      }
      delete BHC;
      BHC = nullptr;
    }
  }
  EXINDEX = 0;
  CUREXFLAG = 0;
  pTable = pdfium::MakeUnique<CJBig2_HuffmanTable>(
      HuffmanTable_B1, HuffmanTable_B1_Size, HuffmanTable_HTOOB_B1);
  EXFLAGS = FX_Alloc(bool, SDNUMINSYMS + SDNUMNEWSYMS);
  num_ex_syms = 0;
  while (EXINDEX < SDNUMINSYMS + SDNUMNEWSYMS) {
    if (pHuffmanDecoder->decodeAValue(pTable.get(), (int*)&EXRUNLENGTH) != 0) {
      FX_Free(EXFLAGS);
      goto failed;
    }
    if (EXINDEX + EXRUNLENGTH > SDNUMINSYMS + SDNUMNEWSYMS) {
      FX_Free(EXFLAGS);
      goto failed;
    }
    if (EXRUNLENGTH != 0) {
      for (I = EXINDEX; I < EXINDEX + EXRUNLENGTH; I++) {
        if (CUREXFLAG)
          num_ex_syms++;

        EXFLAGS[I] = CUREXFLAG;
      }
    }
    EXINDEX = EXINDEX + EXRUNLENGTH;
    CUREXFLAG = !CUREXFLAG;
  }
  if (num_ex_syms > SDNUMEXSYMS) {
    FX_Free(EXFLAGS);
    goto failed;
  }

  I = J = 0;
  for (I = 0; I < SDNUMINSYMS + SDNUMNEWSYMS; I++) {
    if (EXFLAGS[I] && J < SDNUMEXSYMS) {
      if (I < SDNUMINSYMS) {
        pDict->AddImage(SDINSYMS[I]
                            ? pdfium::MakeUnique<CJBig2_Image>(*SDINSYMS[I])
                            : nullptr);
      } else {
        pDict->AddImage(pdfium::WrapUnique(SDNEWSYMS[I - SDNUMINSYMS]));
      }
      ++J;
    } else if (!EXFLAGS[I] && I >= SDNUMINSYMS) {
      delete SDNEWSYMS[I - SDNUMINSYMS];
    }
  }
  FX_Free(EXFLAGS);
  FX_Free(SDNEWSYMS);
  if (SDREFAGG == 0) {
    FX_Free(SDNEWSYMWIDTHS);
  }
  return pDict.release();
failed:
  for (I = 0; I < NSYMSDECODED; I++) {
    delete SDNEWSYMS[I];
  }
  FX_Free(SDNEWSYMS);
  if (SDREFAGG == 0) {
    FX_Free(SDNEWSYMWIDTHS);
  }
  return nullptr;
}
