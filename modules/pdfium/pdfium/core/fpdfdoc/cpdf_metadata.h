// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_METADATA_H_
#define CORE_FPDFDOC_CPDF_METADATA_H_

#include <memory>

class CPDF_Document;
class CXML_Element;

class CPDF_Metadata {
 public:
  explicit CPDF_Metadata(CPDF_Document* pDoc);
  ~CPDF_Metadata();

  const CXML_Element* GetRoot() const;

 private:
  std::unique_ptr<CXML_Element> m_pXmlElement;
};

#endif  // CORE_FPDFDOC_CPDF_METADATA_H_
