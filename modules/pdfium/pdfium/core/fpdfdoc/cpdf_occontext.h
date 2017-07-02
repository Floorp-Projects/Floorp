// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_OCCONTEXT_H_
#define CORE_FPDFDOC_CPDF_OCCONTEXT_H_

#include <unordered_map>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_string.h"

class CPDF_Array;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_PageObject;

class CPDF_OCContext : public CFX_Retainable {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  enum UsageType { View = 0, Design, Print, Export };

  bool CheckOCGVisible(const CPDF_Dictionary* pOCGDict);
  bool CheckObjectVisible(const CPDF_PageObject* pObj);

 private:
  CPDF_OCContext(CPDF_Document* pDoc, UsageType eUsageType);
  ~CPDF_OCContext() override;

  bool LoadOCGStateFromConfig(const CFX_ByteString& csConfig,
                              const CPDF_Dictionary* pOCGDict) const;
  bool LoadOCGState(const CPDF_Dictionary* pOCGDict) const;
  bool GetOCGVisible(const CPDF_Dictionary* pOCGDict);
  bool GetOCGVE(CPDF_Array* pExpression, int nLevel);
  bool LoadOCMDState(const CPDF_Dictionary* pOCMDDict);

  CPDF_Document* const m_pDocument;
  const UsageType m_eUsageType;
  std::unordered_map<const CPDF_Dictionary*, bool> m_OCGStates;
};

#endif  // CORE_FPDFDOC_CPDF_OCCONTEXT_H_
