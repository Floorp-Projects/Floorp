// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_NUMBERTREE_H_
#define CORE_FPDFDOC_CPDF_NUMBERTREE_H_

class CPDF_Dictionary;
class CPDF_Object;

class CPDF_NumberTree {
 public:
  explicit CPDF_NumberTree(CPDF_Dictionary* pRoot) : m_pRoot(pRoot) {}

  CPDF_Object* LookupValue(int num) const;

 protected:
  CPDF_Dictionary* const m_pRoot;
};

#endif  // CORE_FPDFDOC_CPDF_NUMBERTREE_H_
