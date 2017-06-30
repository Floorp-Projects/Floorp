// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_nametree.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"

namespace {

const int nMaxRecursion = 32;

CPDF_Object* SearchNameNode(CPDF_Dictionary* pNode,
                            const CFX_ByteString& csName,
                            size_t& nIndex,
                            CPDF_Array** ppFind,
                            int nLevel = 0) {
  if (nLevel > nMaxRecursion)
    return nullptr;

  CPDF_Array* pLimits = pNode->GetArrayFor("Limits");
  if (pLimits) {
    CFX_ByteString csLeft = pLimits->GetStringAt(0);
    CFX_ByteString csRight = pLimits->GetStringAt(1);
    if (csLeft.Compare(csRight.AsStringC()) > 0) {
      CFX_ByteString csTmp = csRight;
      csRight = csLeft;
      csLeft = csTmp;
    }
    if (csName.Compare(csLeft.AsStringC()) < 0 ||
        csName.Compare(csRight.AsStringC()) > 0) {
      return nullptr;
    }
  }

  CPDF_Array* pNames = pNode->GetArrayFor("Names");
  if (pNames) {
    size_t dwCount = pNames->GetCount() / 2;
    for (size_t i = 0; i < dwCount; i++) {
      CFX_ByteString csValue = pNames->GetStringAt(i * 2);
      int32_t iCompare = csValue.Compare(csName.AsStringC());
      if (iCompare <= 0) {
        if (ppFind)
          *ppFind = pNames;
        if (iCompare < 0)
          continue;
      } else {
        break;
      }
      nIndex += i;
      return pNames->GetDirectObjectAt(i * 2 + 1);
    }
    nIndex += dwCount;
    return nullptr;
  }

  CPDF_Array* pKids = pNode->GetArrayFor("Kids");
  if (!pKids)
    return nullptr;

  for (size_t i = 0; i < pKids->GetCount(); i++) {
    CPDF_Dictionary* pKid = pKids->GetDictAt(i);
    if (!pKid)
      continue;

    CPDF_Object* pFound =
        SearchNameNode(pKid, csName, nIndex, ppFind, nLevel + 1);
    if (pFound)
      return pFound;
  }
  return nullptr;
}

CPDF_Object* SearchNameNode(CPDF_Dictionary* pNode,
                            size_t nIndex,
                            size_t& nCurIndex,
                            CFX_ByteString& csName,
                            CPDF_Array** ppFind,
                            int nLevel = 0) {
  if (nLevel > nMaxRecursion)
    return nullptr;

  CPDF_Array* pNames = pNode->GetArrayFor("Names");
  if (pNames) {
    size_t nCount = pNames->GetCount() / 2;
    if (nIndex >= nCurIndex + nCount) {
      nCurIndex += nCount;
      return nullptr;
    }
    if (ppFind)
      *ppFind = pNames;
    csName = pNames->GetStringAt((nIndex - nCurIndex) * 2);
    return pNames->GetDirectObjectAt((nIndex - nCurIndex) * 2 + 1);
  }
  CPDF_Array* pKids = pNode->GetArrayFor("Kids");
  if (!pKids)
    return nullptr;
  for (size_t i = 0; i < pKids->GetCount(); i++) {
    CPDF_Dictionary* pKid = pKids->GetDictAt(i);
    if (!pKid)
      continue;
    CPDF_Object* pFound =
        SearchNameNode(pKid, nIndex, nCurIndex, csName, ppFind, nLevel + 1);
    if (pFound)
      return pFound;
  }
  return nullptr;
}

size_t CountNames(CPDF_Dictionary* pNode, int nLevel = 0) {
  if (nLevel > nMaxRecursion)
    return 0;

  CPDF_Array* pNames = pNode->GetArrayFor("Names");
  if (pNames)
    return pNames->GetCount() / 2;

  CPDF_Array* pKids = pNode->GetArrayFor("Kids");
  if (!pKids)
    return 0;

  size_t nCount = 0;
  for (size_t i = 0; i < pKids->GetCount(); i++) {
    CPDF_Dictionary* pKid = pKids->GetDictAt(i);
    if (!pKid)
      continue;

    nCount += CountNames(pKid, nLevel + 1);
  }
  return nCount;
}

}  // namespace

CPDF_NameTree::CPDF_NameTree(CPDF_Document* pDoc,
                             const CFX_ByteString& category)
    : m_pRoot(nullptr) {
  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  if (!pRoot)
    return;

  CPDF_Dictionary* pNames = pRoot->GetDictFor("Names");
  if (!pNames)
    return;

  m_pRoot = pNames->GetDictFor(category);
}

size_t CPDF_NameTree::GetCount() const {
  return m_pRoot ? ::CountNames(m_pRoot) : 0;
}

int CPDF_NameTree::GetIndex(const CFX_ByteString& csName) const {
  if (!m_pRoot)
    return -1;

  size_t nIndex = 0;
  if (!SearchNameNode(m_pRoot, csName, nIndex, nullptr))
    return -1;
  return nIndex;
}

CPDF_Object* CPDF_NameTree::LookupValue(int nIndex,
                                        CFX_ByteString& csName) const {
  if (!m_pRoot)
    return nullptr;
  size_t nCurIndex = 0;
  return SearchNameNode(m_pRoot, nIndex, nCurIndex, csName, nullptr);
}

CPDF_Object* CPDF_NameTree::LookupValue(const CFX_ByteString& csName) const {
  if (!m_pRoot)
    return nullptr;
  size_t nIndex = 0;
  return SearchNameNode(m_pRoot, csName, nIndex, nullptr);
}

CPDF_Array* CPDF_NameTree::LookupNamedDest(CPDF_Document* pDoc,
                                           const CFX_ByteString& sName) {
  CPDF_Object* pValue = LookupValue(sName);
  if (!pValue) {
    CPDF_Dictionary* pDests = pDoc->GetRoot()->GetDictFor("Dests");
    if (!pDests)
      return nullptr;
    pValue = pDests->GetDirectObjectFor(sName);
  }
  if (!pValue)
    return nullptr;
  if (CPDF_Array* pArray = pValue->AsArray())
    return pArray;
  if (CPDF_Dictionary* pDict = pValue->AsDictionary())
    return pDict->GetArrayFor("D");
  return nullptr;
}
