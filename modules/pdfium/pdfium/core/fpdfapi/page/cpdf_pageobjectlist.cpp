// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pageobjectlist.h"

#include "core/fpdfapi/page/pageint.h"
#include "third_party/base/stl_util.h"

CPDF_PageObject* CPDF_PageObjectList::GetPageObjectByIndex(int index) {
  if (index < 0 || index >= pdfium::CollectionSize<int>(*this))
    return nullptr;
  return (*this)[index].get();
}
