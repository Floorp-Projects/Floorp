// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_SymbolDict.h"

#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_memory.h"
#include "third_party/base/ptr_util.h"

CJBig2_SymbolDict::CJBig2_SymbolDict() {}

CJBig2_SymbolDict::~CJBig2_SymbolDict() {}

std::unique_ptr<CJBig2_SymbolDict> CJBig2_SymbolDict::DeepCopy() const {
  auto dst = pdfium::MakeUnique<CJBig2_SymbolDict>();
  for (const auto& image : m_SDEXSYMS) {
    dst->m_SDEXSYMS.push_back(image ? pdfium::MakeUnique<CJBig2_Image>(*image)
                                    : nullptr);
  }
  dst->m_gbContext = m_gbContext;
  dst->m_grContext = m_grContext;
  return dst;
}
