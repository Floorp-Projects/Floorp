// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_BOOKMARKTREE_H_
#define CORE_FPDFDOC_CPDF_BOOKMARKTREE_H_

#include "core/fpdfdoc/cpdf_bookmark.h"

class CPDF_Document;

class CPDF_BookmarkTree {
 public:
  explicit CPDF_BookmarkTree(CPDF_Document* pDoc) : m_pDocument(pDoc) {}

  CPDF_Bookmark GetFirstChild(const CPDF_Bookmark& parent) const;
  CPDF_Bookmark GetNextSibling(const CPDF_Bookmark& bookmark) const;
  CPDF_Document* GetDocument() const { return m_pDocument; }

 private:
  CPDF_Document* const m_pDocument;
};

#endif  // CORE_FPDFDOC_CPDF_BOOKMARKTREE_H_
