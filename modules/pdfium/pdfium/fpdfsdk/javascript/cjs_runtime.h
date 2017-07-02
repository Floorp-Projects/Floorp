// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_CJS_RUNTIME_H_
#define FPDFSDK_JAVASCRIPT_CJS_RUNTIME_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "core/fxcrt/cfx_observable.h"
#include "core/fxcrt/fx_basic.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "fxjs/fxjs_v8.h"

class CJS_EventContext;

class CJS_Runtime : public IJS_Runtime,
                    public CFXJS_Engine,
                    public CFX_Observable<CJS_Runtime> {
 public:
  using FieldEvent = std::pair<CFX_WideString, JS_EVENT_T>;

  static CJS_Runtime* CurrentRuntimeFromIsolate(v8::Isolate* pIsolate);

  explicit CJS_Runtime(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  ~CJS_Runtime() override;

  // IJS_Runtime
  IJS_EventContext* NewEventContext() override;
  void ReleaseEventContext(IJS_EventContext* pContext) override;
  CPDFSDK_FormFillEnvironment* GetFormFillEnv() const override;
  int ExecuteScript(const CFX_WideString& script,
                    CFX_WideString* info) override;

  CJS_EventContext* GetCurrentEventContext() const;

  // Returns true if the event isn't already found in the set.
  bool AddEventToSet(const FieldEvent& event);
  void RemoveEventFromSet(const FieldEvent& event);

  void BeginBlock() { m_bBlocking = true; }
  void EndBlock() { m_bBlocking = false; }
  bool IsBlocking() const { return m_bBlocking; }

#ifdef PDF_ENABLE_XFA
  bool GetValueByName(const CFX_ByteStringC& utf8Name,
                      CFXJSE_Value* pValue) override;
  bool SetValueByName(const CFX_ByteStringC& utf8Name,
                      CFXJSE_Value* pValue) override;
#endif  // PDF_ENABLE_XFA

 private:
  void DefineJSObjects();
  void SetFormFillEnvToDocument();

  std::vector<std::unique_ptr<CJS_EventContext>> m_EventContextArray;
  CPDFSDK_FormFillEnvironment::ObservedPtr m_pFormFillEnv;
  bool m_bBlocking;
  bool m_isolateManaged;
  std::set<FieldEvent> m_FieldEventSet;
};

#endif  // FPDFSDK_JAVASCRIPT_CJS_RUNTIME_H_
