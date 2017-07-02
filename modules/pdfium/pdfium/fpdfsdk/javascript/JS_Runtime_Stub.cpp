// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <memory>

#include "fpdfsdk/javascript/ijs_event_context.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "third_party/base/ptr_util.h"

class CJS_EventContextStub final : public IJS_EventContext {
 public:
  CJS_EventContextStub() {}
  ~CJS_EventContextStub() override {}

  // IJS_EventContext:
  bool RunScript(const CFX_WideString& script, CFX_WideString* info) override {
    return false;
  }

  void OnApp_Init() override {}
  void OnDoc_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                  const CFX_WideString& strTargetName) override {}
  void OnDoc_WillPrint(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnDoc_DidPrint(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnDoc_WillSave(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnDoc_DidSave(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnDoc_WillClose(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnPage_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnPage_Close(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnPage_InView(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnPage_OutView(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnField_MouseDown(bool bModifier,
                         bool bShift,
                         CPDF_FormField* pTarget) override {}
  void OnField_MouseEnter(bool bModifier,
                          bool bShift,
                          CPDF_FormField* pTarget) override {}
  void OnField_MouseExit(bool bModifier,
                         bool bShift,
                         CPDF_FormField* pTarget) override {}
  void OnField_MouseUp(bool bModifier,
                       bool bShift,
                       CPDF_FormField* pTarget) override {}
  void OnField_Focus(bool bModifier,
                     bool bShift,
                     CPDF_FormField* pTarget,
                     const CFX_WideString& Value) override {}
  void OnField_Blur(bool bModifier,
                    bool bShift,
                    CPDF_FormField* pTarget,
                    const CFX_WideString& Value) override {}
  void OnField_Calculate(CPDF_FormField* pSource,
                         CPDF_FormField* pTarget,
                         CFX_WideString& Value,
                         bool& bRc) override {}
  void OnField_Format(CPDF_FormField* pTarget,
                      CFX_WideString& Value,
                      bool bWillCommit) override {}
  void OnField_Keystroke(CFX_WideString& strChange,
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
                         bool& bRc) override {}
  void OnField_Validate(CFX_WideString& strChange,
                        const CFX_WideString& strChangeEx,
                        bool bKeyDown,
                        bool bModifier,
                        bool bShift,
                        CPDF_FormField* pTarget,
                        CFX_WideString& Value,
                        bool& bRc) override {}
  void OnScreen_Focus(bool bModifier,
                      bool bShift,
                      CPDFSDK_Annot* pScreen) override {}
  void OnScreen_Blur(bool bModifier,
                     bool bShift,
                     CPDFSDK_Annot* pScreen) override {}
  void OnScreen_Open(bool bModifier,
                     bool bShift,
                     CPDFSDK_Annot* pScreen) override {}
  void OnScreen_Close(bool bModifier,
                      bool bShift,
                      CPDFSDK_Annot* pScreen) override {}
  void OnScreen_MouseDown(bool bModifier,
                          bool bShift,
                          CPDFSDK_Annot* pScreen) override {}
  void OnScreen_MouseUp(bool bModifier,
                        bool bShift,
                        CPDFSDK_Annot* pScreen) override {}
  void OnScreen_MouseEnter(bool bModifier,
                           bool bShift,
                           CPDFSDK_Annot* pScreen) override {}
  void OnScreen_MouseExit(bool bModifier,
                          bool bShift,
                          CPDFSDK_Annot* pScreen) override {}
  void OnScreen_InView(bool bModifier,
                       bool bShift,
                       CPDFSDK_Annot* pScreen) override {}
  void OnScreen_OutView(bool bModifier,
                        bool bShift,
                        CPDFSDK_Annot* pScreen) override {}
  void OnBookmark_MouseUp(CPDF_Bookmark* pBookMark) override {}
  void OnLink_MouseUp(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnMenu_Exec(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                   const CFX_WideString&) override {}
  void OnBatchExec(CPDFSDK_FormFillEnvironment* pFormFillEnv) override {}
  void OnConsole_Exec() override {}
  void OnExternal_Exec() override {}
};

class CJS_RuntimeStub final : public IJS_Runtime {
 public:
  explicit CJS_RuntimeStub(CPDFSDK_FormFillEnvironment* pFormFillEnv)
      : m_pFormFillEnv(pFormFillEnv) {}
  ~CJS_RuntimeStub() override {}

  IJS_EventContext* NewEventContext() override {
    if (!m_pContext)
      m_pContext = pdfium::MakeUnique<CJS_EventContextStub>();
    return m_pContext.get();
  }

  void ReleaseEventContext(IJS_EventContext* pContext) override {}

  CPDFSDK_FormFillEnvironment* GetFormFillEnv() const override {
    return m_pFormFillEnv;
  }

#ifdef PDF_ENABLE_XFA
  bool GetValueByName(const CFX_ByteStringC&, CFXJSE_Value*) override {
    return false;
  }

  bool SetValueByName(const CFX_ByteStringC&, CFXJSE_Value*) override {
    return false;
  }
#endif  // PDF_ENABLE_XFA

  int ExecuteScript(const CFX_WideString& script,
                    CFX_WideString* info) override {
    return 0;
  }

 protected:
  CPDFSDK_FormFillEnvironment* m_pFormFillEnv;
  std::unique_ptr<CJS_EventContextStub> m_pContext;
};

// static
void IJS_Runtime::Initialize(unsigned int slot, void* isolate) {}

// static
void IJS_Runtime::Destroy() {}

// static
IJS_Runtime* IJS_Runtime::Create(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  return new CJS_RuntimeStub(pFormFillEnv);
}
