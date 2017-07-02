// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/Field.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/Icon.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/PublicMethods.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/color.h"

namespace {

bool SetWidgetDisplayStatus(CPDFSDK_Widget* pWidget, int value) {
  if (!pWidget)
    return false;

  uint32_t dwFlag = pWidget->GetFlags();
  switch (value) {
    case 0:
      dwFlag &= ~ANNOTFLAG_INVISIBLE;
      dwFlag &= ~ANNOTFLAG_HIDDEN;
      dwFlag &= ~ANNOTFLAG_NOVIEW;
      dwFlag |= ANNOTFLAG_PRINT;
      break;
    case 1:
      dwFlag &= ~ANNOTFLAG_INVISIBLE;
      dwFlag &= ~ANNOTFLAG_NOVIEW;
      dwFlag |= (ANNOTFLAG_HIDDEN | ANNOTFLAG_PRINT);
      break;
    case 2:
      dwFlag &= ~ANNOTFLAG_INVISIBLE;
      dwFlag &= ~ANNOTFLAG_PRINT;
      dwFlag &= ~ANNOTFLAG_HIDDEN;
      dwFlag &= ~ANNOTFLAG_NOVIEW;
      break;
    case 3:
      dwFlag |= ANNOTFLAG_NOVIEW;
      dwFlag |= ANNOTFLAG_PRINT;
      dwFlag &= ~ANNOTFLAG_HIDDEN;
      break;
  }

  if (dwFlag != pWidget->GetFlags()) {
    pWidget->SetFlags(dwFlag);
    return true;
  }

  return false;
}

}  // namespace

JSConstSpec CJS_Field::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Field::PropertySpecs[] = {
    {"alignment", get_alignment_static, set_alignment_static},
    {"borderStyle", get_borderStyle_static, set_borderStyle_static},
    {"buttonAlignX", get_buttonAlignX_static, set_buttonAlignX_static},
    {"buttonAlignY", get_buttonAlignY_static, set_buttonAlignY_static},
    {"buttonFitBounds", get_buttonFitBounds_static, set_buttonFitBounds_static},
    {"buttonPosition", get_buttonPosition_static, set_buttonPosition_static},
    {"buttonScaleHow", get_buttonScaleHow_static, set_buttonScaleHow_static},
    {"buttonScaleWhen", get_buttonScaleWhen_static, set_buttonScaleWhen_static},
    {"calcOrderIndex", get_calcOrderIndex_static, set_calcOrderIndex_static},
    {"charLimit", get_charLimit_static, set_charLimit_static},
    {"comb", get_comb_static, set_comb_static},
    {"commitOnSelChange", get_commitOnSelChange_static,
     set_commitOnSelChange_static},
    {"currentValueIndices", get_currentValueIndices_static,
     set_currentValueIndices_static},
    {"defaultStyle", get_defaultStyle_static, set_defaultStyle_static},
    {"defaultValue", get_defaultValue_static, set_defaultValue_static},
    {"doNotScroll", get_doNotScroll_static, set_doNotScroll_static},
    {"doNotSpellCheck", get_doNotSpellCheck_static, set_doNotSpellCheck_static},
    {"delay", get_delay_static, set_delay_static},
    {"display", get_display_static, set_display_static},
    {"doc", get_doc_static, set_doc_static},
    {"editable", get_editable_static, set_editable_static},
    {"exportValues", get_exportValues_static, set_exportValues_static},
    {"hidden", get_hidden_static, set_hidden_static},
    {"fileSelect", get_fileSelect_static, set_fileSelect_static},
    {"fillColor", get_fillColor_static, set_fillColor_static},
    {"lineWidth", get_lineWidth_static, set_lineWidth_static},
    {"highlight", get_highlight_static, set_highlight_static},
    {"multiline", get_multiline_static, set_multiline_static},
    {"multipleSelection", get_multipleSelection_static,
     set_multipleSelection_static},
    {"name", get_name_static, set_name_static},
    {"numItems", get_numItems_static, set_numItems_static},
    {"page", get_page_static, set_page_static},
    {"password", get_password_static, set_password_static},
    {"print", get_print_static, set_print_static},
    {"radiosInUnison", get_radiosInUnison_static, set_radiosInUnison_static},
    {"readonly", get_readonly_static, set_readonly_static},
    {"rect", get_rect_static, set_rect_static},
    {"required", get_required_static, set_required_static},
    {"richText", get_richText_static, set_richText_static},
    {"richValue", get_richValue_static, set_richValue_static},
    {"rotation", get_rotation_static, set_rotation_static},
    {"strokeColor", get_strokeColor_static, set_strokeColor_static},
    {"style", get_style_static, set_style_static},
    {"submitName", get_submitName_static, set_submitName_static},
    {"textColor", get_textColor_static, set_textColor_static},
    {"textFont", get_textFont_static, set_textFont_static},
    {"textSize", get_textSize_static, set_textSize_static},
    {"type", get_type_static, set_type_static},
    {"userName", get_userName_static, set_userName_static},
    {"value", get_value_static, set_value_static},
    {"valueAsString", get_valueAsString_static, set_valueAsString_static},
    {"source", get_source_static, set_source_static},
    {0, 0, 0}};

JSMethodSpec CJS_Field::MethodSpecs[] = {
    {"browseForFileToSubmit", browseForFileToSubmit_static},
    {"buttonGetCaption", buttonGetCaption_static},
    {"buttonGetIcon", buttonGetIcon_static},
    {"buttonImportIcon", buttonImportIcon_static},
    {"buttonSetCaption", buttonSetCaption_static},
    {"buttonSetIcon", buttonSetIcon_static},
    {"checkThisBox", checkThisBox_static},
    {"clearItems", clearItems_static},
    {"defaultIsChecked", defaultIsChecked_static},
    {"deleteItemAt", deleteItemAt_static},
    {"getArray", getArray_static},
    {"getItemAt", getItemAt_static},
    {"getLock", getLock_static},
    {"insertItemAt", insertItemAt_static},
    {"isBoxChecked", isBoxChecked_static},
    {"isDefaultChecked", isDefaultChecked_static},
    {"setAction", setAction_static},
    {"setFocus", setFocus_static},
    {"setItems", setItems_static},
    {"setLock", setLock_static},
    {"signatureGetModifications", signatureGetModifications_static},
    {"signatureGetSeedValue", signatureGetSeedValue_static},
    {"signatureInfo", signatureInfo_static},
    {"signatureSetSeedValue", signatureSetSeedValue_static},
    {"signatureSign", signatureSign_static},
    {"signatureValidate", signatureValidate_static},
    {0, 0}};

IMPLEMENT_JS_CLASS(CJS_Field, Field)

CJS_DelayData::CJS_DelayData(FIELD_PROP prop,
                             int idx,
                             const CFX_WideString& name)
    : eProp(prop), nControlIndex(idx), sFieldName(name) {}

CJS_DelayData::~CJS_DelayData() {}

void CJS_Field::InitInstance(IJS_Runtime* pIRuntime) {
}

Field::Field(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject),
      m_pJSDoc(nullptr),
      m_pFormFillEnv(nullptr),
      m_nFormControlIndex(-1),
      m_bCanSet(false),
      m_bDelay(false) {}

Field::~Field() {}

// note: iControlNo = -1, means not a widget.
void Field::ParseFieldName(const std::wstring& strFieldNameParsed,
                           std::wstring& strFieldName,
                           int& iControlNo) {
  int iStart = strFieldNameParsed.find_last_of(L'.');
  if (iStart == -1) {
    strFieldName = strFieldNameParsed;
    iControlNo = -1;
    return;
  }
  std::wstring suffixal = strFieldNameParsed.substr(iStart + 1);
  iControlNo = FXSYS_wtoi(suffixal.c_str());
  if (iControlNo == 0) {
    int iSpaceStart;
    while ((iSpaceStart = suffixal.find_last_of(L" ")) != -1) {
      suffixal.erase(iSpaceStart, 1);
    }

    if (suffixal.compare(L"0") != 0) {
      strFieldName = strFieldNameParsed;
      iControlNo = -1;
      return;
    }
  }
  strFieldName = strFieldNameParsed.substr(0, iStart);
}

bool Field::AttachField(Document* pDocument,
                        const CFX_WideString& csFieldName) {
  m_pJSDoc = pDocument;
  m_pFormFillEnv.Reset(pDocument->GetFormFillEnv());
  m_bCanSet = m_pFormFillEnv->GetPermissions(FPDFPERM_FILL_FORM) ||
              m_pFormFillEnv->GetPermissions(FPDFPERM_ANNOT_FORM) ||
              m_pFormFillEnv->GetPermissions(FPDFPERM_MODIFY);

  CPDFSDK_InterForm* pRDInterForm = m_pFormFillEnv->GetInterForm();
  CPDF_InterForm* pInterForm = pRDInterForm->GetInterForm();
  CFX_WideString swFieldNameTemp = csFieldName;
  swFieldNameTemp.Replace(L"..", L".");

  if (pInterForm->CountFields(swFieldNameTemp) <= 0) {
    std::wstring strFieldName;
    int iControlNo = -1;
    ParseFieldName(swFieldNameTemp.c_str(), strFieldName, iControlNo);
    if (iControlNo == -1)
      return false;

    m_FieldName = strFieldName.c_str();
    m_nFormControlIndex = iControlNo;
    return true;
  }

  m_FieldName = swFieldNameTemp;
  m_nFormControlIndex = -1;

  return true;
}

std::vector<CPDF_FormField*> Field::GetFormFields(
    CPDFSDK_FormFillEnvironment* pFormFillEnv,
    const CFX_WideString& csFieldName) {
  std::vector<CPDF_FormField*> fields;
  CPDFSDK_InterForm* pReaderInterForm = pFormFillEnv->GetInterForm();
  CPDF_InterForm* pInterForm = pReaderInterForm->GetInterForm();
  for (int i = 0, sz = pInterForm->CountFields(csFieldName); i < sz; ++i) {
    if (CPDF_FormField* pFormField = pInterForm->GetField(i, csFieldName))
      fields.push_back(pFormField);
  }
  return fields;
}

std::vector<CPDF_FormField*> Field::GetFormFields(
    const CFX_WideString& csFieldName) const {
  return Field::GetFormFields(m_pFormFillEnv.Get(), csFieldName);
}

void Field::UpdateFormField(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            CPDF_FormField* pFormField,
                            bool bChangeMark,
                            bool bResetAP,
                            bool bRefresh) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();

  if (bResetAP) {
    std::vector<CPDFSDK_Annot::ObservedPtr> widgets;
    pInterForm->GetWidgets(pFormField, &widgets);

    int nFieldType = pFormField->GetFieldType();
    if (nFieldType == FIELDTYPE_COMBOBOX || nFieldType == FIELDTYPE_TEXTFIELD) {
      for (auto& pObserved : widgets) {
        if (pObserved) {
          bool bFormatted = false;
          CFX_WideString sValue = static_cast<CPDFSDK_Widget*>(pObserved.Get())
                                      ->OnFormat(bFormatted);
          if (pObserved) {  // Not redundant, may be clobbered by OnFormat.
            static_cast<CPDFSDK_Widget*>(pObserved.Get())
                ->ResetAppearance(bFormatted ? &sValue : nullptr, false);
          }
        }
      }
    } else {
      for (auto& pObserved : widgets) {
        if (pObserved) {
          static_cast<CPDFSDK_Widget*>(pObserved.Get())
              ->ResetAppearance(nullptr, false);
        }
      }
    }
  }

  if (bRefresh) {
    // Refresh the widget list. The calls in |bResetAP| may have caused widgets
    // to be removed from the list. We need to call |GetWidgets| again to be
    // sure none of the widgets have been deleted.
    std::vector<CPDFSDK_Annot::ObservedPtr> widgets;
    pInterForm->GetWidgets(pFormField, &widgets);

    // TODO(dsinclair): Determine if all widgets share the same
    // CPDFSDK_InterForm. If that's the case, we can move the code to
    // |GetFormFillEnv| out of the loop.
    for (auto& pObserved : widgets) {
      if (pObserved) {
        CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pObserved.Get());
        pWidget->GetInterForm()->GetFormFillEnv()->UpdateAllViews(nullptr,
                                                                  pWidget);
      }
    }
  }

  if (bChangeMark)
    pFormFillEnv->SetChangeMark();
}

void Field::UpdateFormControl(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                              CPDF_FormControl* pFormControl,
                              bool bChangeMark,
                              bool bResetAP,
                              bool bRefresh) {
  ASSERT(pFormControl);

  CPDFSDK_InterForm* pForm = pFormFillEnv->GetInterForm();
  CPDFSDK_Widget* pWidget = pForm->GetWidget(pFormControl);

  if (pWidget) {
    if (bResetAP) {
      int nFieldType = pWidget->GetFieldType();
      if (nFieldType == FIELDTYPE_COMBOBOX ||
          nFieldType == FIELDTYPE_TEXTFIELD) {
        bool bFormatted = false;
        CFX_WideString sValue = pWidget->OnFormat(bFormatted);
        pWidget->ResetAppearance(bFormatted ? &sValue : nullptr, false);
      } else {
        pWidget->ResetAppearance(nullptr, false);
      }
    }

    if (bRefresh) {
      CPDFSDK_InterForm* pInterForm = pWidget->GetInterForm();
      pInterForm->GetFormFillEnv()->UpdateAllViews(nullptr, pWidget);
    }
  }

  if (bChangeMark)
    pFormFillEnv->SetChangeMark();
}

CPDFSDK_Widget* Field::GetWidget(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 CPDF_FormControl* pFormControl) {
  CPDFSDK_InterForm* pInterForm =
      static_cast<CPDFSDK_InterForm*>(pFormFillEnv->GetInterForm());
  return pInterForm ? pInterForm->GetWidget(pFormControl) : nullptr;
}

bool Field::ValueIsOccur(CPDF_FormField* pFormField,
                         CFX_WideString csOptLabel) {
  for (int i = 0, sz = pFormField->CountOptions(); i < sz; i++) {
    if (csOptLabel.Compare(pFormField->GetOptionLabel(i)) == 0)
      return true;
  }

  return false;
}

CPDF_FormControl* Field::GetSmartFieldControl(CPDF_FormField* pFormField) {
  if (!pFormField->CountControls() ||
      m_nFormControlIndex >= pFormField->CountControls())
    return nullptr;

  if (m_nFormControlIndex < 0)
    return pFormField->GetControl(0);

  return pFormField->GetControl(m_nFormControlIndex);
}

bool Field::alignment(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_ByteString alignStr;
    vp >> alignStr;

    if (m_bDelay) {
      AddDelay_String(FP_ALIGNMENT, alignStr);
    } else {
      Field::SetAlignment(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, alignStr);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    switch (pFormControl->GetControlAlignment()) {
      case 1:
        vp << L"center";
        break;
      case 0:
        vp << L"left";
        break;
      case 2:
        vp << L"right";
        break;
      default:
        vp << L"";
    }
  }

  return true;
}

void Field::SetAlignment(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         const CFX_ByteString& string) {
  // Not supported.
}

bool Field::borderStyle(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_ByteString strType = "";
    vp >> strType;

    if (m_bDelay) {
      AddDelay_String(FP_BORDERSTYLE, strType);
    } else {
      Field::SetBorderStyle(m_pFormFillEnv.Get(), m_FieldName,
                            m_nFormControlIndex, strType);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (!pFormField)
      return false;

    CPDFSDK_Widget* pWidget =
        GetWidget(m_pFormFillEnv.Get(), GetSmartFieldControl(pFormField));
    if (!pWidget)
      return false;

    switch (pWidget->GetBorderStyle()) {
      case BorderStyle::SOLID:
        vp << L"solid";
        break;
      case BorderStyle::DASH:
        vp << L"dashed";
        break;
      case BorderStyle::BEVELED:
        vp << L"beveled";
        break;
      case BorderStyle::INSET:
        vp << L"inset";
        break;
      case BorderStyle::UNDERLINE:
        vp << L"underline";
        break;
      default:
        vp << L"";
        break;
    }
  }

  return true;
}

void Field::SetBorderStyle(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                           const CFX_WideString& swFieldName,
                           int nControlIndex,
                           const CFX_ByteString& string) {
  ASSERT(pFormFillEnv);

  BorderStyle nBorderStyle = BorderStyle::SOLID;
  if (string == "solid")
    nBorderStyle = BorderStyle::SOLID;
  else if (string == "beveled")
    nBorderStyle = BorderStyle::BEVELED;
  else if (string == "dashed")
    nBorderStyle = BorderStyle::DASH;
  else if (string == "inset")
    nBorderStyle = BorderStyle::INSET;
  else if (string == "underline")
    nBorderStyle = BorderStyle::UNDERLINE;
  else
    return;

  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);
  for (CPDF_FormField* pFormField : FieldArray) {
    if (nControlIndex < 0) {
      bool bSet = false;
      for (int i = 0, sz = pFormField->CountControls(); i < sz; ++i) {
        if (CPDFSDK_Widget* pWidget =
                GetWidget(pFormFillEnv, pFormField->GetControl(i))) {
          if (pWidget->GetBorderStyle() != nBorderStyle) {
            pWidget->SetBorderStyle(nBorderStyle);
            bSet = true;
          }
        }
      }
      if (bSet)
        UpdateFormField(pFormFillEnv, pFormField, true, true, true);
    } else {
      if (nControlIndex >= pFormField->CountControls())
        return;
      if (CPDF_FormControl* pFormControl =
              pFormField->GetControl(nControlIndex)) {
        if (CPDFSDK_Widget* pWidget = GetWidget(pFormFillEnv, pFormControl)) {
          if (pWidget->GetBorderStyle() != nBorderStyle) {
            pWidget->SetBorderStyle(nBorderStyle);
            UpdateFormControl(pFormFillEnv, pFormControl, true, true, true);
          }
        }
      }
    }
  }
}

bool Field::buttonAlignX(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_BUTTONALIGNX, nVP);
    } else {
      Field::SetButtonAlignX(m_pFormFillEnv.Get(), m_FieldName,
                             m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    CPDF_IconFit IconFit = pFormControl->GetIconFit();

    FX_FLOAT fLeft, fBottom;
    IconFit.GetIconPosition(fLeft, fBottom);

    vp << (int32_t)fLeft;
  }

  return true;
}

void Field::SetButtonAlignX(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            const CFX_WideString& swFieldName,
                            int nControlIndex,
                            int number) {
  // Not supported.
}

bool Field::buttonAlignY(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_BUTTONALIGNY, nVP);
    } else {
      Field::SetButtonAlignY(m_pFormFillEnv.Get(), m_FieldName,
                             m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    CPDF_IconFit IconFit = pFormControl->GetIconFit();

    FX_FLOAT fLeft, fBottom;
    IconFit.GetIconPosition(fLeft, fBottom);

    vp << (int32_t)fBottom;
  }

  return true;
}

void Field::SetButtonAlignY(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            const CFX_WideString& swFieldName,
                            int nControlIndex,
                            int number) {
  // Not supported.
}

bool Field::buttonFitBounds(CJS_Runtime* pRuntime,
                            CJS_PropValue& vp,
                            CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    if (m_bDelay) {
      AddDelay_Bool(FP_BUTTONFITBOUNDS, bVP);
    } else {
      Field::SetButtonFitBounds(m_pFormFillEnv.Get(), m_FieldName,
                                m_nFormControlIndex, bVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    vp << pFormControl->GetIconFit().GetFittingBounds();
  }

  return true;
}

void Field::SetButtonFitBounds(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                               const CFX_WideString& swFieldName,
                               int nControlIndex,
                               bool b) {
  // Not supported.
}

bool Field::buttonPosition(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_BUTTONPOSITION, nVP);
    } else {
      Field::SetButtonPosition(m_pFormFillEnv.Get(), m_FieldName,
                               m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    vp << pFormControl->GetTextPosition();
  }
  return true;
}

void Field::SetButtonPosition(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                              const CFX_WideString& swFieldName,
                              int nControlIndex,
                              int number) {
  // Not supported.
}

bool Field::buttonScaleHow(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_BUTTONSCALEHOW, nVP);
    } else {
      Field::SetButtonScaleHow(m_pFormFillEnv.Get(), m_FieldName,
                               m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    CPDF_IconFit IconFit = pFormControl->GetIconFit();
    if (IconFit.IsProportionalScale())
      vp << (int32_t)0;
    else
      vp << (int32_t)1;
  }

  return true;
}

void Field::SetButtonScaleHow(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                              const CFX_WideString& swFieldName,
                              int nControlIndex,
                              int number) {
  // Not supported.
}

bool Field::buttonScaleWhen(CJS_Runtime* pRuntime,
                            CJS_PropValue& vp,
                            CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_BUTTONSCALEWHEN, nVP);
    } else {
      Field::SetButtonScaleWhen(m_pFormFillEnv.Get(), m_FieldName,
                                m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
      return false;

    CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
    if (!pFormControl)
      return false;

    CPDF_IconFit IconFit = pFormControl->GetIconFit();
    int ScaleM = IconFit.GetScaleMethod();
    switch (ScaleM) {
      case CPDF_IconFit::Always:
        vp << (int32_t)CPDF_IconFit::Always;
        break;
      case CPDF_IconFit::Bigger:
        vp << (int32_t)CPDF_IconFit::Bigger;
        break;
      case CPDF_IconFit::Never:
        vp << (int32_t)CPDF_IconFit::Never;
        break;
      case CPDF_IconFit::Smaller:
        vp << (int32_t)CPDF_IconFit::Smaller;
        break;
    }
  }

  return true;
}

void Field::SetButtonScaleWhen(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                               const CFX_WideString& swFieldName,
                               int nControlIndex,
                               int number) {
  // Not supported.
}

bool Field::calcOrderIndex(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_CALCORDERINDEX, nVP);
    } else {
      Field::SetCalcOrderIndex(m_pFormFillEnv.Get(), m_FieldName,
                               m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_COMBOBOX &&
        pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD) {
      return false;
    }

    CPDFSDK_InterForm* pRDInterForm = m_pFormFillEnv->GetInterForm();
    CPDF_InterForm* pInterForm = pRDInterForm->GetInterForm();
    vp << (int32_t)pInterForm->FindFieldInCalculationOrder(pFormField);
  }

  return true;
}

void Field::SetCalcOrderIndex(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                              const CFX_WideString& swFieldName,
                              int nControlIndex,
                              int number) {
  // Not supported.
}

bool Field::charLimit(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;

    if (m_bDelay) {
      AddDelay_Int(FP_CHARLIMIT, nVP);
    } else {
      Field::SetCharLimit(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, nVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
      return false;

    vp << (int32_t)pFormField->GetMaxLen();
  }
  return true;
}

void Field::SetCharLimit(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         int number) {
  // Not supported.
}

bool Field::comb(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    if (m_bDelay) {
      AddDelay_Bool(FP_COMB, bVP);
    } else {
      Field::SetComb(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                     bVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
      return false;

    if (pFormField->GetFieldFlags() & FIELDFLAG_COMB)
      vp << true;
    else
      vp << false;
  }

  return true;
}

void Field::SetComb(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    const CFX_WideString& swFieldName,
                    int nControlIndex,
                    bool b) {
  // Not supported.
}

bool Field::commitOnSelChange(CJS_Runtime* pRuntime,
                              CJS_PropValue& vp,
                              CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    if (m_bDelay) {
      AddDelay_Bool(FP_COMMITONSELCHANGE, bVP);
    } else {
      Field::SetCommitOnSelChange(m_pFormFillEnv.Get(), m_FieldName,
                                  m_nFormControlIndex, bVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_COMBOBOX &&
        pFormField->GetFieldType() != FIELDTYPE_LISTBOX) {
      return false;
    }

    if (pFormField->GetFieldFlags() & FIELDFLAG_COMMITONSELCHANGE)
      vp << true;
    else
      vp << false;
  }

  return true;
}

void Field::SetCommitOnSelChange(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 const CFX_WideString& swFieldName,
                                 int nControlIndex,
                                 bool b) {
  // Not supported.
}

bool Field::currentValueIndices(CJS_Runtime* pRuntime,
                                CJS_PropValue& vp,
                                CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    std::vector<uint32_t> array;
    if (vp.GetJSValue()->GetType() == CJS_Value::VT_number) {
      int iSelecting = 0;
      vp >> iSelecting;
      array.push_back(iSelecting);
    } else if (vp.GetJSValue()->IsArrayObject()) {
      CJS_Array SelArray;
      CJS_Value SelValue(pRuntime);
      int iSelecting;
      vp >> SelArray;
      for (int i = 0, sz = SelArray.GetLength(pRuntime); i < sz; i++) {
        SelArray.GetElement(pRuntime, i, SelValue);
        iSelecting = SelValue.ToInt(pRuntime);
        array.push_back(iSelecting);
      }
    }

    if (m_bDelay) {
      AddDelay_WordArray(FP_CURRENTVALUEINDICES, array);
    } else {
      Field::SetCurrentValueIndices(m_pFormFillEnv.Get(), m_FieldName,
                                    m_nFormControlIndex, array);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_COMBOBOX &&
        pFormField->GetFieldType() != FIELDTYPE_LISTBOX) {
      return false;
    }

    if (pFormField->CountSelectedItems() == 1) {
      vp << pFormField->GetSelectedIndex(0);
    } else if (pFormField->CountSelectedItems() > 1) {
      CJS_Array SelArray;
      for (int i = 0, sz = pFormField->CountSelectedItems(); i < sz; i++) {
        SelArray.SetElement(
            pRuntime, i, CJS_Value(pRuntime, pFormField->GetSelectedIndex(i)));
      }
      vp << SelArray;
    } else {
      vp << -1;
    }
  }

  return true;
}

void Field::SetCurrentValueIndices(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                   const CFX_WideString& swFieldName,
                                   int nControlIndex,
                                   const std::vector<uint32_t>& array) {
  ASSERT(pFormFillEnv);
  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);

  for (CPDF_FormField* pFormField : FieldArray) {
    int nFieldType = pFormField->GetFieldType();
    if (nFieldType == FIELDTYPE_COMBOBOX || nFieldType == FIELDTYPE_LISTBOX) {
      uint32_t dwFieldFlags = pFormField->GetFieldFlags();
      pFormField->ClearSelection(true);
      for (size_t i = 0; i < array.size(); ++i) {
        if (i != 0 && !(dwFieldFlags & (1 << 21)))
          break;
        if (array[i] < static_cast<uint32_t>(pFormField->CountOptions()) &&
            !pFormField->IsItemSelected(array[i])) {
          pFormField->SetItemSelection(array[i], true);
        }
      }
      UpdateFormField(pFormFillEnv, pFormField, true, true, true);
    }
  }
}

bool Field::defaultStyle(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  return false;
}

void Field::SetDefaultStyle(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            const CFX_WideString& swFieldName,
                            int nControlIndex) {
  // Not supported.
}

bool Field::defaultValue(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_WideString WideStr;
    vp >> WideStr;

    if (m_bDelay) {
      AddDelay_WideString(FP_DEFAULTVALUE, WideStr);
    } else {
      Field::SetDefaultValue(m_pFormFillEnv.Get(), m_FieldName,
                             m_nFormControlIndex, WideStr);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() == FIELDTYPE_PUSHBUTTON ||
        pFormField->GetFieldType() == FIELDTYPE_SIGNATURE) {
      return false;
    }

    vp << pFormField->GetDefaultValue();
  }
  return true;
}

void Field::SetDefaultValue(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                            const CFX_WideString& swFieldName,
                            int nControlIndex,
                            const CFX_WideString& string) {
  // Not supported.
}

bool Field::doNotScroll(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    if (m_bDelay) {
      AddDelay_Bool(FP_DONOTSCROLL, bVP);
    } else {
      Field::SetDoNotScroll(m_pFormFillEnv.Get(), m_FieldName,
                            m_nFormControlIndex, bVP);
    }
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
      return false;

    if (pFormField->GetFieldFlags() & FIELDFLAG_DONOTSCROLL)
      vp << true;
    else
      vp << false;
  }

  return true;
}

void Field::SetDoNotScroll(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                           const CFX_WideString& swFieldName,
                           int nControlIndex,
                           bool b) {
  // Not supported.
}

bool Field::doNotSpellCheck(CJS_Runtime* pRuntime,
                            CJS_PropValue& vp,
                            CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
  } else {
    std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
    if (FieldArray.empty())
      return false;

    CPDF_FormField* pFormField = FieldArray[0];
    if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD &&
        pFormField->GetFieldType() != FIELDTYPE_COMBOBOX) {
      return false;
    }

    if (pFormField->GetFieldFlags() & FIELDFLAG_DONOTSPELLCHECK)
      vp << true;
    else
      vp << false;
  }

  return true;
}

void Field::SetDelay(bool bDelay) {
  m_bDelay = bDelay;

  if (!m_bDelay) {
    if (m_pJSDoc)
      m_pJSDoc->DoFieldDelay(m_FieldName, m_nFormControlIndex);
  }
}

bool Field::delay(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  if (!vp.IsSetting()) {
    vp << m_bDelay;
    return true;
  }
  if (!m_bCanSet)
    return false;

  bool bVP;
  vp >> bVP;
  SetDelay(bVP);
  return true;
}

bool Field::display(CJS_Runtime* pRuntime,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;
    if (m_bDelay) {
      AddDelay_Int(FP_DISPLAY, nVP);
    } else {
      Field::SetDisplay(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                        nVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDFSDK_Widget* pWidget =
      pInterForm->GetWidget(GetSmartFieldControl(pFormField));
  if (!pWidget)
    return false;

  uint32_t dwFlag = pWidget->GetFlags();
  if (ANNOTFLAG_INVISIBLE & dwFlag || ANNOTFLAG_HIDDEN & dwFlag) {
    vp << (int32_t)1;
  } else {
    if (ANNOTFLAG_PRINT & dwFlag) {
      if (ANNOTFLAG_NOVIEW & dwFlag) {
        vp << (int32_t)3;
      } else {
        vp << (int32_t)0;
      }
    } else {
      vp << (int32_t)2;
    }
  }
  return true;
}

void Field::SetDisplay(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                       const CFX_WideString& swFieldName,
                       int nControlIndex,
                       int number) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);
  for (CPDF_FormField* pFormField : FieldArray) {
    if (nControlIndex < 0) {
      bool bAnySet = false;
      for (int i = 0, sz = pFormField->CountControls(); i < sz; ++i) {
        CPDF_FormControl* pFormControl = pFormField->GetControl(i);
        ASSERT(pFormControl);

        CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl);
        if (SetWidgetDisplayStatus(pWidget, number))
          bAnySet = true;
      }

      if (bAnySet)
        UpdateFormField(pFormFillEnv, pFormField, true, false, true);
    } else {
      if (nControlIndex >= pFormField->CountControls())
        return;

      CPDF_FormControl* pFormControl = pFormField->GetControl(nControlIndex);
      if (!pFormControl)
        return;

      CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl);
      if (SetWidgetDisplayStatus(pWidget, number))
        UpdateFormControl(pFormFillEnv, pFormControl, true, false, true);
    }
  }
}

bool Field::doc(CJS_Runtime* pRuntime,
                CJS_PropValue& vp,
                CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  vp << m_pJSDoc->GetCJSDoc();
  return true;
}

bool Field::editable(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_COMBOBOX)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_EDIT);
  return true;
}

bool Field::exportValues(CJS_Runtime* pRuntime,
                         CJS_PropValue& vp,
                         CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_CHECKBOX &&
      pFormField->GetFieldType() != FIELDTYPE_RADIOBUTTON) {
    return false;
  }
  if (vp.IsSetting())
    return m_bCanSet && vp.GetJSValue()->IsArrayObject();

  CJS_Array ExportValusArray;
  if (m_nFormControlIndex < 0) {
    for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
      CPDF_FormControl* pFormControl = pFormField->GetControl(i);
      ExportValusArray.SetElement(
          pRuntime, i,
          CJS_Value(pRuntime, pFormControl->GetExportValue().c_str()));
    }
  } else {
    if (m_nFormControlIndex >= pFormField->CountControls())
      return false;

    CPDF_FormControl* pFormControl =
        pFormField->GetControl(m_nFormControlIndex);
    if (!pFormControl)
      return false;

    ExportValusArray.SetElement(
        pRuntime, 0,
        CJS_Value(pRuntime, pFormControl->GetExportValue().c_str()));
  }
  vp << ExportValusArray;
  return true;
}

bool Field::fileSelect(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    return true;
  }
  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_FILESELECT);
  return true;
}

bool Field::fillColor(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  CJS_Array crArray;
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    if (!vp.GetJSValue()->IsArrayObject())
      return false;

    vp >> crArray;

    CPWL_Color color;
    color::ConvertArrayToPWLColor(pRuntime, crArray, &color);
    if (m_bDelay) {
      AddDelay_Color(FP_FILLCOLOR, color);
    } else {
      Field::SetFillColor(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, color);
    }
    return true;
  }
  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  int iColorType;
  pFormControl->GetBackgroundColor(iColorType);

  CPWL_Color color;
  if (iColorType == COLORTYPE_TRANSPARENT) {
    color = CPWL_Color(COLORTYPE_TRANSPARENT);
  } else if (iColorType == COLORTYPE_GRAY) {
    color =
        CPWL_Color(COLORTYPE_GRAY, pFormControl->GetOriginalBackgroundColor(0));
  } else if (iColorType == COLORTYPE_RGB) {
    color =
        CPWL_Color(COLORTYPE_RGB, pFormControl->GetOriginalBackgroundColor(0),
                   pFormControl->GetOriginalBackgroundColor(1),
                   pFormControl->GetOriginalBackgroundColor(2));
  } else if (iColorType == COLORTYPE_CMYK) {
    color =
        CPWL_Color(COLORTYPE_CMYK, pFormControl->GetOriginalBackgroundColor(0),
                   pFormControl->GetOriginalBackgroundColor(1),
                   pFormControl->GetOriginalBackgroundColor(2),
                   pFormControl->GetOriginalBackgroundColor(3));
  } else {
    return false;
  }
  color::ConvertPWLColorToArray(pRuntime, color, &crArray);
  vp << crArray;
  return true;
}

void Field::SetFillColor(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         const CPWL_Color& color) {
  // Not supported.
}

bool Field::hidden(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    if (m_bDelay) {
      AddDelay_Bool(FP_HIDDEN, bVP);
    } else {
      Field::SetHidden(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                       bVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDFSDK_Widget* pWidget =
      pInterForm->GetWidget(GetSmartFieldControl(pFormField));
  if (!pWidget)
    return false;

  uint32_t dwFlags = pWidget->GetFlags();
  if (ANNOTFLAG_INVISIBLE & dwFlags || ANNOTFLAG_HIDDEN & dwFlags)
    vp << true;
  else
    vp << false;

  return true;
}

void Field::SetHidden(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      const CFX_WideString& swFieldName,
                      int nControlIndex,
                      bool b) {
  int display = b ? 1 /*Hidden*/ : 0 /*Visible*/;
  SetDisplay(pFormFillEnv, swFieldName, nControlIndex, display);
}

bool Field::highlight(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_ByteString strMode;
    vp >> strMode;

    if (m_bDelay) {
      AddDelay_String(FP_HIGHLIGHT, strMode);
    } else {
      Field::SetHighlight(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, strMode);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
    return false;

  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  int eHM = pFormControl->GetHighlightingMode();
  switch (eHM) {
    case CPDF_FormControl::None:
      vp << L"none";
      break;
    case CPDF_FormControl::Push:
      vp << L"push";
      break;
    case CPDF_FormControl::Invert:
      vp << L"invert";
      break;
    case CPDF_FormControl::Outline:
      vp << L"outline";
      break;
    case CPDF_FormControl::Toggle:
      vp << L"toggle";
      break;
  }
  return true;
}

void Field::SetHighlight(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         const CFX_ByteString& string) {
  // Not supported.
}

bool Field::lineWidth(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int iWidth;
    vp >> iWidth;

    if (m_bDelay) {
      AddDelay_Int(FP_LINEWIDTH, iWidth);
    } else {
      Field::SetLineWidth(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, iWidth);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  if (!pFormField->CountControls())
    return false;

  CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormField->GetControl(0));
  if (!pWidget)
    return false;

  vp << (int32_t)pWidget->GetBorderWidth();
  return true;
}

void Field::SetLineWidth(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         int number) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);
  for (CPDF_FormField* pFormField : FieldArray) {
    if (nControlIndex < 0) {
      bool bSet = false;
      for (int i = 0, sz = pFormField->CountControls(); i < sz; ++i) {
        CPDF_FormControl* pFormControl = pFormField->GetControl(i);
        ASSERT(pFormControl);

        if (CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl)) {
          if (number != pWidget->GetBorderWidth()) {
            pWidget->SetBorderWidth(number);
            bSet = true;
          }
        }
      }
      if (bSet)
        UpdateFormField(pFormFillEnv, pFormField, true, true, true);
    } else {
      if (nControlIndex >= pFormField->CountControls())
        return;
      if (CPDF_FormControl* pFormControl =
              pFormField->GetControl(nControlIndex)) {
        if (CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl)) {
          if (number != pWidget->GetBorderWidth()) {
            pWidget->SetBorderWidth(number);
            UpdateFormControl(pFormFillEnv, pFormControl, true, true, true);
          }
        }
      }
    }
  }
}

bool Field::multiline(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    if (m_bDelay) {
      AddDelay_Bool(FP_MULTILINE, bVP);
    } else {
      Field::SetMultiline(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, bVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
    return false;

  if (pFormField->GetFieldFlags() & FIELDFLAG_MULTILINE)
    vp << true;
  else
    vp << false;

  return true;
}

void Field::SetMultiline(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         bool b) {
  // Not supported.
}

bool Field::multipleSelection(CJS_Runtime* pRuntime,
                              CJS_PropValue& vp,
                              CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    if (m_bDelay) {
      AddDelay_Bool(FP_MULTIPLESELECTION, bVP);
    } else {
      Field::SetMultipleSelection(m_pFormFillEnv.Get(), m_FieldName,
                                  m_nFormControlIndex, bVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_LISTBOX)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_MULTISELECT);
  return true;
}

void Field::SetMultipleSelection(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 const CFX_WideString& swFieldName,
                                 int nControlIndex,
                                 bool b) {
  // Not supported.
}

bool Field::name(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  vp << m_FieldName;
  return true;
}

bool Field::numItems(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_COMBOBOX &&
      pFormField->GetFieldType() != FIELDTYPE_LISTBOX) {
    return false;
  }

  vp << (int32_t)pFormField->CountOptions();
  return true;
}

bool Field::page(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  if (!vp.IsGetting()) {
    sError = JSGetStringFromID(IDS_STRING_JSREADONLY);
    return false;
  }

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (!pFormField)
    return false;

  std::vector<CPDFSDK_Annot::ObservedPtr> widgets;
  m_pFormFillEnv->GetInterForm()->GetWidgets(pFormField, &widgets);
  if (widgets.empty()) {
    vp << (int32_t)-1;
    return true;
  }

  CJS_Array PageArray;
  int i = 0;
  for (const auto& pObserved : widgets) {
    if (!pObserved) {
      sError = JSGetStringFromID(IDS_STRING_JSBADOBJECT);
      return false;
    }

    auto pWidget = static_cast<CPDFSDK_Widget*>(pObserved.Get());
    CPDFSDK_PageView* pPageView = pWidget->GetPageView();
    if (!pPageView)
      return false;

    PageArray.SetElement(
        pRuntime, i, CJS_Value(pRuntime, (int32_t)pPageView->GetPageIndex()));
    ++i;
  }

  vp << PageArray;
  return true;
}

bool Field::password(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    if (m_bDelay) {
      AddDelay_Bool(FP_PASSWORD, bVP);
    } else {
      Field::SetPassword(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                         bVP);
    }
    return true;
  }

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_PASSWORD);
  return true;
}

void Field::SetPassword(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                        const CFX_WideString& swFieldName,
                        int nControlIndex,
                        bool b) {
  // Not supported.
}

bool Field::print(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;

    for (CPDF_FormField* pFormField : FieldArray) {
      if (m_nFormControlIndex < 0) {
        bool bSet = false;
        for (int i = 0, sz = pFormField->CountControls(); i < sz; ++i) {
          if (CPDFSDK_Widget* pWidget =
                  pInterForm->GetWidget(pFormField->GetControl(i))) {
            uint32_t dwFlags = pWidget->GetFlags();
            if (bVP)
              dwFlags |= ANNOTFLAG_PRINT;
            else
              dwFlags &= ~ANNOTFLAG_PRINT;

            if (dwFlags != pWidget->GetFlags()) {
              pWidget->SetFlags(dwFlags);
              bSet = true;
            }
          }
        }

        if (bSet)
          UpdateFormField(m_pFormFillEnv.Get(), pFormField, true, false, true);
      } else {
        if (m_nFormControlIndex >= pFormField->CountControls())
          return false;
        if (CPDF_FormControl* pFormControl =
                pFormField->GetControl(m_nFormControlIndex)) {
          if (CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl)) {
            uint32_t dwFlags = pWidget->GetFlags();
            if (bVP)
              dwFlags |= ANNOTFLAG_PRINT;
            else
              dwFlags &= ~ANNOTFLAG_PRINT;

            if (dwFlags != pWidget->GetFlags()) {
              pWidget->SetFlags(dwFlags);
              UpdateFormControl(m_pFormFillEnv.Get(),
                                pFormField->GetControl(m_nFormControlIndex),
                                true, false, true);
            }
          }
        }
      }
    }
    return true;
  }

  CPDF_FormField* pFormField = FieldArray[0];
  CPDFSDK_Widget* pWidget =
      pInterForm->GetWidget(GetSmartFieldControl(pFormField));
  if (!pWidget)
    return false;

  vp << !!(pWidget->GetFlags() & ANNOTFLAG_PRINT);
  return true;
}

bool Field::radiosInUnison(CJS_Runtime* pRuntime,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    return true;
  }
  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_RADIOBUTTON)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_RADIOSINUNISON);
  return true;
}

bool Field::readonly(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    return true;
  }
  vp << !!(FieldArray[0]->GetFieldFlags() & FIELDFLAG_READONLY);
  return true;
}

bool Field::rect(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  CJS_Value Upper_Leftx(pRuntime);
  CJS_Value Upper_Lefty(pRuntime);
  CJS_Value Lower_Rightx(pRuntime);
  CJS_Value Lower_Righty(pRuntime);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;
    if (!vp.GetJSValue()->IsArrayObject())
      return false;

    CJS_Array rcArray;
    vp >> rcArray;
    rcArray.GetElement(pRuntime, 0, Upper_Leftx);
    rcArray.GetElement(pRuntime, 1, Upper_Lefty);
    rcArray.GetElement(pRuntime, 2, Lower_Rightx);
    rcArray.GetElement(pRuntime, 3, Lower_Righty);

    FX_FLOAT pArray[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    pArray[0] = static_cast<FX_FLOAT>(Upper_Leftx.ToInt(pRuntime));
    pArray[1] = static_cast<FX_FLOAT>(Lower_Righty.ToInt(pRuntime));
    pArray[2] = static_cast<FX_FLOAT>(Lower_Rightx.ToInt(pRuntime));
    pArray[3] = static_cast<FX_FLOAT>(Upper_Lefty.ToInt(pRuntime));

    CFX_FloatRect crRect(pArray);
    if (m_bDelay) {
      AddDelay_Rect(FP_RECT, crRect);
    } else {
      Field::SetRect(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                     crRect);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDFSDK_Widget* pWidget =
      pInterForm->GetWidget(GetSmartFieldControl(pFormField));
  if (!pWidget)
    return false;

  CFX_FloatRect crRect = pWidget->GetRect();
  Upper_Leftx = CJS_Value(pRuntime, static_cast<int32_t>(crRect.left));
  Upper_Lefty = CJS_Value(pRuntime, static_cast<int32_t>(crRect.top));
  Lower_Rightx = CJS_Value(pRuntime, static_cast<int32_t>(crRect.right));
  Lower_Righty = CJS_Value(pRuntime, static_cast<int32_t>(crRect.bottom));

  CJS_Array rcArray;
  rcArray.SetElement(pRuntime, 0, Upper_Leftx);
  rcArray.SetElement(pRuntime, 1, Upper_Lefty);
  rcArray.SetElement(pRuntime, 2, Lower_Rightx);
  rcArray.SetElement(pRuntime, 3, Lower_Righty);
  vp << rcArray;
  return true;
}

void Field::SetRect(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    const CFX_WideString& swFieldName,
                    int nControlIndex,
                    const CFX_FloatRect& rect) {
  CPDFSDK_InterForm* pInterForm = pFormFillEnv->GetInterForm();
  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);
  for (CPDF_FormField* pFormField : FieldArray) {
    if (nControlIndex < 0) {
      bool bSet = false;
      for (int i = 0, sz = pFormField->CountControls(); i < sz; ++i) {
        CPDF_FormControl* pFormControl = pFormField->GetControl(i);
        ASSERT(pFormControl);

        if (CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl)) {
          CFX_FloatRect crRect = rect;

          CPDF_Page* pPDFPage = pWidget->GetPDFPage();
          crRect.Intersect(pPDFPage->GetPageBBox());

          if (!crRect.IsEmpty()) {
            CFX_FloatRect rcOld = pWidget->GetRect();
            if (crRect.left != rcOld.left || crRect.right != rcOld.right ||
                crRect.top != rcOld.top || crRect.bottom != rcOld.bottom) {
              pWidget->SetRect(crRect);
              bSet = true;
            }
          }
        }
      }

      if (bSet)
        UpdateFormField(pFormFillEnv, pFormField, true, true, true);
    } else {
      if (nControlIndex >= pFormField->CountControls())
        return;
      if (CPDF_FormControl* pFormControl =
              pFormField->GetControl(nControlIndex)) {
        if (CPDFSDK_Widget* pWidget = pInterForm->GetWidget(pFormControl)) {
          CFX_FloatRect crRect = rect;

          CPDF_Page* pPDFPage = pWidget->GetPDFPage();
          crRect.Intersect(pPDFPage->GetPageBBox());

          if (!crRect.IsEmpty()) {
            CFX_FloatRect rcOld = pWidget->GetRect();
            if (crRect.left != rcOld.left || crRect.right != rcOld.right ||
                crRect.top != rcOld.top || crRect.bottom != rcOld.bottom) {
              pWidget->SetRect(crRect);
              UpdateFormControl(pFormFillEnv, pFormControl, true, true, true);
            }
          }
        }
      }
    }
  }
}

bool Field::required(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    return true;
  }
  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() == FIELDTYPE_PUSHBUTTON)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_REQUIRED);
  return true;
}

bool Field::richText(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    bool bVP;
    vp >> bVP;
    if (m_bDelay)
      AddDelay_Bool(FP_RICHTEXT, bVP);

    return true;
  }

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_TEXTFIELD)
    return false;

  vp << !!(pFormField->GetFieldFlags() & FIELDFLAG_RICHTEXT);
  return true;
}

bool Field::richValue(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  return true;
}

bool Field::rotation(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;
    if (m_bDelay) {
      AddDelay_Int(FP_ROTATION, nVP);
    } else {
      Field::SetRotation(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                         nVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  vp << (int32_t)pFormControl->GetRotation();
  return true;
}

void Field::SetRotation(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                        const CFX_WideString& swFieldName,
                        int nControlIndex,
                        int number) {
  // Not supported.
}

bool Field::strokeColor(CJS_Runtime* pRuntime,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  CJS_Array crArray;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    if (!vp.GetJSValue()->IsArrayObject())
      return false;

    vp >> crArray;

    CPWL_Color color;
    color::ConvertArrayToPWLColor(pRuntime, crArray, &color);
    if (m_bDelay) {
      AddDelay_Color(FP_STROKECOLOR, color);
    } else {
      Field::SetStrokeColor(m_pFormFillEnv.Get(), m_FieldName,
                            m_nFormControlIndex, color);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  int iColorType;
  pFormControl->GetBorderColor(iColorType);

  CPWL_Color color;
  if (iColorType == COLORTYPE_TRANSPARENT) {
    color = CPWL_Color(COLORTYPE_TRANSPARENT);
  } else if (iColorType == COLORTYPE_GRAY) {
    color = CPWL_Color(COLORTYPE_GRAY, pFormControl->GetOriginalBorderColor(0));
  } else if (iColorType == COLORTYPE_RGB) {
    color = CPWL_Color(COLORTYPE_RGB, pFormControl->GetOriginalBorderColor(0),
                       pFormControl->GetOriginalBorderColor(1),
                       pFormControl->GetOriginalBorderColor(2));
  } else if (iColorType == COLORTYPE_CMYK) {
    color = CPWL_Color(COLORTYPE_CMYK, pFormControl->GetOriginalBorderColor(0),
                       pFormControl->GetOriginalBorderColor(1),
                       pFormControl->GetOriginalBorderColor(2),
                       pFormControl->GetOriginalBorderColor(3));
  } else {
    return false;
  }

  color::ConvertPWLColorToArray(pRuntime, color, &crArray);
  vp << crArray;
  return true;
}

void Field::SetStrokeColor(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                           const CFX_WideString& swFieldName,
                           int nControlIndex,
                           const CPWL_Color& color) {
  // Not supported.
}

bool Field::style(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_ByteString csBCaption;
    vp >> csBCaption;

    if (m_bDelay) {
      AddDelay_String(FP_STYLE, csBCaption);
    } else {
      Field::SetStyle(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                      csBCaption);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_RADIOBUTTON &&
      pFormField->GetFieldType() != FIELDTYPE_CHECKBOX) {
    return false;
  }

  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  CFX_WideString csWCaption = pFormControl->GetNormalCaption();
  CFX_ByteString csBCaption;

  switch (csWCaption[0]) {
    case L'l':
      csBCaption = "circle";
      break;
    case L'8':
      csBCaption = "cross";
      break;
    case L'u':
      csBCaption = "diamond";
      break;
    case L'n':
      csBCaption = "square";
      break;
    case L'H':
      csBCaption = "star";
      break;
    default:  // L'4'
      csBCaption = "check";
      break;
  }
  vp << csBCaption;
  return true;
}

void Field::SetStyle(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     const CFX_WideString& swFieldName,
                     int nControlIndex,
                     const CFX_ByteString& string) {
  // Not supported.
}

bool Field::submitName(CJS_Runtime* pRuntime,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  return true;
}

bool Field::textColor(CJS_Runtime* pRuntime,
                      CJS_PropValue& vp,
                      CFX_WideString& sError) {
  CJS_Array crArray;

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    if (!vp.GetJSValue()->IsArrayObject())
      return false;

    vp >> crArray;

    CPWL_Color color;
    color::ConvertArrayToPWLColor(pRuntime, crArray, &color);
    if (m_bDelay) {
      AddDelay_Color(FP_TEXTCOLOR, color);
    } else {
      Field::SetTextColor(m_pFormFillEnv.Get(), m_FieldName,
                          m_nFormControlIndex, color);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  int iColorType;
  FX_ARGB color;
  CPDF_DefaultAppearance FieldAppearance = pFormControl->GetDefaultAppearance();
  FieldAppearance.GetColor(color, iColorType);

  int32_t a;
  int32_t r;
  int32_t g;
  int32_t b;
  ArgbDecode(color, a, r, g, b);

  CPWL_Color crRet =
      CPWL_Color(COLORTYPE_RGB, r / 255.0f, g / 255.0f, b / 255.0f);

  if (iColorType == COLORTYPE_TRANSPARENT)
    crRet = CPWL_Color(COLORTYPE_TRANSPARENT);

  color::ConvertPWLColorToArray(pRuntime, crRet, &crArray);
  vp << crArray;
  return true;
}

void Field::SetTextColor(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         const CFX_WideString& swFieldName,
                         int nControlIndex,
                         const CPWL_Color& color) {
  // Not supported.
}

bool Field::textFont(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_ByteString csFontName;
    vp >> csFontName;
    if (csFontName.IsEmpty())
      return false;

    if (m_bDelay) {
      AddDelay_String(FP_TEXTFONT, csFontName);
    } else {
      Field::SetTextFont(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                         csFontName);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  int nFieldType = pFormField->GetFieldType();
  if (nFieldType != FIELDTYPE_PUSHBUTTON && nFieldType != FIELDTYPE_COMBOBOX &&
      nFieldType != FIELDTYPE_LISTBOX && nFieldType != FIELDTYPE_TEXTFIELD) {
    return false;
  }
  CPDF_Font* pFont = pFormControl->GetDefaultControlFont();
  if (!pFont)
    return false;

  vp << pFont->GetBaseFont();
  return true;
}

void Field::SetTextFont(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                        const CFX_WideString& swFieldName,
                        int nControlIndex,
                        const CFX_ByteString& string) {
  // Not supported.
}

bool Field::textSize(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    int nVP;
    vp >> nVP;
    if (m_bDelay) {
      AddDelay_Int(FP_TEXTSIZE, nVP);
    } else {
      Field::SetTextSize(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                         nVP);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  ASSERT(pFormField);
  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  CPDF_DefaultAppearance FieldAppearance = pFormControl->GetDefaultAppearance();

  CFX_ByteString csFontNameTag;
  FX_FLOAT fFontSize;
  FieldAppearance.GetFont(csFontNameTag, fFontSize);
  vp << (int)fFontSize;
  return true;
}

void Field::SetTextSize(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                        const CFX_WideString& swFieldName,
                        int nControlIndex,
                        int number) {
  // Not supported.
}

bool Field::type(CJS_Runtime* pRuntime,
                 CJS_PropValue& vp,
                 CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  switch (pFormField->GetFieldType()) {
    case FIELDTYPE_UNKNOWN:
      vp << L"unknown";
      break;
    case FIELDTYPE_PUSHBUTTON:
      vp << L"button";
      break;
    case FIELDTYPE_CHECKBOX:
      vp << L"checkbox";
      break;
    case FIELDTYPE_RADIOBUTTON:
      vp << L"radiobutton";
      break;
    case FIELDTYPE_COMBOBOX:
      vp << L"combobox";
      break;
    case FIELDTYPE_LISTBOX:
      vp << L"listbox";
      break;
    case FIELDTYPE_TEXTFIELD:
      vp << L"text";
      break;
    case FIELDTYPE_SIGNATURE:
      vp << L"signature";
      break;
    default:
      vp << L"unknown";
      break;
  }
  return true;
}

bool Field::userName(CJS_Runtime* pRuntime,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  ASSERT(m_pFormFillEnv);

  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    CFX_WideString swName;
    vp >> swName;

    if (m_bDelay) {
      AddDelay_WideString(FP_USERNAME, swName);
    } else {
      Field::SetUserName(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                         swName);
    }
    return true;
  }
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  vp << FieldArray[0]->GetAlternateName();
  return true;
}

void Field::SetUserName(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                        const CFX_WideString& swFieldName,
                        int nControlIndex,
                        const CFX_WideString& string) {
  // Not supported.
}

bool Field::value(CJS_Runtime* pRuntime,
                  CJS_PropValue& vp,
                  CFX_WideString& sError) {
  if (vp.IsSetting()) {
    if (!m_bCanSet)
      return false;

    std::vector<CFX_WideString> strArray;
    if (vp.GetJSValue()->IsArrayObject()) {
      CJS_Array ValueArray;
      vp.GetJSValue()->ConvertToArray(pRuntime, ValueArray);
      for (int i = 0, sz = ValueArray.GetLength(pRuntime); i < sz; i++) {
        CJS_Value ElementValue(pRuntime);
        ValueArray.GetElement(pRuntime, i, ElementValue);
        strArray.push_back(ElementValue.ToCFXWideString(pRuntime));
      }
    } else {
      CFX_WideString swValue;
      vp >> swValue;
      strArray.push_back(swValue);
    }

    if (m_bDelay) {
      AddDelay_WideStringArray(FP_VALUE, strArray);
    } else {
      Field::SetValue(m_pFormFillEnv.Get(), m_FieldName, m_nFormControlIndex,
                      strArray);
    }
    return true;
  }

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  switch (pFormField->GetFieldType()) {
    case FIELDTYPE_PUSHBUTTON:
      return false;
    case FIELDTYPE_COMBOBOX:
    case FIELDTYPE_TEXTFIELD: {
      vp << pFormField->GetValue();
    } break;
    case FIELDTYPE_LISTBOX: {
      if (pFormField->CountSelectedItems() > 1) {
        CJS_Array ValueArray;
        CJS_Value ElementValue(pRuntime);
        int iIndex;
        for (int i = 0, sz = pFormField->CountSelectedItems(); i < sz; i++) {
          iIndex = pFormField->GetSelectedIndex(i);
          ElementValue =
              CJS_Value(pRuntime, pFormField->GetOptionValue(iIndex).c_str());
          if (FXSYS_wcslen(ElementValue.ToCFXWideString(pRuntime).c_str()) ==
              0) {
            ElementValue =
                CJS_Value(pRuntime, pFormField->GetOptionLabel(iIndex).c_str());
          }
          ValueArray.SetElement(pRuntime, i, ElementValue);
        }
        vp << ValueArray;
      } else {
        vp << pFormField->GetValue();
      }
    } break;
    case FIELDTYPE_CHECKBOX:
    case FIELDTYPE_RADIOBUTTON: {
      bool bFind = false;
      for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
        if (pFormField->GetControl(i)->IsChecked()) {
          vp << pFormField->GetControl(i)->GetExportValue();
          bFind = true;
          break;
        }
      }
      if (!bFind)
        vp << L"Off";
    } break;
    default:
      vp << pFormField->GetValue();
      break;
  }
  vp.GetJSValue()->MaybeCoerceToNumber(pRuntime);
  return true;
}

void Field::SetValue(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     const CFX_WideString& swFieldName,
                     int nControlIndex,
                     const std::vector<CFX_WideString>& strArray) {
  ASSERT(pFormFillEnv);
  if (strArray.empty())
    return;

  std::vector<CPDF_FormField*> FieldArray =
      GetFormFields(pFormFillEnv, swFieldName);

  for (CPDF_FormField* pFormField : FieldArray) {
    if (pFormField->GetFullName().Compare(swFieldName) != 0)
      continue;

    switch (pFormField->GetFieldType()) {
      case FIELDTYPE_TEXTFIELD:
      case FIELDTYPE_COMBOBOX:
        if (pFormField->GetValue() != strArray[0]) {
          pFormField->SetValue(strArray[0], true);
          UpdateFormField(pFormFillEnv, pFormField, true, false, true);
        }
        break;
      case FIELDTYPE_CHECKBOX:
      case FIELDTYPE_RADIOBUTTON: {
        if (pFormField->GetValue() != strArray[0]) {
          pFormField->SetValue(strArray[0], true);
          UpdateFormField(pFormFillEnv, pFormField, true, false, true);
        }
      } break;
      case FIELDTYPE_LISTBOX: {
        bool bModified = false;
        for (const auto& str : strArray) {
          if (!pFormField->IsItemSelected(pFormField->FindOption(str))) {
            bModified = true;
            break;
          }
        }
        if (bModified) {
          pFormField->ClearSelection(true);
          for (const auto& str : strArray) {
            int index = pFormField->FindOption(str);
            if (!pFormField->IsItemSelected(index))
              pFormField->SetItemSelection(index, true, true);
          }
          UpdateFormField(pFormFillEnv, pFormField, true, false, true);
        }
      } break;
      default:
        break;
    }
  }
}

bool Field::valueAsString(CJS_Runtime* pRuntime,
                          CJS_PropValue& vp,
                          CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() == FIELDTYPE_PUSHBUTTON)
    return false;

  if (pFormField->GetFieldType() == FIELDTYPE_CHECKBOX) {
    if (!pFormField->CountControls())
      return false;

    if (pFormField->GetControl(0)->IsChecked())
      vp << L"Yes";
    else
      vp << L"Off";
  } else if (pFormField->GetFieldType() == FIELDTYPE_RADIOBUTTON &&
             !(pFormField->GetFieldFlags() & FIELDFLAG_RADIOSINUNISON)) {
    for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
      if (pFormField->GetControl(i)->IsChecked()) {
        vp << pFormField->GetControl(i)->GetExportValue().c_str();
        break;
      } else {
        vp << L"Off";
      }
    }
  } else if (pFormField->GetFieldType() == FIELDTYPE_LISTBOX &&
             (pFormField->CountSelectedItems() > 1)) {
    vp << L"";
  } else {
    vp << pFormField->GetValue().c_str();
  }

  return true;
}

bool Field::browseForFileToSubmit(CJS_Runtime* pRuntime,
                                  const std::vector<CJS_Value>& params,
                                  CJS_Value& vRet,
                                  CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if ((pFormField->GetFieldFlags() & FIELDFLAG_FILESELECT) &&
      (pFormField->GetFieldType() == FIELDTYPE_TEXTFIELD)) {
    CFX_WideString wsFileName = m_pFormFillEnv->JS_fieldBrowse();
    if (!wsFileName.IsEmpty()) {
      pFormField->SetValue(wsFileName);
      UpdateFormField(m_pFormFillEnv.Get(), pFormField, true, true, true);
    }
    return true;
  }
  return false;
}

bool Field::buttonGetCaption(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  int nface = 0;
  int iSize = params.size();
  if (iSize >= 1)
    nface = params[0].ToInt(pRuntime);

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
    return false;

  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  if (nface == 0)
    vRet = CJS_Value(pRuntime, pFormControl->GetNormalCaption().c_str());
  else if (nface == 1)
    vRet = CJS_Value(pRuntime, pFormControl->GetDownCaption().c_str());
  else if (nface == 2)
    vRet = CJS_Value(pRuntime, pFormControl->GetRolloverCaption().c_str());
  else
    return false;

  return true;
}

bool Field::buttonGetIcon(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  if (params.size() >= 1) {
    int nFace = params[0].ToInt(pRuntime);
    if (nFace < 0 || nFace > 2)
      return false;
  }

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_PUSHBUTTON)
    return false;

  CPDF_FormControl* pFormControl = GetSmartFieldControl(pFormField);
  if (!pFormControl)
    return false;

  v8::Local<v8::Object> pObj =
      pRuntime->NewFxDynamicObj(CJS_Icon::g_nObjDefnID);
  if (pObj.IsEmpty())
    return false;

  CJS_Icon* pJS_Icon = static_cast<CJS_Icon*>(pRuntime->GetObjectPrivate(pObj));
  vRet = CJS_Value(pRuntime, pJS_Icon);
  return true;
}

bool Field::buttonImportIcon(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  return true;
}

bool Field::buttonSetCaption(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  return false;
}

bool Field::buttonSetIcon(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  return false;
}

bool Field::checkThisBox(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  int iSize = params.size();
  if (iSize < 1)
    return false;

  if (!m_bCanSet)
    return false;

  int nWidget = params[0].ToInt(pRuntime);
  bool bCheckit = true;
  if (iSize >= 2)
    bCheckit = params[1].ToBool(pRuntime);

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (pFormField->GetFieldType() != FIELDTYPE_CHECKBOX &&
      pFormField->GetFieldType() != FIELDTYPE_RADIOBUTTON)
    return false;
  if (nWidget < 0 || nWidget >= pFormField->CountControls())
    return false;
  // TODO(weili): Check whether anything special needed for radio button,
  // otherwise merge these branches.
  if (pFormField->GetFieldType() == FIELDTYPE_RADIOBUTTON)
    pFormField->CheckControl(nWidget, bCheckit, true);
  else
    pFormField->CheckControl(nWidget, bCheckit, true);

  UpdateFormField(m_pFormFillEnv.Get(), pFormField, true, true, true);
  return true;
}

bool Field::clearItems(CJS_Runtime* pRuntime,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  return true;
}

bool Field::defaultIsChecked(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  if (!m_bCanSet)
    return false;

  int iSize = params.size();
  if (iSize < 1)
    return false;

  int nWidget = params[0].ToInt(pRuntime);
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (nWidget < 0 || nWidget >= pFormField->CountControls())
    return false;

  vRet = CJS_Value(pRuntime,
                   pFormField->GetFieldType() == FIELDTYPE_CHECKBOX ||
                       pFormField->GetFieldType() == FIELDTYPE_RADIOBUTTON);

  return true;
}

bool Field::deleteItemAt(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  return true;
}

bool Field::getArray(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  std::vector<std::unique_ptr<CFX_WideString>> swSort;
  for (CPDF_FormField* pFormField : FieldArray) {
    swSort.push_back(std::unique_ptr<CFX_WideString>(
        new CFX_WideString(pFormField->GetFullName())));
  }

  std::sort(
      swSort.begin(), swSort.end(),
      [](const std::unique_ptr<CFX_WideString>& p1,
         const std::unique_ptr<CFX_WideString>& p2) { return *p1 < *p2; });

  CJS_Array FormFieldArray;

  int j = 0;
  for (const auto& pStr : swSort) {
    v8::Local<v8::Object> pObj =
        pRuntime->NewFxDynamicObj(CJS_Field::g_nObjDefnID);
    if (pObj.IsEmpty())
      return false;

    CJS_Field* pJSField =
        static_cast<CJS_Field*>(pRuntime->GetObjectPrivate(pObj));
    Field* pField = static_cast<Field*>(pJSField->GetEmbedObject());
    pField->AttachField(m_pJSDoc, *pStr);
    FormFieldArray.SetElement(pRuntime, j++, CJS_Value(pRuntime, pJSField));
  }

  vRet = CJS_Value(pRuntime, FormFieldArray);
  return true;
}

bool Field::getItemAt(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  int iSize = params.size();
  int nIdx = -1;
  if (iSize >= 1)
    nIdx = params[0].ToInt(pRuntime);

  bool bExport = true;
  if (iSize >= 2)
    bExport = params[1].ToBool(pRuntime);

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if ((pFormField->GetFieldType() == FIELDTYPE_LISTBOX) ||
      (pFormField->GetFieldType() == FIELDTYPE_COMBOBOX)) {
    if (nIdx == -1 || nIdx > pFormField->CountOptions())
      nIdx = pFormField->CountOptions() - 1;
    if (bExport) {
      CFX_WideString strval = pFormField->GetOptionValue(nIdx);
      if (strval.IsEmpty())
        vRet = CJS_Value(pRuntime, pFormField->GetOptionLabel(nIdx).c_str());
      else
        vRet = CJS_Value(pRuntime, strval.c_str());
    } else {
      vRet = CJS_Value(pRuntime, pFormField->GetOptionLabel(nIdx).c_str());
    }
  } else {
    return false;
  }

  return true;
}

bool Field::getLock(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  return false;
}

bool Field::insertItemAt(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  return true;
}

bool Field::isBoxChecked(CJS_Runtime* pRuntime,
                         const std::vector<CJS_Value>& params,
                         CJS_Value& vRet,
                         CFX_WideString& sError) {
  int nIndex = -1;
  if (params.size() >= 1)
    nIndex = params[0].ToInt(pRuntime);

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (nIndex < 0 || nIndex >= pFormField->CountControls()) {
    return false;
  }

  vRet = CJS_Value(pRuntime,
                   ((pFormField->GetFieldType() == FIELDTYPE_CHECKBOX ||
                     pFormField->GetFieldType() == FIELDTYPE_RADIOBUTTON) &&
                    pFormField->GetControl(nIndex)->IsChecked() != 0));
  return true;
}

bool Field::isDefaultChecked(CJS_Runtime* pRuntime,
                             const std::vector<CJS_Value>& params,
                             CJS_Value& vRet,
                             CFX_WideString& sError) {
  int nIndex = -1;
  if (params.size() >= 1)
    nIndex = params[0].ToInt(pRuntime);

  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  if (nIndex < 0 || nIndex >= pFormField->CountControls())
    return false;

  vRet = CJS_Value(pRuntime,
                   ((pFormField->GetFieldType() == FIELDTYPE_CHECKBOX ||
                     pFormField->GetFieldType() == FIELDTYPE_RADIOBUTTON) &&
                    pFormField->GetControl(nIndex)->IsDefaultChecked() != 0));
  return true;
}

bool Field::setAction(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  return true;
}

bool Field::setFocus(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  std::vector<CPDF_FormField*> FieldArray = GetFormFields(m_FieldName);
  if (FieldArray.empty())
    return false;

  CPDF_FormField* pFormField = FieldArray[0];
  int32_t nCount = pFormField->CountControls();
  if (nCount < 1)
    return false;

  CPDFSDK_InterForm* pInterForm = m_pFormFillEnv->GetInterForm();
  CPDFSDK_Widget* pWidget = nullptr;
  if (nCount == 1) {
    pWidget = pInterForm->GetWidget(pFormField->GetControl(0));
  } else {
    UnderlyingPageType* pPage =
        UnderlyingFromFPDFPage(m_pFormFillEnv->GetCurrentPage(
            m_pFormFillEnv->GetUnderlyingDocument()));
    if (!pPage)
      return false;
    if (CPDFSDK_PageView* pCurPageView =
            m_pFormFillEnv->GetPageView(pPage, true)) {
      for (int32_t i = 0; i < nCount; i++) {
        if (CPDFSDK_Widget* pTempWidget =
                pInterForm->GetWidget(pFormField->GetControl(i))) {
          if (pTempWidget->GetPDFPage() == pCurPageView->GetPDFPage()) {
            pWidget = pTempWidget;
            break;
          }
        }
      }
    }
  }

  if (pWidget) {
    CPDFSDK_Annot::ObservedPtr pObserved(pWidget);
    m_pFormFillEnv->SetFocusAnnot(&pObserved);
  }

  return true;
}

bool Field::setItems(CJS_Runtime* pRuntime,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  return true;
}

bool Field::setLock(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  return false;
}

bool Field::signatureGetModifications(CJS_Runtime* pRuntime,
                                      const std::vector<CJS_Value>& params,
                                      CJS_Value& vRet,
                                      CFX_WideString& sError) {
  return false;
}

bool Field::signatureGetSeedValue(CJS_Runtime* pRuntime,
                                  const std::vector<CJS_Value>& params,
                                  CJS_Value& vRet,
                                  CFX_WideString& sError) {
  return false;
}

bool Field::signatureInfo(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  return false;
}

bool Field::signatureSetSeedValue(CJS_Runtime* pRuntime,
                                  const std::vector<CJS_Value>& params,
                                  CJS_Value& vRet,
                                  CFX_WideString& sError) {
  return false;
}

bool Field::signatureSign(CJS_Runtime* pRuntime,
                          const std::vector<CJS_Value>& params,
                          CJS_Value& vRet,
                          CFX_WideString& sError) {
  return false;
}

bool Field::signatureValidate(CJS_Runtime* pRuntime,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError) {
  return false;
}

bool Field::source(CJS_Runtime* pRuntime,
                   CJS_PropValue& vp,
                   CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << (CJS_Object*)nullptr;
  }

  return true;
}

void Field::AddDelay_Int(FIELD_PROP prop, int32_t n) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->num = n;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_Bool(FIELD_PROP prop, bool b) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->b = b;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_String(FIELD_PROP prop, const CFX_ByteString& string) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->string = string;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_WideString(FIELD_PROP prop, const CFX_WideString& string) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->widestring = string;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_Rect(FIELD_PROP prop, const CFX_FloatRect& rect) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->rect = rect;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_Color(FIELD_PROP prop, const CPWL_Color& color) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->color = color;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_WordArray(FIELD_PROP prop,
                               const std::vector<uint32_t>& array) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->wordarray = array;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::AddDelay_WideStringArray(FIELD_PROP prop,
                                     const std::vector<CFX_WideString>& array) {
  CJS_DelayData* pNewData =
      new CJS_DelayData(prop, m_nFormControlIndex, m_FieldName);
  pNewData->widestringarray = array;
  m_pJSDoc->AddDelayData(pNewData);
}

void Field::DoDelay(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    CJS_DelayData* pData) {
  ASSERT(pFormFillEnv);
  switch (pData->eProp) {
    case FP_ALIGNMENT:
      Field::SetAlignment(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->string);
      break;
    case FP_BORDERSTYLE:
      Field::SetBorderStyle(pFormFillEnv, pData->sFieldName,
                            pData->nControlIndex, pData->string);
      break;
    case FP_BUTTONALIGNX:
      Field::SetButtonAlignX(pFormFillEnv, pData->sFieldName,
                             pData->nControlIndex, pData->num);
      break;
    case FP_BUTTONALIGNY:
      Field::SetButtonAlignY(pFormFillEnv, pData->sFieldName,
                             pData->nControlIndex, pData->num);
      break;
    case FP_BUTTONFITBOUNDS:
      Field::SetButtonFitBounds(pFormFillEnv, pData->sFieldName,
                                pData->nControlIndex, pData->b);
      break;
    case FP_BUTTONPOSITION:
      Field::SetButtonPosition(pFormFillEnv, pData->sFieldName,
                               pData->nControlIndex, pData->num);
      break;
    case FP_BUTTONSCALEHOW:
      Field::SetButtonScaleHow(pFormFillEnv, pData->sFieldName,
                               pData->nControlIndex, pData->num);
      break;
    case FP_BUTTONSCALEWHEN:
      Field::SetButtonScaleWhen(pFormFillEnv, pData->sFieldName,
                                pData->nControlIndex, pData->num);
      break;
    case FP_CALCORDERINDEX:
      Field::SetCalcOrderIndex(pFormFillEnv, pData->sFieldName,
                               pData->nControlIndex, pData->num);
      break;
    case FP_CHARLIMIT:
      Field::SetCharLimit(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->num);
      break;
    case FP_COMB:
      Field::SetComb(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                     pData->b);
      break;
    case FP_COMMITONSELCHANGE:
      Field::SetCommitOnSelChange(pFormFillEnv, pData->sFieldName,
                                  pData->nControlIndex, pData->b);
      break;
    case FP_CURRENTVALUEINDICES:
      Field::SetCurrentValueIndices(pFormFillEnv, pData->sFieldName,
                                    pData->nControlIndex, pData->wordarray);
      break;
    case FP_DEFAULTVALUE:
      Field::SetDefaultValue(pFormFillEnv, pData->sFieldName,
                             pData->nControlIndex, pData->widestring);
      break;
    case FP_DONOTSCROLL:
      Field::SetDoNotScroll(pFormFillEnv, pData->sFieldName,
                            pData->nControlIndex, pData->b);
      break;
    case FP_DISPLAY:
      Field::SetDisplay(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                        pData->num);
      break;
    case FP_FILLCOLOR:
      Field::SetFillColor(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->color);
      break;
    case FP_HIDDEN:
      Field::SetHidden(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                       pData->b);
      break;
    case FP_HIGHLIGHT:
      Field::SetHighlight(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->string);
      break;
    case FP_LINEWIDTH:
      Field::SetLineWidth(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->num);
      break;
    case FP_MULTILINE:
      Field::SetMultiline(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->b);
      break;
    case FP_MULTIPLESELECTION:
      Field::SetMultipleSelection(pFormFillEnv, pData->sFieldName,
                                  pData->nControlIndex, pData->b);
      break;
    case FP_PASSWORD:
      Field::SetPassword(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                         pData->b);
      break;
    case FP_RECT:
      Field::SetRect(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                     pData->rect);
      break;
    case FP_RICHTEXT:
      // Not supported.
      break;
    case FP_RICHVALUE:
      break;
    case FP_ROTATION:
      Field::SetRotation(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                         pData->num);
      break;
    case FP_STROKECOLOR:
      Field::SetStrokeColor(pFormFillEnv, pData->sFieldName,
                            pData->nControlIndex, pData->color);
      break;
    case FP_STYLE:
      Field::SetStyle(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                      pData->string);
      break;
    case FP_TEXTCOLOR:
      Field::SetTextColor(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                          pData->color);
      break;
    case FP_TEXTFONT:
      Field::SetTextFont(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                         pData->string);
      break;
    case FP_TEXTSIZE:
      Field::SetTextSize(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                         pData->num);
      break;
    case FP_USERNAME:
      Field::SetUserName(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                         pData->widestring);
      break;
    case FP_VALUE:
      Field::SetValue(pFormFillEnv, pData->sFieldName, pData->nControlIndex,
                      pData->widestringarray);
      break;
  }
}

void Field::AddField(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     int nPageIndex,
                     int nFieldType,
                     const CFX_WideString& sName,
                     const CFX_FloatRect& rcCoords) {
  // Not supported.
}
