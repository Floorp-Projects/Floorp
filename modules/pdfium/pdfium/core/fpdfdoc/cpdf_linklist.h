// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_LINKLIST_H_
#define CORE_FPDFDOC_CPDF_LINKLIST_H_

#include <map>
#include <vector>

#include "core/fpdfdoc/cpdf_link.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Page;
class CPDF_Dictionary;

class CPDF_LinkList {
 public:
  CPDF_LinkList();
  ~CPDF_LinkList();

  CPDF_Link GetLinkAtPoint(CPDF_Page* pPage,
                           const CFX_PointF& point,
                           int* z_order);

 private:
  const std::vector<CPDF_Dictionary*>* GetPageLinks(CPDF_Page* pPage);
  void LoadPageLinks(CPDF_Page* pPage, std::vector<CPDF_Dictionary*>* pList);

  std::map<uint32_t, std::vector<CPDF_Dictionary*>> m_PageMap;
};

#endif  // CORE_FPDFDOC_CPDF_LINKLIST_H_
