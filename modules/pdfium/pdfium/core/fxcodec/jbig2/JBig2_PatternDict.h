// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_PATTERNDICT_H_
#define CORE_FXCODEC_JBIG2_JBIG2_PATTERNDICT_H_

#include "core/fxcodec/jbig2/JBig2_Define.h"
#include "core/fxcodec/jbig2/JBig2_Image.h"

class CJBig2_PatternDict {
 public:
  CJBig2_PatternDict();

  ~CJBig2_PatternDict();

  uint32_t NUMPATS;
  CJBig2_Image** HDPATS;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_PATTERNDICT_H_
