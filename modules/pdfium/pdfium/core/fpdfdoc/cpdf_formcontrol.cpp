// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_formcontrol.h"

#include <algorithm>

#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fxge/cfx_renderdevice.h"

namespace {

const FX_CHAR* const g_sHighlightingMode[] = {
    // Must match order of HighlightingMode enum.
    "N", "I", "O", "P", "T"};

}  // namespace

CPDF_FormControl::CPDF_FormControl(CPDF_FormField* pField,
                                   CPDF_Dictionary* pWidgetDict)
    : m_pField(pField),
      m_pWidgetDict(pWidgetDict),
      m_pForm(m_pField->m_pForm) {}

CFX_ByteString CPDF_FormControl::GetOnStateName() const {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csOn;
  CPDF_Dictionary* pAP = m_pWidgetDict->GetDictFor("AP");
  if (!pAP)
    return csOn;

  CPDF_Dictionary* pN = pAP->GetDictFor("N");
  if (!pN)
    return csOn;

  for (const auto& it : *pN) {
    if (it.first != "Off")
      return it.first;
  }
  return CFX_ByteString();
}

void CPDF_FormControl::SetOnStateName(const CFX_ByteString& csOn) {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csValue = csOn;
  if (csValue.IsEmpty())
    csValue = "Yes";
  else if (csValue == "Off")
    csValue = "Yes";

  CFX_ByteString csAS = m_pWidgetDict->GetStringFor("AS", "Off");
  if (csAS != "Off")
    m_pWidgetDict->SetNewFor<CPDF_Name>("AS", csValue);

  CPDF_Dictionary* pAP = m_pWidgetDict->GetDictFor("AP");
  if (!pAP)
    return;

  for (const auto& it : *pAP) {
    CPDF_Object* pObj1 = it.second.get();
    if (!pObj1)
      continue;

    CPDF_Object* pObjDirect1 = pObj1->GetDirect();
    CPDF_Dictionary* pSubDict = pObjDirect1->AsDictionary();
    if (!pSubDict)
      continue;

    auto subdict_it = pSubDict->begin();
    while (subdict_it != pSubDict->end()) {
      const CFX_ByteString& csKey2 = subdict_it->first;
      CPDF_Object* pObj2 = subdict_it->second.get();
      ++subdict_it;
      if (!pObj2)
        continue;
      if (csKey2 != "Off") {
        pSubDict->ReplaceKey(csKey2, csValue);
        break;
      }
    }
  }
}

CFX_ByteString CPDF_FormControl::GetCheckedAPState() {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csOn = GetOnStateName();
  if (GetType() == CPDF_FormField::RadioButton ||
      GetType() == CPDF_FormField::CheckBox) {
    if (ToArray(FPDF_GetFieldAttr(m_pField->m_pDict, "Opt"))) {
      int iIndex = m_pField->GetControlIndex(this);
      csOn.Format("%d", iIndex);
    }
  }
  if (csOn.IsEmpty())
    csOn = "Yes";
  return csOn;
}

CFX_WideString CPDF_FormControl::GetExportValue() const {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csOn = GetOnStateName();
  if (GetType() == CPDF_FormField::RadioButton ||
      GetType() == CPDF_FormField::CheckBox) {
    if (CPDF_Array* pArray =
            ToArray(FPDF_GetFieldAttr(m_pField->m_pDict, "Opt"))) {
      int iIndex = m_pField->GetControlIndex(this);
      csOn = pArray->GetStringAt(iIndex);
    }
  }
  if (csOn.IsEmpty())
    csOn = "Yes";
  return PDF_DecodeText(csOn);
}

bool CPDF_FormControl::IsChecked() const {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csOn = GetOnStateName();
  CFX_ByteString csAS = m_pWidgetDict->GetStringFor("AS");
  return csAS == csOn;
}

bool CPDF_FormControl::IsDefaultChecked() const {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CPDF_Object* pDV = FPDF_GetFieldAttr(m_pField->m_pDict, "DV");
  if (!pDV)
    return false;

  CFX_ByteString csDV = pDV->GetString();
  CFX_ByteString csOn = GetOnStateName();
  return (csDV == csOn);
}

void CPDF_FormControl::CheckControl(bool bChecked) {
  ASSERT(GetType() == CPDF_FormField::CheckBox ||
         GetType() == CPDF_FormField::RadioButton);
  CFX_ByteString csOn = GetOnStateName();
  CFX_ByteString csOldAS = m_pWidgetDict->GetStringFor("AS", "Off");
  CFX_ByteString csAS = "Off";
  if (bChecked)
    csAS = csOn;
  if (csOldAS == csAS)
    return;
  m_pWidgetDict->SetNewFor<CPDF_Name>("AS", csAS);
}

void CPDF_FormControl::DrawControl(CFX_RenderDevice* pDevice,
                                   CFX_Matrix* pMatrix,
                                   CPDF_Page* pPage,
                                   CPDF_Annot::AppearanceMode mode,
                                   const CPDF_RenderOptions* pOptions) {
  if (m_pWidgetDict->GetIntegerFor("F") & ANNOTFLAG_HIDDEN)
    return;

  CPDF_Stream* pStream = FPDFDOC_GetAnnotAP(m_pWidgetDict, mode);
  if (!pStream)
    return;

  CFX_FloatRect form_bbox = pStream->GetDict()->GetRectFor("BBox");
  CFX_Matrix form_matrix = pStream->GetDict()->GetMatrixFor("Matrix");
  form_matrix.TransformRect(form_bbox);
  CFX_FloatRect arect = m_pWidgetDict->GetRectFor("Rect");
  CFX_Matrix matrix;
  matrix.MatchRect(arect, form_bbox);
  matrix.Concat(*pMatrix);
  CPDF_Form form(m_pField->m_pForm->m_pDocument,
                 m_pField->m_pForm->m_pFormDict->GetDictFor("DR"), pStream);
  form.ParseContent(nullptr, nullptr, nullptr);
  CPDF_RenderContext context(pPage);
  context.AppendLayer(&form, &matrix);
  context.Render(pDevice, pOptions, nullptr);
}

CPDF_FormControl::HighlightingMode CPDF_FormControl::GetHighlightingMode() {
  if (!m_pWidgetDict)
    return Invert;

  CFX_ByteString csH = m_pWidgetDict->GetStringFor("H", "I");
  for (size_t i = 0; i < FX_ArraySize(g_sHighlightingMode); ++i) {
    if (csH == g_sHighlightingMode[i])
      return static_cast<HighlightingMode>(i);
  }
  return Invert;
}

CPDF_ApSettings CPDF_FormControl::GetMK() const {
  return CPDF_ApSettings(m_pWidgetDict ? m_pWidgetDict->GetDictFor("MK")
                                       : nullptr);
}

bool CPDF_FormControl::HasMKEntry(const CFX_ByteString& csEntry) const {
  return GetMK().HasMKEntry(csEntry);
}

int CPDF_FormControl::GetRotation() {
  return GetMK().GetRotation();
}

FX_ARGB CPDF_FormControl::GetColor(int& iColorType,
                                   const CFX_ByteString& csEntry) {
  return GetMK().GetColor(iColorType, csEntry);
}

FX_FLOAT CPDF_FormControl::GetOriginalColor(int index,
                                            const CFX_ByteString& csEntry) {
  return GetMK().GetOriginalColor(index, csEntry);
}

void CPDF_FormControl::GetOriginalColor(int& iColorType,
                                        FX_FLOAT fc[4],
                                        const CFX_ByteString& csEntry) {
  GetMK().GetOriginalColor(iColorType, fc, csEntry);
}

CFX_WideString CPDF_FormControl::GetCaption(const CFX_ByteString& csEntry) {
  return GetMK().GetCaption(csEntry);
}

CPDF_Stream* CPDF_FormControl::GetIcon(const CFX_ByteString& csEntry) {
  return GetMK().GetIcon(csEntry);
}

CPDF_IconFit CPDF_FormControl::GetIconFit() {
  return GetMK().GetIconFit();
}

int CPDF_FormControl::GetTextPosition() {
  return GetMK().GetTextPosition();
}

CPDF_Action CPDF_FormControl::GetAction() {
  if (!m_pWidgetDict)
    return CPDF_Action();

  if (m_pWidgetDict->KeyExist("A"))
    return CPDF_Action(m_pWidgetDict->GetDictFor("A"));

  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pField->m_pDict, "A");
  if (!pObj)
    return CPDF_Action();

  return CPDF_Action(pObj->GetDict());
}

CPDF_AAction CPDF_FormControl::GetAdditionalAction() {
  if (!m_pWidgetDict)
    return CPDF_AAction();

  if (m_pWidgetDict->KeyExist("AA"))
    return CPDF_AAction(m_pWidgetDict->GetDictFor("AA"));
  return m_pField->GetAdditionalAction();
}

CPDF_DefaultAppearance CPDF_FormControl::GetDefaultAppearance() {
  if (!m_pWidgetDict)
    return CPDF_DefaultAppearance();

  if (m_pWidgetDict->KeyExist("DA"))
    return CPDF_DefaultAppearance(m_pWidgetDict->GetStringFor("DA"));

  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pField->m_pDict, "DA");
  if (pObj)
    return CPDF_DefaultAppearance(pObj->GetString());
  return m_pField->m_pForm->GetDefaultAppearance();
}

CPDF_Font* CPDF_FormControl::GetDefaultControlFont() {
  CPDF_DefaultAppearance cDA = GetDefaultAppearance();
  CFX_ByteString csFontNameTag;
  FX_FLOAT fFontSize;
  cDA.GetFont(csFontNameTag, fFontSize);
  if (csFontNameTag.IsEmpty())
    return nullptr;

  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pWidgetDict, "DR");
  if (CPDF_Dictionary* pDict = ToDictionary(pObj)) {
    CPDF_Dictionary* pFonts = pDict->GetDictFor("Font");
    if (pFonts) {
      CPDF_Dictionary* pElement = pFonts->GetDictFor(csFontNameTag);
      if (pElement) {
        CPDF_Font* pFont = m_pField->m_pForm->m_pDocument->LoadFont(pElement);
        if (pFont)
          return pFont;
      }
    }
  }
  if (CPDF_Font* pFormFont = m_pField->m_pForm->GetFormFont(csFontNameTag))
    return pFormFont;

  CPDF_Dictionary* pPageDict = m_pWidgetDict->GetDictFor("P");
  pObj = FPDF_GetFieldAttr(pPageDict, "Resources");
  if (CPDF_Dictionary* pDict = ToDictionary(pObj)) {
    CPDF_Dictionary* pFonts = pDict->GetDictFor("Font");
    if (pFonts) {
      CPDF_Dictionary* pElement = pFonts->GetDictFor(csFontNameTag);
      if (pElement) {
        CPDF_Font* pFont = m_pField->m_pForm->m_pDocument->LoadFont(pElement);
        if (pFont)
          return pFont;
      }
    }
  }
  return nullptr;
}

int CPDF_FormControl::GetControlAlignment() {
  if (!m_pWidgetDict)
    return 0;
  if (m_pWidgetDict->KeyExist("Q"))
    return m_pWidgetDict->GetIntegerFor("Q", 0);

  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pField->m_pDict, "Q");
  if (pObj)
    return pObj->GetInteger();
  return m_pField->m_pForm->GetFormAlignment();
}
