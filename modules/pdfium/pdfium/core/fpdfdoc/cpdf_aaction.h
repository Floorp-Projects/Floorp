// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_AACTION_H_
#define CORE_FPDFDOC_CPDF_AACTION_H_

#include "core/fpdfdoc/cpdf_action.h"

class CPDF_Dictionary;

class CPDF_AAction {
 public:
  enum AActionType {
    CursorEnter = 0,
    CursorExit,
    ButtonDown,
    ButtonUp,
    GetFocus,
    LoseFocus,
    PageOpen,
    PageClose,
    PageVisible,
    PageInvisible,
    OpenPage,
    ClosePage,
    KeyStroke,
    Format,
    Validate,
    Calculate,
    CloseDocument,
    SaveDocument,
    DocumentSaved,
    PrintDocument,
    DocumentPrinted
  };

  CPDF_AAction() : m_pDict(nullptr) {}
  explicit CPDF_AAction(CPDF_Dictionary* pDict) : m_pDict(pDict) {}

  bool ActionExist(AActionType eType) const;
  CPDF_Action GetAction(AActionType eType) const;
  CPDF_Dictionary* GetDict() const { return m_pDict; }

 private:
  CPDF_Dictionary* const m_pDict;
};

#endif  // CORE_FPDFDOC_CPDF_AACTION_H_
