// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_interform.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cfdf_document.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfdoc/cpdf_actionfields.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cba_annotiterator.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/fsdk_actionhandler.h"
#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/fxedit/fxet_edit.h"
#include "fpdfsdk/ipdfsdk_annothandler.h"
#include "fpdfsdk/javascript/ijs_event_context.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "third_party/base/stl_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/cpdfsdk_xfawidget.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffwidget.h"
#include "xfa/fxfa/xfa_ffwidgethandler.h"
#endif  // PDF_ENABLE_XFA

CPDFSDK_InterForm::CPDFSDK_InterForm(CPDFSDK_FormFillEnvironment* pFormFillEnv)
    : m_pFormFillEnv(pFormFillEnv),
      m_pInterForm(new CPDF_InterForm(m_pFormFillEnv->GetPDFDocument())),
#ifdef PDF_ENABLE_XFA
      m_bXfaCalculate(true),
      m_bXfaValidationsEnabled(true),
#endif  // PDF_ENABLE_XFA
      m_bCalculate(true),
      m_bBusy(false),
      m_iHighlightAlpha(0) {
  m_pInterForm->SetFormNotify(this);
  for (int i = 0; i < kNumFieldTypes; ++i)
    m_bNeedHightlight[i] = false;
}

CPDFSDK_InterForm::~CPDFSDK_InterForm() {
  m_Map.clear();
#ifdef PDF_ENABLE_XFA
  m_XFAMap.clear();
#endif  // PDF_ENABLE_XFA
}

bool CPDFSDK_InterForm::HighlightWidgets() {
  return false;
}

CPDFSDK_Widget* CPDFSDK_InterForm::GetSibling(CPDFSDK_Widget* pWidget,
                                              bool bNext) const {
  std::unique_ptr<CBA_AnnotIterator> pIterator(new CBA_AnnotIterator(
      pWidget->GetPageView(), CPDF_Annot::Subtype::WIDGET));

  if (bNext)
    return static_cast<CPDFSDK_Widget*>(pIterator->GetNextAnnot(pWidget));

  return static_cast<CPDFSDK_Widget*>(pIterator->GetPrevAnnot(pWidget));
}

CPDFSDK_Widget* CPDFSDK_InterForm::GetWidget(CPDF_FormControl* pControl) const {
  if (!pControl || !m_pInterForm)
    return nullptr;

  CPDFSDK_Widget* pWidget = nullptr;
  const auto it = m_Map.find(pControl);
  if (it != m_Map.end())
    pWidget = it->second;
  if (pWidget)
    return pWidget;

  CPDF_Dictionary* pControlDict = pControl->GetWidget();
  CPDF_Document* pDocument = m_pFormFillEnv->GetPDFDocument();
  CPDFSDK_PageView* pPage = nullptr;

  if (CPDF_Dictionary* pPageDict = pControlDict->GetDictFor("P")) {
    int nPageIndex = pDocument->GetPageIndex(pPageDict->GetObjNum());
    if (nPageIndex >= 0)
      pPage = m_pFormFillEnv->GetPageView(nPageIndex);
  }

  if (!pPage) {
    int nPageIndex = GetPageIndexByAnnotDict(pDocument, pControlDict);
    if (nPageIndex >= 0)
      pPage = m_pFormFillEnv->GetPageView(nPageIndex);
  }

  if (!pPage)
    return nullptr;

  return static_cast<CPDFSDK_Widget*>(pPage->GetAnnotByDict(pControlDict));
}

void CPDFSDK_InterForm::GetWidgets(
    const CFX_WideString& sFieldName,
    std::vector<CPDFSDK_Annot::ObservedPtr>* widgets) const {
  for (int i = 0, sz = m_pInterForm->CountFields(sFieldName); i < sz; ++i) {
    CPDF_FormField* pFormField = m_pInterForm->GetField(i, sFieldName);
    ASSERT(pFormField);
    GetWidgets(pFormField, widgets);
  }
}

void CPDFSDK_InterForm::GetWidgets(
    CPDF_FormField* pField,
    std::vector<CPDFSDK_Annot::ObservedPtr>* widgets) const {
  for (int i = 0, sz = pField->CountControls(); i < sz; ++i) {
    CPDF_FormControl* pFormCtrl = pField->GetControl(i);
    ASSERT(pFormCtrl);
    CPDFSDK_Widget* pWidget = GetWidget(pFormCtrl);
    if (pWidget)
      widgets->emplace_back(pWidget);
  }
}

int CPDFSDK_InterForm::GetPageIndexByAnnotDict(
    CPDF_Document* pDocument,
    CPDF_Dictionary* pAnnotDict) const {
  ASSERT(pAnnotDict);

  for (int i = 0, sz = pDocument->GetPageCount(); i < sz; i++) {
    if (CPDF_Dictionary* pPageDict = pDocument->GetPage(i)) {
      if (CPDF_Array* pAnnots = pPageDict->GetArrayFor("Annots")) {
        for (int j = 0, jsz = pAnnots->GetCount(); j < jsz; j++) {
          CPDF_Object* pDict = pAnnots->GetDirectObjectAt(j);
          if (pAnnotDict == pDict)
            return i;
        }
      }
    }
  }

  return -1;
}

void CPDFSDK_InterForm::AddMap(CPDF_FormControl* pControl,
                               CPDFSDK_Widget* pWidget) {
  m_Map[pControl] = pWidget;
}

void CPDFSDK_InterForm::RemoveMap(CPDF_FormControl* pControl) {
  m_Map.erase(pControl);
}

void CPDFSDK_InterForm::EnableCalculate(bool bEnabled) {
  m_bCalculate = bEnabled;
}

bool CPDFSDK_InterForm::IsCalculateEnabled() const {
  return m_bCalculate;
}

#ifdef PDF_ENABLE_XFA
void CPDFSDK_InterForm::AddXFAMap(CXFA_FFWidget* hWidget,
                                  CPDFSDK_XFAWidget* pWidget) {
  ASSERT(hWidget);
  m_XFAMap[hWidget] = pWidget;
}

void CPDFSDK_InterForm::RemoveXFAMap(CXFA_FFWidget* hWidget) {
  ASSERT(hWidget);
  m_XFAMap.erase(hWidget);
}

CPDFSDK_XFAWidget* CPDFSDK_InterForm::GetXFAWidget(CXFA_FFWidget* hWidget) {
  ASSERT(hWidget);
  auto it = m_XFAMap.find(hWidget);
  return it != m_XFAMap.end() ? it->second : nullptr;
}

void CPDFSDK_InterForm::XfaEnableCalculate(bool bEnabled) {
  m_bXfaCalculate = bEnabled;
}
bool CPDFSDK_InterForm::IsXfaCalculateEnabled() const {
  return m_bXfaCalculate;
}

bool CPDFSDK_InterForm::IsXfaValidationsEnabled() {
  return m_bXfaValidationsEnabled;
}
void CPDFSDK_InterForm::XfaSetValidationsEnabled(bool bEnabled) {
  m_bXfaValidationsEnabled = bEnabled;
}

void CPDFSDK_InterForm::SynchronizeField(CPDF_FormField* pFormField,
                                         bool bSynchronizeElse) {
  for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
    CPDF_FormControl* pFormCtrl = pFormField->GetControl(i);
    if (CPDFSDK_Widget* pWidget = GetWidget(pFormCtrl))
      pWidget->Synchronize(bSynchronizeElse);
  }
}
#endif  // PDF_ENABLE_XFA

void CPDFSDK_InterForm::OnCalculate(CPDF_FormField* pFormField) {
  if (!m_pFormFillEnv->IsJSInitiated())
    return;

  if (m_bBusy)
    return;

  m_bBusy = true;

  if (!IsCalculateEnabled()) {
    m_bBusy = false;
    return;
  }

  IJS_Runtime* pRuntime = m_pFormFillEnv->GetJSRuntime();
  int nSize = m_pInterForm->CountFieldsInCalculationOrder();
  for (int i = 0; i < nSize; i++) {
    CPDF_FormField* pField = m_pInterForm->GetFieldInCalculationOrder(i);
    if (!pField)
      continue;

    int nType = pField->GetFieldType();
    if (nType != FIELDTYPE_COMBOBOX && nType != FIELDTYPE_TEXTFIELD)
      continue;

    CPDF_AAction aAction = pField->GetAdditionalAction();
    if (!aAction.GetDict() || !aAction.ActionExist(CPDF_AAction::Calculate))
      continue;

    CPDF_Action action = aAction.GetAction(CPDF_AAction::Calculate);
    if (!action.GetDict())
      continue;

    CFX_WideString csJS = action.GetJavaScript();
    if (csJS.IsEmpty())
      continue;

    IJS_EventContext* pContext = pRuntime->NewEventContext();
    CFX_WideString sOldValue = pField->GetValue();
    CFX_WideString sValue = sOldValue;
    bool bRC = true;
    pContext->OnField_Calculate(pFormField, pField, sValue, bRC);

    CFX_WideString sInfo;
    bool bRet = pContext->RunScript(csJS, &sInfo);
    pRuntime->ReleaseEventContext(pContext);
    if (bRet && bRC && sValue.Compare(sOldValue) != 0)
      pField->SetValue(sValue, true);
  }
  m_bBusy = false;
}

CFX_WideString CPDFSDK_InterForm::OnFormat(CPDF_FormField* pFormField,
                                           bool& bFormatted) {
  CFX_WideString sValue = pFormField->GetValue();
  if (!m_pFormFillEnv->IsJSInitiated()) {
    bFormatted = false;
    return sValue;
  }

  IJS_Runtime* pRuntime = m_pFormFillEnv->GetJSRuntime();
  if (pFormField->GetFieldType() == FIELDTYPE_COMBOBOX &&
      pFormField->CountSelectedItems() > 0) {
    int index = pFormField->GetSelectedIndex(0);
    if (index >= 0)
      sValue = pFormField->GetOptionLabel(index);
  }

  bFormatted = false;

  CPDF_AAction aAction = pFormField->GetAdditionalAction();
  if (aAction.GetDict() && aAction.ActionExist(CPDF_AAction::Format)) {
    CPDF_Action action = aAction.GetAction(CPDF_AAction::Format);
    if (action.GetDict()) {
      CFX_WideString script = action.GetJavaScript();
      if (!script.IsEmpty()) {
        CFX_WideString Value = sValue;

        IJS_EventContext* pContext = pRuntime->NewEventContext();
        pContext->OnField_Format(pFormField, Value, true);
        CFX_WideString sInfo;
        bool bRet = pContext->RunScript(script, &sInfo);
        pRuntime->ReleaseEventContext(pContext);
        if (bRet) {
          sValue = Value;
          bFormatted = true;
        }
      }
    }
  }
  return sValue;
}

void CPDFSDK_InterForm::ResetFieldAppearance(CPDF_FormField* pFormField,
                                             const CFX_WideString* sValue,
                                             bool bValueChanged) {
  for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
    CPDF_FormControl* pFormCtrl = pFormField->GetControl(i);
    ASSERT(pFormCtrl);
    if (CPDFSDK_Widget* pWidget = GetWidget(pFormCtrl))
      pWidget->ResetAppearance(sValue, bValueChanged);
  }
}

void CPDFSDK_InterForm::UpdateField(CPDF_FormField* pFormField) {
  auto formfiller = m_pFormFillEnv->GetInteractiveFormFiller();
  for (int i = 0, sz = pFormField->CountControls(); i < sz; i++) {
    CPDF_FormControl* pFormCtrl = pFormField->GetControl(i);
    ASSERT(pFormCtrl);

    if (CPDFSDK_Widget* pWidget = GetWidget(pFormCtrl)) {
      UnderlyingPageType* pPage = pWidget->GetUnderlyingPage();
      m_pFormFillEnv->Invalidate(
          pPage, formfiller->GetViewBBox(
                     m_pFormFillEnv->GetPageView(pPage, false), pWidget));
    }
  }
}

bool CPDFSDK_InterForm::OnKeyStrokeCommit(CPDF_FormField* pFormField,
                                          const CFX_WideString& csValue) {
  CPDF_AAction aAction = pFormField->GetAdditionalAction();
  if (!aAction.GetDict() || !aAction.ActionExist(CPDF_AAction::KeyStroke))
    return true;

  CPDF_Action action = aAction.GetAction(CPDF_AAction::KeyStroke);
  if (!action.GetDict())
    return true;

  CPDFSDK_ActionHandler* pActionHandler = m_pFormFillEnv->GetActionHander();
  PDFSDK_FieldAction fa;
  fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(0);
  fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(0);
  fa.sValue = csValue;
  pActionHandler->DoAction_FieldJavaScript(action, CPDF_AAction::KeyStroke,
                                           m_pFormFillEnv, pFormField, fa);
  return fa.bRC;
}

bool CPDFSDK_InterForm::OnValidate(CPDF_FormField* pFormField,
                                   const CFX_WideString& csValue) {
  CPDF_AAction aAction = pFormField->GetAdditionalAction();
  if (!aAction.GetDict() || !aAction.ActionExist(CPDF_AAction::Validate))
    return true;

  CPDF_Action action = aAction.GetAction(CPDF_AAction::Validate);
  if (!action.GetDict())
    return true;

  CPDFSDK_ActionHandler* pActionHandler = m_pFormFillEnv->GetActionHander();
  PDFSDK_FieldAction fa;
  fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(0);
  fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(0);
  fa.sValue = csValue;
  pActionHandler->DoAction_FieldJavaScript(action, CPDF_AAction::Validate,
                                           m_pFormFillEnv, pFormField, fa);
  return fa.bRC;
}

bool CPDFSDK_InterForm::DoAction_Hide(const CPDF_Action& action) {
  ASSERT(action.GetDict());

  CPDF_ActionFields af(&action);
  std::vector<CPDF_Object*> fieldObjects = af.GetAllFields();
  std::vector<CPDF_FormField*> fields = GetFieldFromObjects(fieldObjects);

  bool bHide = action.GetHideStatus();
  bool bChanged = false;

  for (CPDF_FormField* pField : fields) {
    for (int i = 0, sz = pField->CountControls(); i < sz; ++i) {
      CPDF_FormControl* pControl = pField->GetControl(i);
      ASSERT(pControl);

      if (CPDFSDK_Widget* pWidget = GetWidget(pControl)) {
        uint32_t nFlags = pWidget->GetFlags();
        nFlags &= ~ANNOTFLAG_INVISIBLE;
        nFlags &= ~ANNOTFLAG_NOVIEW;
        if (bHide)
          nFlags |= ANNOTFLAG_HIDDEN;
        else
          nFlags &= ~ANNOTFLAG_HIDDEN;
        pWidget->SetFlags(nFlags);
        pWidget->GetPageView()->UpdateView(pWidget);
        bChanged = true;
      }
    }
  }

  return bChanged;
}

bool CPDFSDK_InterForm::DoAction_SubmitForm(const CPDF_Action& action) {
  CFX_WideString sDestination = action.GetFilePath();
  if (sDestination.IsEmpty())
    return false;

  CPDF_Dictionary* pActionDict = action.GetDict();
  if (pActionDict->KeyExist("Fields")) {
    CPDF_ActionFields af(&action);
    uint32_t dwFlags = action.GetFlags();
    std::vector<CPDF_Object*> fieldObjects = af.GetAllFields();
    std::vector<CPDF_FormField*> fields = GetFieldFromObjects(fieldObjects);
    if (!fields.empty()) {
      bool bIncludeOrExclude = !(dwFlags & 0x01);
      if (!m_pInterForm->CheckRequiredFields(&fields, bIncludeOrExclude))
        return false;

      return SubmitFields(sDestination, fields, bIncludeOrExclude, false);
    }
  }
  if (!m_pInterForm->CheckRequiredFields(nullptr, true))
    return false;

  return SubmitForm(sDestination, false);
}

bool CPDFSDK_InterForm::SubmitFields(const CFX_WideString& csDestination,
                                     const std::vector<CPDF_FormField*>& fields,
                                     bool bIncludeOrExclude,
                                     bool bUrlEncoded) {
  CFX_ByteTextBuf textBuf;
  ExportFieldsToFDFTextBuf(fields, bIncludeOrExclude, textBuf);

  uint8_t* pBuffer = textBuf.GetBuffer();
  FX_STRSIZE nBufSize = textBuf.GetLength();

  if (bUrlEncoded && !FDFToURLEncodedData(pBuffer, nBufSize))
    return false;

  m_pFormFillEnv->JS_docSubmitForm(pBuffer, nBufSize, csDestination.c_str());
  return true;
}

bool CPDFSDK_InterForm::FDFToURLEncodedData(CFX_WideString csFDFFile,
                                            CFX_WideString csTxtFile) {
  return true;
}

bool CPDFSDK_InterForm::FDFToURLEncodedData(uint8_t*& pBuf,
                                            FX_STRSIZE& nBufSize) {
  std::unique_ptr<CFDF_Document> pFDF =
      CFDF_Document::ParseMemory(pBuf, nBufSize);
  if (!pFDF)
    return true;

  CPDF_Dictionary* pMainDict = pFDF->GetRoot()->GetDictFor("FDF");
  if (!pMainDict)
    return false;

  CPDF_Array* pFields = pMainDict->GetArrayFor("Fields");
  if (!pFields)
    return false;

  CFX_ByteTextBuf fdfEncodedData;
  for (uint32_t i = 0; i < pFields->GetCount(); i++) {
    CPDF_Dictionary* pField = pFields->GetDictAt(i);
    if (!pField)
      continue;
    CFX_WideString name;
    name = pField->GetUnicodeTextFor("T");
    CFX_ByteString name_b = CFX_ByteString::FromUnicode(name);
    CFX_ByteString csBValue = pField->GetStringFor("V");
    CFX_WideString csWValue = PDF_DecodeText(csBValue);
    CFX_ByteString csValue_b = CFX_ByteString::FromUnicode(csWValue);

    fdfEncodedData << name_b.GetBuffer(name_b.GetLength());
    name_b.ReleaseBuffer();
    fdfEncodedData << "=";
    fdfEncodedData << csValue_b.GetBuffer(csValue_b.GetLength());
    csValue_b.ReleaseBuffer();
    if (i != pFields->GetCount() - 1)
      fdfEncodedData << "&";
  }

  nBufSize = fdfEncodedData.GetLength();
  pBuf = FX_Alloc(uint8_t, nBufSize);
  FXSYS_memcpy(pBuf, fdfEncodedData.GetBuffer(), nBufSize);
  return true;
}

bool CPDFSDK_InterForm::ExportFieldsToFDFTextBuf(
    const std::vector<CPDF_FormField*>& fields,
    bool bIncludeOrExclude,
    CFX_ByteTextBuf& textBuf) {
  std::unique_ptr<CFDF_Document> pFDF =
      m_pInterForm->ExportToFDF(m_pFormFillEnv->JS_docGetFilePath().AsStringC(),
                                fields, bIncludeOrExclude, false);
  return pFDF ? pFDF->WriteBuf(textBuf) : false;
}

CFX_WideString CPDFSDK_InterForm::GetTemporaryFileName(
    const CFX_WideString& sFileExt) {
  return L"";
}

bool CPDFSDK_InterForm::SubmitForm(const CFX_WideString& sDestination,
                                   bool bUrlEncoded) {
  if (sDestination.IsEmpty())
    return false;

  if (!m_pFormFillEnv || !m_pInterForm)
    return false;

  std::unique_ptr<CFDF_Document> pFDFDoc = m_pInterForm->ExportToFDF(
      m_pFormFillEnv->JS_docGetFilePath().AsStringC(), false);
  if (!pFDFDoc)
    return false;

  CFX_ByteTextBuf FdfBuffer;
  if (!pFDFDoc->WriteBuf(FdfBuffer))
    return false;

  uint8_t* pBuffer = FdfBuffer.GetBuffer();
  FX_STRSIZE nBufSize = FdfBuffer.GetLength();
  if (bUrlEncoded && !FDFToURLEncodedData(pBuffer, nBufSize))
    return false;

  m_pFormFillEnv->JS_docSubmitForm(pBuffer, nBufSize, sDestination.c_str());
  if (bUrlEncoded)
    FX_Free(pBuffer);

  return true;
}

bool CPDFSDK_InterForm::ExportFormToFDFTextBuf(CFX_ByteTextBuf& textBuf) {
  std::unique_ptr<CFDF_Document> pFDF = m_pInterForm->ExportToFDF(
      m_pFormFillEnv->JS_docGetFilePath().AsStringC(), false);
  return pFDF && pFDF->WriteBuf(textBuf);
}

bool CPDFSDK_InterForm::DoAction_ResetForm(const CPDF_Action& action) {
  ASSERT(action.GetDict());

  CPDF_Dictionary* pActionDict = action.GetDict();
  if (!pActionDict->KeyExist("Fields"))
    return m_pInterForm->ResetForm(true);

  CPDF_ActionFields af(&action);
  uint32_t dwFlags = action.GetFlags();

  std::vector<CPDF_Object*> fieldObjects = af.GetAllFields();
  std::vector<CPDF_FormField*> fields = GetFieldFromObjects(fieldObjects);
  return m_pInterForm->ResetForm(fields, !(dwFlags & 0x01), true);
}

bool CPDFSDK_InterForm::DoAction_ImportData(const CPDF_Action& action) {
  return false;
}

std::vector<CPDF_FormField*> CPDFSDK_InterForm::GetFieldFromObjects(
    const std::vector<CPDF_Object*>& objects) const {
  std::vector<CPDF_FormField*> fields;
  for (CPDF_Object* pObject : objects) {
    if (pObject && pObject->IsString()) {
      CFX_WideString csName = pObject->GetUnicodeText();
      CPDF_FormField* pField = m_pInterForm->GetField(0, csName);
      if (pField)
        fields.push_back(pField);
    }
  }
  return fields;
}

int CPDFSDK_InterForm::BeforeValueChange(CPDF_FormField* pField,
                                         const CFX_WideString& csValue) {
  int nType = pField->GetFieldType();
  if (nType != FIELDTYPE_COMBOBOX && nType != FIELDTYPE_TEXTFIELD)
    return 0;

  if (!OnKeyStrokeCommit(pField, csValue))
    return -1;

  if (!OnValidate(pField, csValue))
    return -1;

  return 1;
}

void CPDFSDK_InterForm::AfterValueChange(CPDF_FormField* pField) {
#ifdef PDF_ENABLE_XFA
  SynchronizeField(pField, false);
#endif  // PDF_ENABLE_XFA
  int nType = pField->GetFieldType();
  if (nType == FIELDTYPE_COMBOBOX || nType == FIELDTYPE_TEXTFIELD) {
    OnCalculate(pField);
    bool bFormatted = false;
    CFX_WideString sValue = OnFormat(pField, bFormatted);
    ResetFieldAppearance(pField, bFormatted ? &sValue : nullptr, true);
    UpdateField(pField);
  }
}

int CPDFSDK_InterForm::BeforeSelectionChange(CPDF_FormField* pField,
                                             const CFX_WideString& csValue) {
  if (pField->GetFieldType() != FIELDTYPE_LISTBOX)
    return 0;

  if (!OnKeyStrokeCommit(pField, csValue))
    return -1;

  if (!OnValidate(pField, csValue))
    return -1;

  return 1;
}

void CPDFSDK_InterForm::AfterSelectionChange(CPDF_FormField* pField) {
  if (pField->GetFieldType() != FIELDTYPE_LISTBOX)
    return;

  OnCalculate(pField);
  ResetFieldAppearance(pField, nullptr, true);
  UpdateField(pField);
}

void CPDFSDK_InterForm::AfterCheckedStatusChange(CPDF_FormField* pField) {
  int nType = pField->GetFieldType();
  if (nType != FIELDTYPE_CHECKBOX && nType != FIELDTYPE_RADIOBUTTON)
    return;

  OnCalculate(pField);
  UpdateField(pField);
}

int CPDFSDK_InterForm::BeforeFormReset(CPDF_InterForm* pForm) {
  return 0;
}

void CPDFSDK_InterForm::AfterFormReset(CPDF_InterForm* pForm) {
  OnCalculate(nullptr);
}

int CPDFSDK_InterForm::BeforeFormImportData(CPDF_InterForm* pForm) {
  return 0;
}

void CPDFSDK_InterForm::AfterFormImportData(CPDF_InterForm* pForm) {
  OnCalculate(nullptr);
}

bool CPDFSDK_InterForm::IsNeedHighLight(int nFieldType) {
  if (nFieldType < 1 || nFieldType > kNumFieldTypes)
    return false;
  return m_bNeedHightlight[nFieldType - 1];
}

void CPDFSDK_InterForm::RemoveAllHighLight() {
  for (int i = 0; i < kNumFieldTypes; ++i)
    m_bNeedHightlight[i] = false;
}

void CPDFSDK_InterForm::SetHighlightColor(FX_COLORREF clr, int nFieldType) {
  if (nFieldType < 0 || nFieldType > kNumFieldTypes)
    return;
  switch (nFieldType) {
    case 0: {
      for (int i = 0; i < kNumFieldTypes; ++i) {
        m_aHighlightColor[i] = clr;
        m_bNeedHightlight[i] = true;
      }
      break;
    }
    default: {
      m_aHighlightColor[nFieldType - 1] = clr;
      m_bNeedHightlight[nFieldType - 1] = true;
      break;
    }
  }
}

FX_COLORREF CPDFSDK_InterForm::GetHighlightColor(int nFieldType) {
  if (nFieldType < 0 || nFieldType > kNumFieldTypes)
    return FXSYS_RGB(255, 255, 255);
  if (nFieldType == 0)
    return m_aHighlightColor[0];
  return m_aHighlightColor[nFieldType - 1];
}
