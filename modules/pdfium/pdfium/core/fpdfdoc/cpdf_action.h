// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_ACTION_H_
#define CORE_FPDFDOC_CPDF_ACTION_H_

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfdoc/cpdf_dest.h"
#include "core/fxcrt/fx_string.h"

class CPDF_Document;

class CPDF_Action {
 public:
  enum ActionType {
    Unknown = 0,
    GoTo,
    GoToR,
    GoToE,
    Launch,
    Thread,
    URI,
    Sound,
    Movie,
    Hide,
    Named,
    SubmitForm,
    ResetForm,
    ImportData,
    JavaScript,
    SetOCGState,
    Rendition,
    Trans,
    GoTo3DView
  };

  CPDF_Action() : m_pDict(nullptr) {}
  explicit CPDF_Action(CPDF_Dictionary* pDict) : m_pDict(pDict) {}

  CPDF_Dictionary* GetDict() const { return m_pDict; }
  ActionType GetType() const;
  CPDF_Dest GetDest(CPDF_Document* pDoc) const;
  CFX_WideString GetFilePath() const;
  CFX_ByteString GetURI(CPDF_Document* pDoc) const;
  bool GetHideStatus() const { return m_pDict->GetBooleanFor("H", true); }
  CFX_ByteString GetNamedAction() const { return m_pDict->GetStringFor("N"); }
  uint32_t GetFlags() const { return m_pDict->GetIntegerFor("Flags"); }
  CFX_WideString GetJavaScript() const;
  size_t GetSubActionsCount() const;
  CPDF_Action GetSubAction(size_t iIndex) const;

 private:
  CPDF_Dictionary* const m_pDict;
};

#endif  // CORE_FPDFDOC_CPDF_ACTION_H_
