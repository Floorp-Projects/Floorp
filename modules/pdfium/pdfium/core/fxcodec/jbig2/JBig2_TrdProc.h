// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_TRDPROC_H_
#define CORE_FXCODEC_JBIG2_JBIG2_TRDPROC_H_

#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_system.h"

class CJBig2_ArithDecoder;
class CJBig2_ArithIaidDecoder;
class CJBig2_ArithIntDecoder;
class CJBig2_BitStream;
class CJBig2_HuffmanTable;
struct JBig2ArithCtx;
struct JBig2HuffmanCode;

struct JBig2IntDecoderState {
  CJBig2_ArithIntDecoder* IADT;
  CJBig2_ArithIntDecoder* IAFS;
  CJBig2_ArithIntDecoder* IADS;
  CJBig2_ArithIntDecoder* IAIT;
  CJBig2_ArithIntDecoder* IARI;
  CJBig2_ArithIntDecoder* IARDW;
  CJBig2_ArithIntDecoder* IARDH;
  CJBig2_ArithIntDecoder* IARDX;
  CJBig2_ArithIntDecoder* IARDY;
  CJBig2_ArithIaidDecoder* IAID;
};

enum JBig2Corner {
  JBIG2_CORNER_BOTTOMLEFT = 0,
  JBIG2_CORNER_TOPLEFT = 1,
  JBIG2_CORNER_BOTTOMRIGHT = 2,
  JBIG2_CORNER_TOPRIGHT = 3
};

class CJBig2_TRDProc {
 public:
  CJBig2_Image* decode_Huffman(CJBig2_BitStream* pStream,
                               JBig2ArithCtx* grContext);

  CJBig2_Image* decode_Arith(CJBig2_ArithDecoder* pArithDecoder,
                             JBig2ArithCtx* grContext,
                             JBig2IntDecoderState* pIDS);

  bool SBHUFF;
  bool SBREFINE;
  uint32_t SBW;
  uint32_t SBH;
  uint32_t SBNUMINSTANCES;
  uint32_t SBSTRIPS;
  uint32_t SBNUMSYMS;

  JBig2HuffmanCode* SBSYMCODES;
  uint8_t SBSYMCODELEN;

  CJBig2_Image** SBSYMS;
  bool SBDEFPIXEL;

  JBig2ComposeOp SBCOMBOP;
  bool TRANSPOSED;

  JBig2Corner REFCORNER;
  int8_t SBDSOFFSET;
  CJBig2_HuffmanTable* SBHUFFFS;
  CJBig2_HuffmanTable* SBHUFFDS;
  CJBig2_HuffmanTable* SBHUFFDT;
  CJBig2_HuffmanTable* SBHUFFRDW;
  CJBig2_HuffmanTable* SBHUFFRDH;
  CJBig2_HuffmanTable* SBHUFFRDX;
  CJBig2_HuffmanTable* SBHUFFRDY;
  CJBig2_HuffmanTable* SBHUFFRSIZE;
  bool SBRTEMPLATE;
  int8_t SBRAT[4];
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_TRDPROC_H_
