// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <map>
#include <memory>
#include <utility>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfdoc/cpdf_numbertree.h"
#include "core/fpdfdoc/fpdf_tagged.h"
#include "core/fpdfdoc/tagged_int.h"
#include "third_party/base/ptr_util.h"

namespace {

const int nMaxRecursion = 32;

bool IsTagged(const CPDF_Document* pDoc) {
  CPDF_Dictionary* pCatalog = pDoc->GetRoot();
  CPDF_Dictionary* pMarkInfo = pCatalog->GetDictFor("MarkInfo");
  return pMarkInfo && pMarkInfo->GetIntegerFor("Marked");
}

}  // namespace

CPDF_StructKid::CPDF_StructKid()
    : m_Type(Invalid),
      m_pDict(nullptr),
      m_PageObjNum(0),
      m_RefObjNum(0),
      m_ContentId(0) {}

CPDF_StructKid::CPDF_StructKid(const CPDF_StructKid& that) = default;

CPDF_StructKid::~CPDF_StructKid() {}

// static
std::unique_ptr<IPDF_StructTree> IPDF_StructTree::LoadPage(
    const CPDF_Document* pDoc,
    const CPDF_Dictionary* pPageDict) {
  if (!IsTagged(pDoc))
    return nullptr;

  auto pTree = pdfium::MakeUnique<CPDF_StructTree>(pDoc);
  pTree->LoadPageTree(pPageDict);
  return std::move(pTree);
}

CPDF_StructTree::CPDF_StructTree(const CPDF_Document* pDoc)
    : m_pTreeRoot(pDoc->GetRoot()->GetDictFor("StructTreeRoot")),
      m_pRoleMap(m_pTreeRoot ? m_pTreeRoot->GetDictFor("RoleMap") : nullptr),
      m_pPage(nullptr) {}

CPDF_StructTree::~CPDF_StructTree() {}

int CPDF_StructTree::CountTopElements() const {
  return pdfium::CollectionSize<int>(m_Kids);
}

IPDF_StructElement* CPDF_StructTree::GetTopElement(int i) const {
  return m_Kids[i].Get();
}

void CPDF_StructTree::LoadPageTree(const CPDF_Dictionary* pPageDict) {
  m_pPage = pPageDict;
  if (!m_pTreeRoot)
    return;

  CPDF_Object* pKids = m_pTreeRoot->GetDirectObjectFor("K");
  if (!pKids)
    return;

  uint32_t dwKids = 0;
  if (pKids->IsDictionary())
    dwKids = 1;
  else if (CPDF_Array* pArray = pKids->AsArray())
    dwKids = pArray->GetCount();
  else
    return;

  m_Kids.clear();
  m_Kids.resize(dwKids);
  CPDF_Dictionary* pParentTree = m_pTreeRoot->GetDictFor("ParentTree");
  if (!pParentTree)
    return;

  CPDF_NumberTree parent_tree(pParentTree);
  int parents_id = pPageDict->GetIntegerFor("StructParents", -1);
  if (parents_id < 0)
    return;

  CPDF_Array* pParentArray = ToArray(parent_tree.LookupValue(parents_id));
  if (!pParentArray)
    return;

  std::map<CPDF_Dictionary*, CFX_RetainPtr<CPDF_StructElement>> element_map;
  for (size_t i = 0; i < pParentArray->GetCount(); i++) {
    if (CPDF_Dictionary* pParent = pParentArray->GetDictAt(i))
      AddPageNode(pParent, &element_map);
  }
}

CFX_RetainPtr<CPDF_StructElement> CPDF_StructTree::AddPageNode(
    CPDF_Dictionary* pDict,
    std::map<CPDF_Dictionary*, CFX_RetainPtr<CPDF_StructElement>>* map,
    int nLevel) {
  if (nLevel > nMaxRecursion)
    return nullptr;

  auto it = map->find(pDict);
  if (it != map->end())
    return it->second;

  auto pElement = pdfium::MakeRetain<CPDF_StructElement>(this, nullptr, pDict);
  (*map)[pDict] = pElement;
  CPDF_Dictionary* pParent = pDict->GetDictFor("P");
  if (!pParent || pParent->GetStringFor("Type") == "StructTreeRoot") {
    if (!AddTopLevelNode(pDict, pElement))
      map->erase(pDict);
    return pElement;
  }

  CFX_RetainPtr<CPDF_StructElement> pParentElement =
      AddPageNode(pParent, map, nLevel + 1);
  bool bSave = false;
  for (CPDF_StructKid& kid : *pParentElement->GetKids()) {
    if (kid.m_Type == CPDF_StructKid::Element && kid.m_pDict == pDict) {
      kid.m_pElement = pElement;
      bSave = true;
    }
  }
  if (!bSave)
    map->erase(pDict);
  return pElement;
}

bool CPDF_StructTree::AddTopLevelNode(
    CPDF_Dictionary* pDict,
    const CFX_RetainPtr<CPDF_StructElement>& pElement) {
  CPDF_Object* pObj = m_pTreeRoot->GetDirectObjectFor("K");
  if (!pObj)
    return false;

  if (pObj->IsDictionary()) {
    if (pObj->GetObjNum() != pDict->GetObjNum())
      return false;
    m_Kids[0] = pElement;
  }
  if (CPDF_Array* pTopKids = pObj->AsArray()) {
    bool bSave = false;
    for (size_t i = 0; i < pTopKids->GetCount(); i++) {
      CPDF_Reference* pKidRef = ToReference(pTopKids->GetObjectAt(i));
      if (pKidRef && pKidRef->GetRefObjNum() == pDict->GetObjNum()) {
        m_Kids[i] = pElement;
        bSave = true;
      }
    }
    if (!bSave)
      return false;
  }
  return true;
}

CPDF_StructElement::CPDF_StructElement(CPDF_StructTree* pTree,
                                       CPDF_StructElement* pParent,
                                       CPDF_Dictionary* pDict)
    : m_pTree(pTree),
      m_pParent(pParent),
      m_pDict(pDict),
      m_Type(pDict->GetStringFor("S")) {
  if (pTree->m_pRoleMap) {
    CFX_ByteString mapped = pTree->m_pRoleMap->GetStringFor(m_Type);
    if (!mapped.IsEmpty())
      m_Type = mapped;
  }
  LoadKids(pDict);
}

IPDF_StructTree* CPDF_StructElement::GetTree() const {
  return m_pTree;
}

const CFX_ByteString& CPDF_StructElement::GetType() const {
  return m_Type;
}

IPDF_StructElement* CPDF_StructElement::GetParent() const {
  return m_pParent;
}

CPDF_Dictionary* CPDF_StructElement::GetDict() const {
  return m_pDict;
}

int CPDF_StructElement::CountKids() const {
  return pdfium::CollectionSize<int>(m_Kids);
}

IPDF_StructElement* CPDF_StructElement::GetKidIfElement(int index) const {
  if (m_Kids[index].m_Type != CPDF_StructKid::Element)
    return nullptr;

  return m_Kids[index].m_pElement.Get();
}

CPDF_StructElement::~CPDF_StructElement() {}

void CPDF_StructElement::LoadKids(CPDF_Dictionary* pDict) {
  CPDF_Object* pObj = pDict->GetObjectFor("Pg");
  uint32_t PageObjNum = 0;
  if (CPDF_Reference* pRef = ToReference(pObj))
    PageObjNum = pRef->GetRefObjNum();

  CPDF_Object* pKids = pDict->GetDirectObjectFor("K");
  if (!pKids)
    return;

  m_Kids.clear();
  if (CPDF_Array* pArray = pKids->AsArray()) {
    m_Kids.resize(pArray->GetCount());
    for (uint32_t i = 0; i < pArray->GetCount(); i++) {
      CPDF_Object* pKid = pArray->GetDirectObjectAt(i);
      LoadKid(PageObjNum, pKid, &m_Kids[i]);
    }
  } else {
    m_Kids.resize(1);
    LoadKid(PageObjNum, pKids, &m_Kids[0]);
  }
}
void CPDF_StructElement::LoadKid(uint32_t PageObjNum,
                                 CPDF_Object* pKidObj,
                                 CPDF_StructKid* pKid) {
  pKid->m_Type = CPDF_StructKid::Invalid;
  if (!pKidObj)
    return;

  if (pKidObj->IsNumber()) {
    if (m_pTree->m_pPage && m_pTree->m_pPage->GetObjNum() != PageObjNum) {
      return;
    }
    pKid->m_Type = CPDF_StructKid::PageContent;
    pKid->m_ContentId = pKidObj->GetInteger();
    pKid->m_PageObjNum = PageObjNum;
    return;
  }

  CPDF_Dictionary* pKidDict = pKidObj->AsDictionary();
  if (!pKidDict)
    return;

  if (CPDF_Reference* pRef = ToReference(pKidDict->GetObjectFor("Pg")))
    PageObjNum = pRef->GetRefObjNum();

  CFX_ByteString type = pKidDict->GetStringFor("Type");
  if (type == "MCR") {
    if (m_pTree->m_pPage && m_pTree->m_pPage->GetObjNum() != PageObjNum) {
      return;
    }
    pKid->m_Type = CPDF_StructKid::StreamContent;
    CPDF_Reference* pRef = ToReference(pKidDict->GetObjectFor("Stm"));
    pKid->m_RefObjNum = pRef ? pRef->GetRefObjNum() : 0;
    pKid->m_PageObjNum = PageObjNum;
    pKid->m_ContentId = pKidDict->GetIntegerFor("MCID");
  } else if (type == "OBJR") {
    if (m_pTree->m_pPage && m_pTree->m_pPage->GetObjNum() != PageObjNum) {
      return;
    }
    pKid->m_Type = CPDF_StructKid::Object;
    CPDF_Reference* pObj = ToReference(pKidDict->GetObjectFor("Obj"));
    pKid->m_RefObjNum = pObj ? pObj->GetRefObjNum() : 0;
    pKid->m_PageObjNum = PageObjNum;
  } else {
    pKid->m_Type = CPDF_StructKid::Element;
    pKid->m_pDict = pKidDict;
    if (!m_pTree->m_pPage) {
      pKid->m_pElement =
          pdfium::MakeRetain<CPDF_StructElement>(m_pTree, this, pKidDict);
    } else {
      pKid->m_pElement = nullptr;
    }
  }
}
static CPDF_Dictionary* FindAttrDict(CPDF_Object* pAttrs,
                                     const CFX_ByteStringC& owner,
                                     FX_FLOAT nLevel = 0.0F) {
  if (nLevel > nMaxRecursion)
    return nullptr;
  if (!pAttrs)
    return nullptr;

  CPDF_Dictionary* pDict = nullptr;
  if (pAttrs->IsDictionary()) {
    pDict = pAttrs->AsDictionary();
  } else if (CPDF_Stream* pStream = pAttrs->AsStream()) {
    pDict = pStream->GetDict();
  } else if (CPDF_Array* pArray = pAttrs->AsArray()) {
    for (uint32_t i = 0; i < pArray->GetCount(); i++) {
      CPDF_Object* pElement = pArray->GetDirectObjectAt(i);
      pDict = FindAttrDict(pElement, owner, nLevel + 1);
      if (pDict)
        return pDict;
    }
  }
  if (pDict && pDict->GetStringFor("O") == owner)
    return pDict;
  return nullptr;
}
CPDF_Object* CPDF_StructElement::GetAttr(const CFX_ByteStringC& owner,
                                         const CFX_ByteStringC& name,
                                         bool bInheritable,
                                         FX_FLOAT fLevel) {
  if (fLevel > nMaxRecursion) {
    return nullptr;
  }
  if (bInheritable) {
    CPDF_Object* pAttr = GetAttr(owner, name, false);
    if (pAttr) {
      return pAttr;
    }
    if (!m_pParent) {
      return nullptr;
    }
    return m_pParent->GetAttr(owner, name, true, fLevel + 1);
  }
  CPDF_Object* pA = m_pDict->GetDirectObjectFor("A");
  if (pA) {
    CPDF_Dictionary* pAttrDict = FindAttrDict(pA, owner);
    if (pAttrDict) {
      CPDF_Object* pAttr = pAttrDict->GetDirectObjectFor(CFX_ByteString(name));
      if (pAttr) {
        return pAttr;
      }
    }
  }
  CPDF_Object* pC = m_pDict->GetDirectObjectFor("C");
  if (!pC)
    return nullptr;

  CPDF_Dictionary* pClassMap = m_pTree->m_pTreeRoot->GetDictFor("ClassMap");
  if (!pClassMap)
    return nullptr;

  if (CPDF_Array* pArray = pC->AsArray()) {
    for (uint32_t i = 0; i < pArray->GetCount(); i++) {
      CFX_ByteString class_name = pArray->GetStringAt(i);
      CPDF_Dictionary* pClassDict = pClassMap->GetDictFor(class_name);
      if (pClassDict && pClassDict->GetStringFor("O") == owner)
        return pClassDict->GetDirectObjectFor(CFX_ByteString(name));
    }
    return nullptr;
  }
  CFX_ByteString class_name = pC->GetString();
  CPDF_Dictionary* pClassDict = pClassMap->GetDictFor(class_name);
  if (pClassDict && pClassDict->GetStringFor("O") == owner)
    return pClassDict->GetDirectObjectFor(CFX_ByteString(name));
  return nullptr;
}
CPDF_Object* CPDF_StructElement::GetAttr(const CFX_ByteStringC& owner,
                                         const CFX_ByteStringC& name,
                                         bool bInheritable,
                                         int subindex) {
  CPDF_Object* pAttr = GetAttr(owner, name, bInheritable);
  CPDF_Array* pArray = ToArray(pAttr);
  if (!pArray || subindex == -1)
    return pAttr;

  if (subindex >= static_cast<int>(pArray->GetCount()))
    return pAttr;
  return pArray->GetDirectObjectAt(subindex);
}
CFX_ByteString CPDF_StructElement::GetName(const CFX_ByteStringC& owner,
                                           const CFX_ByteStringC& name,
                                           const CFX_ByteStringC& default_value,
                                           bool bInheritable,
                                           int subindex) {
  CPDF_Object* pAttr = GetAttr(owner, name, bInheritable, subindex);
  if (ToName(pAttr))
    return pAttr->GetString();
  return CFX_ByteString(default_value);
}

FX_ARGB CPDF_StructElement::GetColor(const CFX_ByteStringC& owner,
                                     const CFX_ByteStringC& name,
                                     FX_ARGB default_value,
                                     bool bInheritable,
                                     int subindex) {
  CPDF_Array* pArray = ToArray(GetAttr(owner, name, bInheritable, subindex));
  if (!pArray)
    return default_value;
  return 0xff000000 | ((int)(pArray->GetNumberAt(0) * 255) << 16) |
         ((int)(pArray->GetNumberAt(1) * 255) << 8) |
         (int)(pArray->GetNumberAt(2) * 255);
}
FX_FLOAT CPDF_StructElement::GetNumber(const CFX_ByteStringC& owner,
                                       const CFX_ByteStringC& name,
                                       FX_FLOAT default_value,
                                       bool bInheritable,
                                       int subindex) {
  CPDF_Object* pAttr = GetAttr(owner, name, bInheritable, subindex);
  return ToNumber(pAttr) ? pAttr->GetNumber() : default_value;
}
int CPDF_StructElement::GetInteger(const CFX_ByteStringC& owner,
                                   const CFX_ByteStringC& name,
                                   int default_value,
                                   bool bInheritable,
                                   int subindex) {
  CPDF_Object* pAttr = GetAttr(owner, name, bInheritable, subindex);
  return ToNumber(pAttr) ? pAttr->GetInteger() : default_value;
}
