// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_aaction.h"

namespace {

const FX_CHAR* g_sAATypes[] = {"E",  "X",  "D",  "U",  "Fo", "Bl", "PO", "PC",
                               "PV", "PI", "O",  "C",  "K",  "F",  "V",  "C",
                               "WC", "WS", "DS", "WP", "DP", ""};

}  // namespace

bool CPDF_AAction::ActionExist(AActionType eType) const {
  return m_pDict && m_pDict->KeyExist(g_sAATypes[eType]);
}

CPDF_Action CPDF_AAction::GetAction(AActionType eType) const {
  return m_pDict ? CPDF_Action(m_pDict->GetDictFor(g_sAATypes[eType]))
                 : CPDF_Action();
}
