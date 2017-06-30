// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_action.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_filespec.h"
#include "core/fpdfdoc/cpdf_nametree.h"

namespace {

const FX_CHAR* const g_sATypes[] = {
    "Unknown",     "GoTo",       "GoToR",     "GoToE",      "Launch",
    "Thread",      "URI",        "Sound",     "Movie",      "Hide",
    "Named",       "SubmitForm", "ResetForm", "ImportData", "JavaScript",
    "SetOCGState", "Rendition",  "Trans",     "GoTo3DView", nullptr};

}  // namespace

CPDF_Dest CPDF_Action::GetDest(CPDF_Document* pDoc) const {
  if (!m_pDict)
    return CPDF_Dest();

  CFX_ByteString type = m_pDict->GetStringFor("S");
  if (type != "GoTo" && type != "GoToR")
    return CPDF_Dest();

  CPDF_Object* pDest = m_pDict->GetDirectObjectFor("D");
  if (!pDest)
    return CPDF_Dest();
  if (pDest->IsString() || pDest->IsName()) {
    CPDF_NameTree name_tree(pDoc, "Dests");
    return CPDF_Dest(name_tree.LookupNamedDest(pDoc, pDest->GetString()));
  }
  if (CPDF_Array* pArray = pDest->AsArray())
    return CPDF_Dest(pArray);

  return CPDF_Dest();
}

CPDF_Action::ActionType CPDF_Action::GetType() const {
  if (!m_pDict)
    return Unknown;

  CFX_ByteString csType = m_pDict->GetStringFor("S");
  if (csType.IsEmpty())
    return Unknown;

  for (int i = 0; g_sATypes[i]; ++i) {
    if (csType == g_sATypes[i])
      return static_cast<ActionType>(i);
  }
  return Unknown;
}

CFX_WideString CPDF_Action::GetFilePath() const {
  CFX_ByteString type = m_pDict->GetStringFor("S");
  if (type != "GoToR" && type != "Launch" && type != "SubmitForm" &&
      type != "ImportData") {
    return CFX_WideString();
  }

  CPDF_Object* pFile = m_pDict->GetDirectObjectFor("F");
  CFX_WideString path;
  if (!pFile) {
    if (type == "Launch") {
      CPDF_Dictionary* pWinDict = m_pDict->GetDictFor("Win");
      if (pWinDict) {
        return CFX_WideString::FromLocal(
            pWinDict->GetStringFor("F").AsStringC());
      }
    }
    return path;
  }

  CPDF_FileSpec filespec(pFile);
  filespec.GetFileName(&path);
  return path;
}

CFX_ByteString CPDF_Action::GetURI(CPDF_Document* pDoc) const {
  CFX_ByteString csURI;
  if (!m_pDict)
    return csURI;
  if (m_pDict->GetStringFor("S") != "URI")
    return csURI;

  csURI = m_pDict->GetStringFor("URI");
  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  CPDF_Dictionary* pURI = pRoot->GetDictFor("URI");
  if (pURI) {
    if (csURI.Find(":", 0) < 1)
      csURI = pURI->GetStringFor("Base") + csURI;
  }
  return csURI;
}

CFX_WideString CPDF_Action::GetJavaScript() const {
  CFX_WideString csJS;
  if (!m_pDict)
    return csJS;

  CPDF_Object* pJS = m_pDict->GetDirectObjectFor("JS");
  return pJS ? pJS->GetUnicodeText() : csJS;
}

size_t CPDF_Action::GetSubActionsCount() const {
  if (!m_pDict || !m_pDict->KeyExist("Next"))
    return 0;

  CPDF_Object* pNext = m_pDict->GetDirectObjectFor("Next");
  if (!pNext)
    return 0;
  if (pNext->IsDictionary())
    return 1;
  if (CPDF_Array* pArray = pNext->AsArray())
    return pArray->GetCount();
  return 0;
}

CPDF_Action CPDF_Action::GetSubAction(size_t iIndex) const {
  if (!m_pDict || !m_pDict->KeyExist("Next"))
    return CPDF_Action();

  CPDF_Object* pNext = m_pDict->GetDirectObjectFor("Next");
  if (CPDF_Dictionary* pDict = ToDictionary(pNext)) {
    if (iIndex == 0)
      return CPDF_Action(pDict);
  } else if (CPDF_Array* pArray = ToArray(pNext)) {
    return CPDF_Action(pArray->GetDictAt(iIndex));
  }
  return CPDF_Action();
}
