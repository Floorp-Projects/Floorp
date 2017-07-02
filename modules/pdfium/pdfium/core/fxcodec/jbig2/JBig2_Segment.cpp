// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_Segment.h"

#include "core/fxcrt/fx_memory.h"

CJBig2_Segment::CJBig2_Segment() {
  m_dwNumber = 0;
  m_cFlags.c = 0;
  m_nReferred_to_segment_count = 0;
  m_pReferred_to_segment_numbers = nullptr;
  m_dwPage_association = 0;
  m_dwData_length = 0;
  m_dwHeader_Length = 0;
  m_dwObjNum = 0;
  m_dwDataOffset = 0;
  m_State = JBIG2_SEGMENT_HEADER_UNPARSED;
  m_nResultType = JBIG2_VOID_POINTER;
  m_Result.vd = nullptr;
}
CJBig2_Segment::~CJBig2_Segment() {
  FX_Free(m_pReferred_to_segment_numbers);

  switch (m_nResultType) {
    case JBIG2_IMAGE_POINTER:
      delete m_Result.im;
      break;
    case JBIG2_SYMBOL_DICT_POINTER:
      delete m_Result.sd;
      break;
    case JBIG2_PATTERN_DICT_POINTER:
      delete m_Result.pd;
      break;
    case JBIG2_HUFFMAN_TABLE_POINTER:
      delete m_Result.ht;
      break;
    default:
      FX_Free(m_Result.vd);
  }
}
