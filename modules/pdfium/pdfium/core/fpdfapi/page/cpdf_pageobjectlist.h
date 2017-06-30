// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTLIST_H_
#define CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTLIST_H_

#include <deque>
#include <memory>

class CPDF_PageObject;

class CPDF_PageObjectList
    : public std::deque<std::unique_ptr<CPDF_PageObject>> {
 public:
  CPDF_PageObject* GetPageObjectByIndex(int index);
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTLIST_H_
