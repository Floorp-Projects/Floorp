// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_GRDPROC_H_
#define CORE_FXCODEC_JBIG2_JBIG2_GRDPROC_H_

#include "core/fxcodec/fx_codec_def.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CJBig2_ArithDecoder;
class CJBig2_BitStream;
class CJBig2_Image;
class IFX_Pause;
struct JBig2ArithCtx;

class CJBig2_GRDProc {
 public:
  CJBig2_GRDProc();

  CJBig2_Image* decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                             JBig2ArithCtx* gbContext);

  FXCODEC_STATUS Start_decode_Arith(CJBig2_Image** pImage,
                                    CJBig2_ArithDecoder* pArithDecoder,
                                    JBig2ArithCtx* gbContext,
                                    IFX_Pause* pPause);
  FXCODEC_STATUS Start_decode_MMR(CJBig2_Image** pImage,
                                  CJBig2_BitStream* pStream,
                                  IFX_Pause* pPause);
  FXCODEC_STATUS Continue_decode(IFX_Pause* pPause);
  FX_RECT GetReplaceRect() const { return m_ReplaceRect; }

  bool MMR;
  uint32_t GBW;
  uint32_t GBH;
  uint8_t GBTEMPLATE;
  bool TPGDON;
  bool USESKIP;
  CJBig2_Image* SKIP;
  int8_t GBAT[8];

 private:
  bool UseTemplate0Opt3() const;
  bool UseTemplate1Opt3() const;
  bool UseTemplate23Opt3() const;

  FXCODEC_STATUS decode_Arith(IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template0_opt3(CJBig2_Image* pImage,
                                             CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext,
                                             IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template0_unopt(
      CJBig2_Image* pImage,
      CJBig2_ArithDecoder* pArithDecoder,
      JBig2ArithCtx* gbContext,
      IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template1_opt3(CJBig2_Image* pImage,
                                             CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext,
                                             IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template1_unopt(
      CJBig2_Image* pImage,
      CJBig2_ArithDecoder* pArithDecoder,
      JBig2ArithCtx* gbContext,
      IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template2_opt3(CJBig2_Image* pImage,
                                             CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext,
                                             IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template2_unopt(
      CJBig2_Image* pImage,
      CJBig2_ArithDecoder* pArithDecoder,
      JBig2ArithCtx* gbContext,
      IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template3_opt3(CJBig2_Image* pImage,
                                             CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext,
                                             IFX_Pause* pPause);
  FXCODEC_STATUS decode_Arith_Template3_unopt(
      CJBig2_Image* pImage,
      CJBig2_ArithDecoder* pArithDecoder,
      JBig2ArithCtx* gbContext,
      IFX_Pause* pPause);
  CJBig2_Image* decode_Arith_Template0_opt3(CJBig2_ArithDecoder* pArithDecoder,
                                            JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template0_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template1_opt3(CJBig2_ArithDecoder* pArithDecoder,
                                            JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template1_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template2_opt3(CJBig2_ArithDecoder* pArithDecoder,
                                            JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template2_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template3_opt3(CJBig2_ArithDecoder* pArithDecoder,
                                            JBig2ArithCtx* gbContext);

  CJBig2_Image* decode_Arith_Template3_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                             JBig2ArithCtx* gbContext);

  uint32_t m_loopIndex;
  uint8_t* m_pLine;
  IFX_Pause* m_pPause;
  FXCODEC_STATUS m_ProssiveStatus;
  CJBig2_Image** m_pImage;
  CJBig2_ArithDecoder* m_pArithDecoder;
  JBig2ArithCtx* m_gbContext;
  uint16_t m_DecodeType;
  int m_LTP;
  FX_RECT m_ReplaceRect;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_GRDPROC_H_
