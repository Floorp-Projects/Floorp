// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_CONTEXT_H_
#define FXJS_CFXJSE_CONTEXT_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "fxjs/fxjse.h"
#include "v8/include/v8.h"

class CFXJSE_Class;
class CFXJSE_Value;
struct FXJSE_CLASS_DESCRIPTOR;

class CFXJSE_Context {
 public:
  static CFXJSE_Context* Create(
      v8::Isolate* pIsolate,
      const FXJSE_CLASS_DESCRIPTOR* lpGlobalClass = nullptr,
      CFXJSE_HostObject* lpGlobalObject = nullptr);

  ~CFXJSE_Context();

  v8::Isolate* GetRuntime() { return m_pIsolate; }
  std::unique_ptr<CFXJSE_Value> GetGlobalObject();
  void EnableCompatibleMode();
  bool ExecuteScript(const FX_CHAR* szScript,
                     CFXJSE_Value* lpRetValue,
                     CFXJSE_Value* lpNewThisObject = nullptr);

 protected:
  friend class CFXJSE_Class;
  friend class CFXJSE_ScopeUtil_IsolateHandleContext;

  CFXJSE_Context();
  CFXJSE_Context(const CFXJSE_Context&);
  explicit CFXJSE_Context(v8::Isolate* pIsolate);

  CFXJSE_Context& operator=(const CFXJSE_Context&);

  v8::Global<v8::Context> m_hContext;
  v8::Isolate* m_pIsolate;
  std::vector<std::unique_ptr<CFXJSE_Class>> m_rgClasses;
};

v8::Local<v8::Object> FXJSE_CreateReturnValue(v8::Isolate* pIsolate,
                                              v8::TryCatch& trycatch);

v8::Local<v8::Object> FXJSE_GetGlobalObjectFromContext(
    const v8::Local<v8::Context>& hContext);

void FXJSE_UpdateObjectBinding(v8::Local<v8::Object>& hObject,
                               CFXJSE_HostObject* lpNewBinding = nullptr);

CFXJSE_HostObject* FXJSE_RetrieveObjectBinding(
    const v8::Local<v8::Object>& hJSObject,
    CFXJSE_Class* lpClass = nullptr);

#endif  // FXJS_CFXJSE_CONTEXT_H_
