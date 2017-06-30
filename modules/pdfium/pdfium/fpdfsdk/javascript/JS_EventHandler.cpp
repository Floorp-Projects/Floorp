// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_EventHandler.h"

#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/Field.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_event_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"

CJS_EventHandler::CJS_EventHandler(CJS_EventContext* pContext)
    : m_pJSEventContext(pContext),
      m_eEventType(JET_UNKNOWN),
      m_bValid(false),
      m_pWideStrChange(nullptr),
      m_nCommitKey(-1),
      m_bKeyDown(false),
      m_bModifier(false),
      m_bShift(false),
      m_pISelEnd(nullptr),
      m_nSelEndDu(0),
      m_pISelStart(nullptr),
      m_nSelStartDu(0),
      m_bWillCommit(false),
      m_pValue(nullptr),
      m_bFieldFull(false),
      m_pbRc(nullptr),
      m_bRcDu(false),
      m_pTargetBookMark(nullptr),
      m_pTargetFormFillEnv(nullptr),
      m_pTargetAnnot(nullptr) {}

CJS_EventHandler::~CJS_EventHandler() {}

void CJS_EventHandler::OnApp_Init() {
  Initial(JET_APP_INIT);
}

void CJS_EventHandler::OnDoc_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                  const CFX_WideString& strTargetName) {
  Initial(JET_DOC_OPEN);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
  m_strTargetName = strTargetName;
}

void CJS_EventHandler::OnDoc_WillPrint(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_DOC_WILLPRINT);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnDoc_DidPrint(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_DOC_DIDPRINT);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnDoc_WillSave(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_DOC_WILLSAVE);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnDoc_DidSave(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_DOC_DIDSAVE);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnDoc_WillClose(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_DOC_WILLCLOSE);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnPage_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_PAGE_OPEN);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnPage_Close(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_PAGE_CLOSE);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnPage_InView(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_PAGE_INVIEW);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnPage_OutView(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  Initial(JET_PAGE_OUTVIEW);
  m_pTargetFormFillEnv.Reset(pFormFillEnv);
}

void CJS_EventHandler::OnField_MouseEnter(bool bModifier,
                                          bool bShift,
                                          CPDF_FormField* pTarget) {
  Initial(JET_FIELD_MOUSEENTER);

  m_bModifier = bModifier;
  m_bShift = bShift;

  m_strTargetName = pTarget->GetFullName();
}

void CJS_EventHandler::OnField_MouseExit(bool bModifier,
                                         bool bShift,
                                         CPDF_FormField* pTarget) {
  Initial(JET_FIELD_MOUSEEXIT);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
}

void CJS_EventHandler::OnField_MouseDown(bool bModifier,
                                         bool bShift,
                                         CPDF_FormField* pTarget) {
  Initial(JET_FIELD_MOUSEDOWN);
  m_eEventType = JET_FIELD_MOUSEDOWN;

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
}

void CJS_EventHandler::OnField_MouseUp(bool bModifier,
                                       bool bShift,
                                       CPDF_FormField* pTarget) {
  Initial(JET_FIELD_MOUSEUP);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
}

void CJS_EventHandler::OnField_Focus(bool bModifier,
                                     bool bShift,
                                     CPDF_FormField* pTarget,
                                     const CFX_WideString& Value) {
  Initial(JET_FIELD_FOCUS);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
  m_pValue = (CFX_WideString*)&Value;
}

void CJS_EventHandler::OnField_Blur(bool bModifier,
                                    bool bShift,
                                    CPDF_FormField* pTarget,
                                    const CFX_WideString& Value) {
  Initial(JET_FIELD_BLUR);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
  m_pValue = (CFX_WideString*)&Value;
}

void CJS_EventHandler::OnField_Keystroke(CFX_WideString& strChange,
                                         const CFX_WideString& strChangeEx,
                                         bool KeyDown,
                                         bool bModifier,
                                         int& nSelEnd,
                                         int& nSelStart,
                                         bool bShift,
                                         CPDF_FormField* pTarget,
                                         CFX_WideString& Value,
                                         bool bWillCommit,
                                         bool bFieldFull,
                                         bool& bRc) {
  Initial(JET_FIELD_KEYSTROKE);

  m_nCommitKey = 0;
  m_pWideStrChange = &strChange;
  m_WideStrChangeEx = strChangeEx;
  m_bKeyDown = KeyDown;
  m_bModifier = bModifier;
  m_pISelEnd = &nSelEnd;
  m_pISelStart = &nSelStart;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
  m_pValue = &Value;
  m_bWillCommit = bWillCommit;
  m_pbRc = &bRc;
  m_bFieldFull = bFieldFull;
}

void CJS_EventHandler::OnField_Validate(CFX_WideString& strChange,
                                        const CFX_WideString& strChangeEx,
                                        bool bKeyDown,
                                        bool bModifier,
                                        bool bShift,
                                        CPDF_FormField* pTarget,
                                        CFX_WideString& Value,
                                        bool& bRc) {
  Initial(JET_FIELD_VALIDATE);

  m_pWideStrChange = &strChange;
  m_WideStrChangeEx = strChangeEx;
  m_bKeyDown = bKeyDown;
  m_bModifier = bModifier;
  m_bShift = bShift;
  m_strTargetName = pTarget->GetFullName();
  m_pValue = &Value;
  m_pbRc = &bRc;
}

void CJS_EventHandler::OnField_Calculate(CPDF_FormField* pSource,
                                         CPDF_FormField* pTarget,
                                         CFX_WideString& Value,
                                         bool& bRc) {
  Initial(JET_FIELD_CALCULATE);

  if (pSource)
    m_strSourceName = pSource->GetFullName();
  m_strTargetName = pTarget->GetFullName();
  m_pValue = &Value;
  m_pbRc = &bRc;
}

void CJS_EventHandler::OnField_Format(CPDF_FormField* pTarget,
                                      CFX_WideString& Value,
                                      bool bWillCommit) {
  Initial(JET_FIELD_FORMAT);

  m_nCommitKey = 0;
  m_strTargetName = pTarget->GetFullName();
  m_pValue = &Value;
  m_bWillCommit = bWillCommit;
}

void CJS_EventHandler::OnScreen_Focus(bool bModifier,
                                      bool bShift,
                                      CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_FOCUS);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_Blur(bool bModifier,
                                     bool bShift,
                                     CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_BLUR);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_Open(bool bModifier,
                                     bool bShift,
                                     CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_OPEN);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_Close(bool bModifier,
                                      bool bShift,
                                      CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_CLOSE);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_MouseDown(bool bModifier,
                                          bool bShift,
                                          CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_MOUSEDOWN);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_MouseUp(bool bModifier,
                                        bool bShift,
                                        CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_MOUSEUP);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_MouseEnter(bool bModifier,
                                           bool bShift,
                                           CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_MOUSEENTER);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_MouseExit(bool bModifier,
                                          bool bShift,
                                          CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_MOUSEEXIT);

  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_InView(bool bModifier,
                                       bool bShift,
                                       CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_INVIEW);
  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnScreen_OutView(bool bModifier,
                                        bool bShift,
                                        CPDFSDK_Annot* pScreen) {
  Initial(JET_SCREEN_OUTVIEW);
  m_bModifier = bModifier;
  m_bShift = bShift;
  m_pTargetAnnot.Reset(pScreen);
}

void CJS_EventHandler::OnLink_MouseUp(
    CPDFSDK_FormFillEnvironment* pTargetFormFillEnv) {
  Initial(JET_LINK_MOUSEUP);
  m_pTargetFormFillEnv.Reset(pTargetFormFillEnv);
}

void CJS_EventHandler::OnBookmark_MouseUp(CPDF_Bookmark* pBookMark) {
  Initial(JET_BOOKMARK_MOUSEUP);
  m_pTargetBookMark = pBookMark;
}

void CJS_EventHandler::OnMenu_Exec(
    CPDFSDK_FormFillEnvironment* pTargetFormFillEnv,
    const CFX_WideString& strTargetName) {
  Initial(JET_MENU_EXEC);
  m_pTargetFormFillEnv.Reset(pTargetFormFillEnv);
  m_strTargetName = strTargetName;
}

void CJS_EventHandler::OnExternal_Exec() {
  Initial(JET_EXTERNAL_EXEC);
}

void CJS_EventHandler::OnBatchExec(
    CPDFSDK_FormFillEnvironment* pTargetFormFillEnv) {
  Initial(JET_BATCH_EXEC);
  m_pTargetFormFillEnv.Reset(pTargetFormFillEnv);
}

void CJS_EventHandler::OnConsole_Exec() {
  Initial(JET_CONSOLE_EXEC);
}

void CJS_EventHandler::Initial(JS_EVENT_T type) {
  m_eEventType = type;

  m_strTargetName = L"";
  m_strSourceName = L"";
  m_pWideStrChange = nullptr;
  m_WideStrChangeDu = L"";
  m_WideStrChangeEx = L"";
  m_nCommitKey = -1;
  m_bKeyDown = false;
  m_bModifier = false;
  m_bShift = false;
  m_pISelEnd = nullptr;
  m_nSelEndDu = 0;
  m_pISelStart = nullptr;
  m_nSelStartDu = 0;
  m_bWillCommit = false;
  m_pValue = nullptr;
  m_bFieldFull = false;
  m_pbRc = nullptr;
  m_bRcDu = false;

  m_pTargetBookMark = nullptr;
  m_pTargetFormFillEnv.Reset();
  m_pTargetAnnot.Reset();

  m_bValid = true;
}

void CJS_EventHandler::Destroy() {
  m_bValid = false;
}

bool CJS_EventHandler::IsValid() {
  return m_bValid;
}

CFX_WideString& CJS_EventHandler::Change() {
  if (m_pWideStrChange) {
    return *m_pWideStrChange;
  }
  return m_WideStrChangeDu;
}

CFX_WideString CJS_EventHandler::ChangeEx() {
  return m_WideStrChangeEx;
}

int CJS_EventHandler::CommitKey() {
  return m_nCommitKey;
}

bool CJS_EventHandler::FieldFull() {
  return m_bFieldFull;
}

bool CJS_EventHandler::KeyDown() {
  return m_bKeyDown;
}

bool CJS_EventHandler::Modifier() {
  return m_bModifier;
}

const FX_WCHAR* CJS_EventHandler::Name() {
  switch (m_eEventType) {
    case JET_APP_INIT:
      return L"Init";
    case JET_BATCH_EXEC:
      return L"Exec";
    case JET_BOOKMARK_MOUSEUP:
      return L"Mouse Up";
    case JET_CONSOLE_EXEC:
      return L"Exec";
    case JET_DOC_DIDPRINT:
      return L"DidPrint";
    case JET_DOC_DIDSAVE:
      return L"DidSave";
    case JET_DOC_OPEN:
      return L"Open";
    case JET_DOC_WILLCLOSE:
      return L"WillClose";
    case JET_DOC_WILLPRINT:
      return L"WillPrint";
    case JET_DOC_WILLSAVE:
      return L"WillSave";
    case JET_EXTERNAL_EXEC:
      return L"Exec";
    case JET_FIELD_FOCUS:
    case JET_SCREEN_FOCUS:
      return L"Focus";
    case JET_FIELD_BLUR:
    case JET_SCREEN_BLUR:
      return L"Blur";
    case JET_FIELD_MOUSEDOWN:
    case JET_SCREEN_MOUSEDOWN:
      return L"Mouse Down";
    case JET_FIELD_MOUSEUP:
    case JET_SCREEN_MOUSEUP:
      return L"Mouse Up";
    case JET_FIELD_MOUSEENTER:
    case JET_SCREEN_MOUSEENTER:
      return L"Mouse Enter";
    case JET_FIELD_MOUSEEXIT:
    case JET_SCREEN_MOUSEEXIT:
      return L"Mouse Exit";
    case JET_FIELD_CALCULATE:
      return L"Calculate";
    case JET_FIELD_FORMAT:
      return L"Format";
    case JET_FIELD_KEYSTROKE:
      return L"Keystroke";
    case JET_FIELD_VALIDATE:
      return L"Validate";
    case JET_LINK_MOUSEUP:
      return L"Mouse Up";
    case JET_MENU_EXEC:
      return L"Exec";
    case JET_PAGE_OPEN:
    case JET_SCREEN_OPEN:
      return L"Open";
    case JET_PAGE_CLOSE:
    case JET_SCREEN_CLOSE:
      return L"Close";
    case JET_SCREEN_INVIEW:
    case JET_PAGE_INVIEW:
      return L"InView";
    case JET_PAGE_OUTVIEW:
    case JET_SCREEN_OUTVIEW:
      return L"OutView";
    default:
      return L"";
  }
}

const FX_WCHAR* CJS_EventHandler::Type() {
  switch (m_eEventType) {
    case JET_APP_INIT:
      return L"App";
    case JET_BATCH_EXEC:
      return L"Batch";
    case JET_BOOKMARK_MOUSEUP:
      return L"BookMark";
    case JET_CONSOLE_EXEC:
      return L"Console";
    case JET_DOC_DIDPRINT:
    case JET_DOC_DIDSAVE:
    case JET_DOC_OPEN:
    case JET_DOC_WILLCLOSE:
    case JET_DOC_WILLPRINT:
    case JET_DOC_WILLSAVE:
      return L"Doc";
    case JET_EXTERNAL_EXEC:
      return L"External";
    case JET_FIELD_BLUR:
    case JET_FIELD_FOCUS:
    case JET_FIELD_MOUSEDOWN:
    case JET_FIELD_MOUSEENTER:
    case JET_FIELD_MOUSEEXIT:
    case JET_FIELD_MOUSEUP:
    case JET_FIELD_CALCULATE:
    case JET_FIELD_FORMAT:
    case JET_FIELD_KEYSTROKE:
    case JET_FIELD_VALIDATE:
      return L"Field";
    case JET_SCREEN_FOCUS:
    case JET_SCREEN_BLUR:
    case JET_SCREEN_OPEN:
    case JET_SCREEN_CLOSE:
    case JET_SCREEN_MOUSEDOWN:
    case JET_SCREEN_MOUSEUP:
    case JET_SCREEN_MOUSEENTER:
    case JET_SCREEN_MOUSEEXIT:
    case JET_SCREEN_INVIEW:
    case JET_SCREEN_OUTVIEW:
      return L"Screen";
    case JET_LINK_MOUSEUP:
      return L"Link";
    case JET_MENU_EXEC:
      return L"Menu";
    case JET_PAGE_OPEN:
    case JET_PAGE_CLOSE:
    case JET_PAGE_INVIEW:
    case JET_PAGE_OUTVIEW:
      return L"Page";
    default:
      return L"";
  }
}

bool& CJS_EventHandler::Rc() {
  if (m_pbRc) {
    return *m_pbRc;
  }
  return m_bRcDu;
}

int& CJS_EventHandler::SelEnd() {
  if (m_pISelEnd) {
    return *m_pISelEnd;
  }
  return m_nSelEndDu;
}

int& CJS_EventHandler::SelStart() {
  if (m_pISelStart) {
    return *m_pISelStart;
  }
  return m_nSelStartDu;
}

bool CJS_EventHandler::Shift() {
  return m_bShift;
}

Field* CJS_EventHandler::Source() {
  CJS_Runtime* pRuntime = m_pJSEventContext->GetJSRuntime();
  v8::Local<v8::Object> pDocObj =
      pRuntime->NewFxDynamicObj(CJS_Document::g_nObjDefnID);
  if (pDocObj.IsEmpty())
    return nullptr;

  v8::Local<v8::Object> pFieldObj =
      pRuntime->NewFxDynamicObj(CJS_Field::g_nObjDefnID);
  if (pFieldObj.IsEmpty())
    return nullptr;

  CJS_Document* pJSDocument =
      static_cast<CJS_Document*>(pRuntime->GetObjectPrivate(pDocObj));
  CJS_Field* pJSField =
      static_cast<CJS_Field*>(pRuntime->GetObjectPrivate(pFieldObj));

  Document* pDocument = static_cast<Document*>(pJSDocument->GetEmbedObject());
  pDocument->SetFormFillEnv(m_pTargetFormFillEnv
                                ? m_pTargetFormFillEnv.Get()
                                : m_pJSEventContext->GetFormFillEnv());

  Field* pField = static_cast<Field*>(pJSField->GetEmbedObject());
  pField->AttachField(pDocument, m_strSourceName);
  return pField;
}

Field* CJS_EventHandler::Target_Field() {
  CJS_Runtime* pRuntime = m_pJSEventContext->GetJSRuntime();
  v8::Local<v8::Object> pDocObj =
      pRuntime->NewFxDynamicObj(CJS_Document::g_nObjDefnID);
  if (pDocObj.IsEmpty())
    return nullptr;

  v8::Local<v8::Object> pFieldObj =
      pRuntime->NewFxDynamicObj(CJS_Field::g_nObjDefnID);
  if (pFieldObj.IsEmpty())
    return nullptr;

  CJS_Document* pJSDocument =
      static_cast<CJS_Document*>(pRuntime->GetObjectPrivate(pDocObj));
  CJS_Field* pJSField =
      static_cast<CJS_Field*>(pRuntime->GetObjectPrivate(pFieldObj));

  Document* pDocument = static_cast<Document*>(pJSDocument->GetEmbedObject());
  pDocument->SetFormFillEnv(m_pTargetFormFillEnv
                                ? m_pTargetFormFillEnv.Get()
                                : m_pJSEventContext->GetFormFillEnv());

  Field* pField = static_cast<Field*>(pJSField->GetEmbedObject());
  pField->AttachField(pDocument, m_strTargetName);
  return pField;
}

CFX_WideString& CJS_EventHandler::Value() {
  return *m_pValue;
}

bool CJS_EventHandler::WillCommit() {
  return m_bWillCommit;
}

CFX_WideString CJS_EventHandler::TargetName() {
  return m_strTargetName;
}
