// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_linklist.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"

CPDF_LinkList::CPDF_LinkList() {}

CPDF_LinkList::~CPDF_LinkList() {}

const std::vector<CPDF_Dictionary*>* CPDF_LinkList::GetPageLinks(
    CPDF_Page* pPage) {
  uint32_t objnum = pPage->m_pFormDict->GetObjNum();
  if (objnum == 0)
    return nullptr;

  auto it = m_PageMap.find(objnum);
  if (it != m_PageMap.end())
    return &it->second;

  // std::map::operator[] forces the creation of a map entry.
  std::vector<CPDF_Dictionary*>& page_link_list = m_PageMap[objnum];
  LoadPageLinks(pPage, &page_link_list);
  return &page_link_list;
}

CPDF_Link CPDF_LinkList::GetLinkAtPoint(CPDF_Page* pPage,
                                        const CFX_PointF& point,
                                        int* z_order) {
  const std::vector<CPDF_Dictionary*>* pPageLinkList = GetPageLinks(pPage);
  if (!pPageLinkList)
    return CPDF_Link();

  for (size_t i = pPageLinkList->size(); i > 0; --i) {
    size_t annot_index = i - 1;
    CPDF_Dictionary* pAnnot = (*pPageLinkList)[annot_index];
    if (!pAnnot)
      continue;

    CPDF_Link link(pAnnot);
    if (!link.GetRect().Contains(point))
      continue;

    if (z_order)
      *z_order = annot_index;
    return link;
  }
  return CPDF_Link();
}

void CPDF_LinkList::LoadPageLinks(CPDF_Page* pPage,
                                  std::vector<CPDF_Dictionary*>* pList) {
  CPDF_Array* pAnnotList = pPage->m_pFormDict->GetArrayFor("Annots");
  if (!pAnnotList)
    return;

  for (size_t i = 0; i < pAnnotList->GetCount(); ++i) {
    CPDF_Dictionary* pAnnot = pAnnotList->GetDictAt(i);
    bool add_link = (pAnnot && pAnnot->GetStringFor("Subtype") == "Link");
    // Add non-links as nullptrs to preserve z-order.
    pList->push_back(add_link ? pAnnot : nullptr);
  }
}
