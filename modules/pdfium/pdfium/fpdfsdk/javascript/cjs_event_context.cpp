// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/cjs_event_context.h"

#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/resource.h"

CJS_EventContext::CJS_EventContext(CJS_Runtime* pRuntime)
    : m_pRuntime(pRuntime),
      m_pEventHandler(new CJS_EventHandler(this)),
      m_bBusy(false) {
  ASSERT(pRuntime);
}

CJS_EventContext::~CJS_EventContext() {}

CPDFSDK_FormFillEnvironment* CJS_EventContext::GetFormFillEnv() {
  return m_pRuntime->GetFormFillEnv();
}

bool CJS_EventContext::RunScript(const CFX_WideString& script,
                                 CFX_WideString* info) {
  v8::Isolate::Scope isolate_scope(m_pRuntime->GetIsolate());
  v8::HandleScope handle_scope(m_pRuntime->GetIsolate());
  v8::Local<v8::Context> context = m_pRuntime->NewLocalContext();
  v8::Context::Scope context_scope(context);

  if (m_bBusy) {
    *info = JSGetStringFromID(IDS_STRING_JSBUSY);
    return false;
  }

  CFX_AutoRestorer<bool> restorer(&m_bBusy);
  m_bBusy = true;

  ASSERT(m_pEventHandler->IsValid());
  CJS_Runtime::FieldEvent event(m_pEventHandler->TargetName(),
                                m_pEventHandler->EventType());
  if (!m_pRuntime->AddEventToSet(event)) {
    *info = JSGetStringFromID(IDS_STRING_JSEVENT);
    return false;
  }

  CFX_WideString sErrorMessage;
  int nRet = 0;
  if (script.GetLength() > 0)
    nRet = m_pRuntime->ExecuteScript(script.c_str(), &sErrorMessage);

  if (nRet < 0)
    *info += sErrorMessage;
  else
    *info = JSGetStringFromID(IDS_STRING_RUN);

  m_pRuntime->RemoveEventFromSet(event);
  m_pEventHandler->Destroy();
  return nRet >= 0;
}

void CJS_EventContext::OnApp_Init() {
  m_pEventHandler->OnApp_Init();
}

void CJS_EventContext::OnDoc_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                  const CFX_WideString& strTargetName) {
  m_pEventHandler->OnDoc_Open(pFormFillEnv, strTargetName);
}

void CJS_EventContext::OnDoc_WillPrint(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnDoc_WillPrint(pFormFillEnv);
}

void CJS_EventContext::OnDoc_DidPrint(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnDoc_DidPrint(pFormFillEnv);
}

void CJS_EventContext::OnDoc_WillSave(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnDoc_WillSave(pFormFillEnv);
}

void CJS_EventContext::OnDoc_DidSave(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnDoc_DidSave(pFormFillEnv);
}

void CJS_EventContext::OnDoc_WillClose(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnDoc_WillClose(pFormFillEnv);
}

void CJS_EventContext::OnPage_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnPage_Open(pFormFillEnv);
}

void CJS_EventContext::OnPage_Close(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnPage_Close(pFormFillEnv);
}

void CJS_EventContext::OnPage_InView(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnPage_InView(pFormFillEnv);
}

void CJS_EventContext::OnPage_OutView(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnPage_OutView(pFormFillEnv);
}

void CJS_EventContext::OnField_MouseDown(bool bModifier,
                                         bool bShift,
                                         CPDF_FormField* pTarget) {
  m_pEventHandler->OnField_MouseDown(bModifier, bShift, pTarget);
}

void CJS_EventContext::OnField_MouseEnter(bool bModifier,
                                          bool bShift,
                                          CPDF_FormField* pTarget) {
  m_pEventHandler->OnField_MouseEnter(bModifier, bShift, pTarget);
}

void CJS_EventContext::OnField_MouseExit(bool bModifier,
                                         bool bShift,
                                         CPDF_FormField* pTarget) {
  m_pEventHandler->OnField_MouseExit(bModifier, bShift, pTarget);
}

void CJS_EventContext::OnField_MouseUp(bool bModifier,
                                       bool bShift,
                                       CPDF_FormField* pTarget) {
  m_pEventHandler->OnField_MouseUp(bModifier, bShift, pTarget);
}

void CJS_EventContext::OnField_Focus(bool bModifier,
                                     bool bShift,
                                     CPDF_FormField* pTarget,
                                     const CFX_WideString& Value) {
  m_pEventHandler->OnField_Focus(bModifier, bShift, pTarget, Value);
}

void CJS_EventContext::OnField_Blur(bool bModifier,
                                    bool bShift,
                                    CPDF_FormField* pTarget,
                                    const CFX_WideString& Value) {
  m_pEventHandler->OnField_Blur(bModifier, bShift, pTarget, Value);
}

void CJS_EventContext::OnField_Calculate(CPDF_FormField* pSource,
                                         CPDF_FormField* pTarget,
                                         CFX_WideString& Value,
                                         bool& bRc) {
  m_pEventHandler->OnField_Calculate(pSource, pTarget, Value, bRc);
}

void CJS_EventContext::OnField_Format(CPDF_FormField* pTarget,
                                      CFX_WideString& Value,
                                      bool bWillCommit) {
  m_pEventHandler->OnField_Format(pTarget, Value, bWillCommit);
}

void CJS_EventContext::OnField_Keystroke(CFX_WideString& strChange,
                                         const CFX_WideString& strChangeEx,
                                         bool bKeyDown,
                                         bool bModifier,
                                         int& nSelEnd,
                                         int& nSelStart,
                                         bool bShift,
                                         CPDF_FormField* pTarget,
                                         CFX_WideString& Value,
                                         bool bWillCommit,
                                         bool bFieldFull,
                                         bool& bRc) {
  m_pEventHandler->OnField_Keystroke(
      strChange, strChangeEx, bKeyDown, bModifier, nSelEnd, nSelStart, bShift,
      pTarget, Value, bWillCommit, bFieldFull, bRc);
}

void CJS_EventContext::OnField_Validate(CFX_WideString& strChange,
                                        const CFX_WideString& strChangeEx,
                                        bool bKeyDown,
                                        bool bModifier,
                                        bool bShift,
                                        CPDF_FormField* pTarget,
                                        CFX_WideString& Value,
                                        bool& bRc) {
  m_pEventHandler->OnField_Validate(strChange, strChangeEx, bKeyDown, bModifier,
                                    bShift, pTarget, Value, bRc);
}

void CJS_EventContext::OnScreen_Focus(bool bModifier,
                                      bool bShift,
                                      CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_Focus(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_Blur(bool bModifier,
                                     bool bShift,
                                     CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_Blur(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_Open(bool bModifier,
                                     bool bShift,
                                     CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_Open(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_Close(bool bModifier,
                                      bool bShift,
                                      CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_Close(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_MouseDown(bool bModifier,
                                          bool bShift,
                                          CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_MouseDown(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_MouseUp(bool bModifier,
                                        bool bShift,
                                        CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_MouseUp(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_MouseEnter(bool bModifier,
                                           bool bShift,
                                           CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_MouseEnter(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_MouseExit(bool bModifier,
                                          bool bShift,
                                          CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_MouseExit(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_InView(bool bModifier,
                                       bool bShift,
                                       CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_InView(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnScreen_OutView(bool bModifier,
                                        bool bShift,
                                        CPDFSDK_Annot* pScreen) {
  m_pEventHandler->OnScreen_OutView(bModifier, bShift, pScreen);
}

void CJS_EventContext::OnBookmark_MouseUp(CPDF_Bookmark* pBookMark) {
  m_pEventHandler->OnBookmark_MouseUp(pBookMark);
}

void CJS_EventContext::OnLink_MouseUp(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnLink_MouseUp(pFormFillEnv);
}

void CJS_EventContext::OnConsole_Exec() {
  m_pEventHandler->OnConsole_Exec();
}

void CJS_EventContext::OnExternal_Exec() {
  m_pEventHandler->OnExternal_Exec();
}

void CJS_EventContext::OnBatchExec(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pEventHandler->OnBatchExec(pFormFillEnv);
}

void CJS_EventContext::OnMenu_Exec(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                   const CFX_WideString& strTargetName) {
  m_pEventHandler->OnMenu_Exec(pFormFillEnv, strTargetName);
}
