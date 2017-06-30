// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_IPDF_FORMNOTIFY_H_
#define CORE_FPDFDOC_IPDF_FORMNOTIFY_H_

#include "core/fxcrt/fx_string.h"

class CPDF_FormField;
class CPDF_InterForm;

class IPDF_FormNotify {
 public:
  virtual ~IPDF_FormNotify() {}

  virtual int BeforeValueChange(CPDF_FormField* pField,
                                const CFX_WideString& csValue) = 0;
  virtual void AfterValueChange(CPDF_FormField* pField) = 0;
  virtual int BeforeSelectionChange(CPDF_FormField* pField,
                                    const CFX_WideString& csValue) = 0;
  virtual void AfterSelectionChange(CPDF_FormField* pField) = 0;
  virtual void AfterCheckedStatusChange(CPDF_FormField* pField) = 0;
  virtual int BeforeFormReset(CPDF_InterForm* pForm) = 0;
  virtual void AfterFormReset(CPDF_InterForm* pForm) = 0;
  virtual int BeforeFormImportData(CPDF_InterForm* pForm) = 0;
  virtual void AfterFormImportData(CPDF_InterForm* pForm) = 0;
};

#endif  // CORE_FPDFDOC_IPDF_FORMNOTIFY_H_
