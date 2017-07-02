// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_numbertree.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"

namespace {

CPDF_Object* SearchNumberNode(const CPDF_Dictionary* pNode, int num) {
  CPDF_Array* pLimits = pNode->GetArrayFor("Limits");
  if (pLimits &&
      (num < pLimits->GetIntegerAt(0) || num > pLimits->GetIntegerAt(1))) {
    return nullptr;
  }
  CPDF_Array* pNumbers = pNode->GetArrayFor("Nums");
  if (pNumbers) {
    for (size_t i = 0; i < pNumbers->GetCount() / 2; i++) {
      int index = pNumbers->GetIntegerAt(i * 2);
      if (num == index)
        return pNumbers->GetDirectObjectAt(i * 2 + 1);
      if (index > num)
        break;
    }
    return nullptr;
  }

  CPDF_Array* pKids = pNode->GetArrayFor("Kids");
  if (!pKids)
    return nullptr;

  for (size_t i = 0; i < pKids->GetCount(); i++) {
    CPDF_Dictionary* pKid = pKids->GetDictAt(i);
    if (!pKid)
      continue;

    CPDF_Object* pFound = SearchNumberNode(pKid, num);
    if (pFound)
      return pFound;
  }
  return nullptr;
}

}  // namespace

CPDF_Object* CPDF_NumberTree::LookupValue(int num) const {
  return SearchNumberNode(m_pRoot, num);
}
