// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_BOOKMARK_H_
#define CORE_FPDFDOC_CPDF_BOOKMARK_H_

#include "core/fpdfdoc/cpdf_action.h"
#include "core/fpdfdoc/cpdf_dest.h"
#include "core/fxcrt/fx_string.h"

class CPDF_Dictionary;
class CPDF_Document;

class CPDF_Bookmark {
 public:
  CPDF_Bookmark() : m_pDict(nullptr) {}
  explicit CPDF_Bookmark(CPDF_Dictionary* pDict) : m_pDict(pDict) {}

  CPDF_Dictionary* GetDict() const { return m_pDict; }
  uint32_t GetColorRef() const;
  uint32_t GetFontStyle() const;
  CFX_WideString GetTitle() const;
  CPDF_Dest GetDest(CPDF_Document* pDocument) const;
  CPDF_Action GetAction() const;

 private:
  CPDF_Dictionary* m_pDict;
};

#endif  // CORE_FPDFDOC_CPDF_BOOKMARK_H_
