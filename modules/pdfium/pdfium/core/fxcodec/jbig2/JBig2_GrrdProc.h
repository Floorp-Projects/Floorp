// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_GRRDPROC_H_
#define CORE_FXCODEC_JBIG2_JBIG2_GRRDPROC_H_

#include "core/fxcrt/fx_system.h"

class CJBig2_ArithDecoder;
class CJBig2_Image;
struct JBig2ArithCtx;

class CJBig2_GRRDProc {
 public:
  CJBig2_Image* decode(CJBig2_ArithDecoder* pArithDecoder,
                       JBig2ArithCtx* grContext);

  CJBig2_Image* decode_Template0_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                       JBig2ArithCtx* grContext);

  CJBig2_Image* decode_Template0_opt(CJBig2_ArithDecoder* pArithDecoder,
                                     JBig2ArithCtx* grContext);

  CJBig2_Image* decode_Template1_unopt(CJBig2_ArithDecoder* pArithDecoder,
                                       JBig2ArithCtx* grContext);

  CJBig2_Image* decode_Template1_opt(CJBig2_ArithDecoder* pArithDecoder,
                                     JBig2ArithCtx* grContext);

  uint32_t GRW;
  uint32_t GRH;
  bool GRTEMPLATE;
  CJBig2_Image* GRREFERENCE;
  int32_t GRREFERENCEDX;
  int32_t GRREFERENCEDY;
  bool TPGRON;
  int8_t GRAT[4];
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_GRRDPROC_H_
