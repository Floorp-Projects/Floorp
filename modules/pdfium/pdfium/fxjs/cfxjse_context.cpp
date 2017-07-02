// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cfxjse_context.h"

#include "fxjs/cfxjse_class.h"
#include "fxjs/cfxjse_value.h"

namespace {

const FX_CHAR szCompatibleModeScript[] =
    "(function(global, list) {\n"
    "  'use strict';\n"
    "  var objname;\n"
    "  for (objname in list) {\n"
    "    var globalobj = global[objname];\n"
    "    if (globalobj) {\n"
    "      list[objname].forEach(function(name) {\n"
    "        if (!globalobj[name]) {\n"
    "          Object.defineProperty(globalobj, name, {\n"
    "            writable: true,\n"
    "            enumerable: false,\n"
    "            value: (function(obj) {\n"
    "              if (arguments.length === 0) {\n"
    "                throw new TypeError('missing argument 0 when calling "
    "                    function ' + objname + '.' + name);\n"
    "              }\n"
    "              return globalobj.prototype[name].apply(obj, "
    "                  Array.prototype.slice.call(arguments, 1));\n"
    "            })\n"
    "          });\n"
    "        }\n"
    "      });\n"
    "    }\n"
    "  }\n"
    "}(this, {String: ['substr', 'toUpperCase']}));";

}  // namespace

// Note, not in the anonymous namespace due to the friend call
// in cfxjse_context.h
// TODO(dsinclair): Remove the friending, use public methods.
class CFXJSE_ScopeUtil_IsolateHandleContext {
 public:
  explicit CFXJSE_ScopeUtil_IsolateHandleContext(CFXJSE_Context* pContext)
      : m_context(pContext),
        m_parent(pContext->m_pIsolate),
        m_cscope(v8::Local<v8::Context>::New(pContext->m_pIsolate,
                                             pContext->m_hContext)) {}
  v8::Isolate* GetIsolate() { return m_context->m_pIsolate; }
  v8::Local<v8::Context> GetLocalContext() {
    return v8::Local<v8::Context>::New(m_context->m_pIsolate,
                                       m_context->m_hContext);
  }

 private:
  CFXJSE_ScopeUtil_IsolateHandleContext(
      const CFXJSE_ScopeUtil_IsolateHandleContext&) = delete;
  void operator=(const CFXJSE_ScopeUtil_IsolateHandleContext&) = delete;
  void* operator new(size_t size) = delete;
  void operator delete(void*, size_t) = delete;

  CFXJSE_Context* m_context;
  CFXJSE_ScopeUtil_IsolateHandle m_parent;
  v8::Context::Scope m_cscope;
};

v8::Local<v8::Object> FXJSE_GetGlobalObjectFromContext(
    const v8::Local<v8::Context>& hContext) {
  return hContext->Global()->GetPrototype().As<v8::Object>();
}

void FXJSE_UpdateObjectBinding(v8::Local<v8::Object>& hObject,
                               CFXJSE_HostObject* lpNewBinding) {
  ASSERT(!hObject.IsEmpty());
  ASSERT(hObject->InternalFieldCount() > 0);
  hObject->SetAlignedPointerInInternalField(0,
                                            static_cast<void*>(lpNewBinding));
}

CFXJSE_HostObject* FXJSE_RetrieveObjectBinding(
    const v8::Local<v8::Object>& hJSObject,
    CFXJSE_Class* lpClass) {
  ASSERT(!hJSObject.IsEmpty());
  if (!hJSObject->IsObject())
    return nullptr;

  v8::Local<v8::Object> hObject = hJSObject;
  if (hObject->InternalFieldCount() == 0) {
    v8::Local<v8::Value> hProtoObject = hObject->GetPrototype();
    if (hProtoObject.IsEmpty() || !hProtoObject->IsObject())
      return nullptr;

    hObject = hProtoObject.As<v8::Object>();
    if (hObject->InternalFieldCount() == 0)
      return nullptr;
  }
  if (lpClass) {
    v8::Local<v8::FunctionTemplate> hClass =
        v8::Local<v8::FunctionTemplate>::New(
            lpClass->GetContext()->GetRuntime(), lpClass->GetTemplate());
    if (!hClass->HasInstance(hObject))
      return nullptr;
  }
  return static_cast<CFXJSE_HostObject*>(
      hObject->GetAlignedPointerFromInternalField(0));
}

v8::Local<v8::Object> FXJSE_CreateReturnValue(v8::Isolate* pIsolate,
                                              v8::TryCatch& trycatch) {
  v8::Local<v8::Object> hReturnValue = v8::Object::New(pIsolate);
  if (trycatch.HasCaught()) {
    v8::Local<v8::Value> hException = trycatch.Exception();
    v8::Local<v8::Message> hMessage = trycatch.Message();
    if (hException->IsObject()) {
      v8::Local<v8::Value> hValue;
      hValue = hException.As<v8::Object>()->Get(
          v8::String::NewFromUtf8(pIsolate, "name"));
      if (hValue->IsString() || hValue->IsStringObject())
        hReturnValue->Set(0, hValue);
      else
        hReturnValue->Set(0, v8::String::NewFromUtf8(pIsolate, "Error"));

      hValue = hException.As<v8::Object>()->Get(
          v8::String::NewFromUtf8(pIsolate, "message"));
      if (hValue->IsString() || hValue->IsStringObject())
        hReturnValue->Set(1, hValue);
      else
        hReturnValue->Set(1, hMessage->Get());
    } else {
      hReturnValue->Set(0, v8::String::NewFromUtf8(pIsolate, "Error"));
      hReturnValue->Set(1, hMessage->Get());
    }
    hReturnValue->Set(2, hException);
    hReturnValue->Set(3, v8::Integer::New(pIsolate, hMessage->GetLineNumber()));
    hReturnValue->Set(4, hMessage->GetSourceLine());
    v8::Maybe<int32_t> maybe_int =
        hMessage->GetStartColumn(pIsolate->GetCurrentContext());
    hReturnValue->Set(5, v8::Integer::New(pIsolate, maybe_int.FromMaybe(0)));
    maybe_int = hMessage->GetEndColumn(pIsolate->GetCurrentContext());
    hReturnValue->Set(6, v8::Integer::New(pIsolate, maybe_int.FromMaybe(0)));
  }
  return hReturnValue;
}

// static
CFXJSE_Context* CFXJSE_Context::Create(
    v8::Isolate* pIsolate,
    const FXJSE_CLASS_DESCRIPTOR* lpGlobalClass,
    CFXJSE_HostObject* lpGlobalObject) {
  CFXJSE_ScopeUtil_IsolateHandle scope(pIsolate);
  CFXJSE_Context* pContext = new CFXJSE_Context(pIsolate);
  CFXJSE_Class* lpGlobalClassObj = nullptr;
  v8::Local<v8::ObjectTemplate> hObjectTemplate;
  if (lpGlobalClass) {
    lpGlobalClassObj = CFXJSE_Class::Create(pContext, lpGlobalClass, true);
    ASSERT(lpGlobalClassObj);
    v8::Local<v8::FunctionTemplate> hFunctionTemplate =
        v8::Local<v8::FunctionTemplate>::New(pIsolate,
                                             lpGlobalClassObj->m_hTemplate);
    hObjectTemplate = hFunctionTemplate->InstanceTemplate();
  } else {
    hObjectTemplate = v8::ObjectTemplate::New(pIsolate);
    hObjectTemplate->SetInternalFieldCount(1);
  }
  hObjectTemplate->Set(
      v8::Symbol::GetToStringTag(pIsolate),
      v8::String::NewFromUtf8(pIsolate, "global", v8::NewStringType::kNormal)
          .ToLocalChecked());
  v8::Local<v8::Context> hNewContext =
      v8::Context::New(pIsolate, nullptr, hObjectTemplate);
  v8::Local<v8::Context> hRootContext = v8::Local<v8::Context>::New(
      pIsolate, CFXJSE_RuntimeData::Get(pIsolate)->m_hRootContext);
  hNewContext->SetSecurityToken(hRootContext->GetSecurityToken());
  v8::Local<v8::Object> hGlobalObject =
      FXJSE_GetGlobalObjectFromContext(hNewContext);
  FXJSE_UpdateObjectBinding(hGlobalObject, lpGlobalObject);
  pContext->m_hContext.Reset(pIsolate, hNewContext);
  return pContext;
}

CFXJSE_Context::CFXJSE_Context(v8::Isolate* pIsolate) : m_pIsolate(pIsolate) {}

CFXJSE_Context::~CFXJSE_Context() {}

std::unique_ptr<CFXJSE_Value> CFXJSE_Context::GetGlobalObject() {
  std::unique_ptr<CFXJSE_Value> pValue(new CFXJSE_Value(m_pIsolate));

  CFXJSE_ScopeUtil_IsolateHandleContext scope(this);
  v8::Local<v8::Context> hContext =
      v8::Local<v8::Context>::New(m_pIsolate, m_hContext);
  v8::Local<v8::Object> hGlobalObject = hContext->Global();
  pValue->ForceSetValue(hGlobalObject);

  return pValue;
}

void CFXJSE_Context::EnableCompatibleMode() {
  ExecuteScript(szCompatibleModeScript, nullptr, nullptr);
}

bool CFXJSE_Context::ExecuteScript(const FX_CHAR* szScript,
                                   CFXJSE_Value* lpRetValue,
                                   CFXJSE_Value* lpNewThisObject) {
  CFXJSE_ScopeUtil_IsolateHandleContext scope(this);
  v8::TryCatch trycatch(m_pIsolate);
  v8::Local<v8::String> hScriptString =
      v8::String::NewFromUtf8(m_pIsolate, szScript);
  if (!lpNewThisObject) {
    v8::Local<v8::Script> hScript = v8::Script::Compile(hScriptString);
    if (!trycatch.HasCaught()) {
      v8::Local<v8::Value> hValue = hScript->Run();
      if (!trycatch.HasCaught()) {
        if (lpRetValue)
          lpRetValue->m_hValue.Reset(m_pIsolate, hValue);
        return true;
      }
    }
    if (lpRetValue) {
      lpRetValue->m_hValue.Reset(m_pIsolate,
                                 FXJSE_CreateReturnValue(m_pIsolate, trycatch));
    }
    return false;
  }

  v8::Local<v8::Value> hNewThis =
      v8::Local<v8::Value>::New(m_pIsolate, lpNewThisObject->m_hValue);
  ASSERT(!hNewThis.IsEmpty());
  v8::Local<v8::Script> hWrapper = v8::Script::Compile(v8::String::NewFromUtf8(
      m_pIsolate, "(function () { return eval(arguments[0]); })"));
  v8::Local<v8::Value> hWrapperValue = hWrapper->Run();
  ASSERT(hWrapperValue->IsFunction());
  v8::Local<v8::Function> hWrapperFn = hWrapperValue.As<v8::Function>();
  if (!trycatch.HasCaught()) {
    v8::Local<v8::Value> rgArgs[] = {hScriptString};
    v8::Local<v8::Value> hValue =
        hWrapperFn->Call(hNewThis.As<v8::Object>(), 1, rgArgs);
    if (!trycatch.HasCaught()) {
      if (lpRetValue)
        lpRetValue->m_hValue.Reset(m_pIsolate, hValue);
      return true;
    }
  }
  if (lpRetValue) {
    lpRetValue->m_hValue.Reset(m_pIsolate,
                               FXJSE_CreateReturnValue(m_pIsolate, trycatch));
  }
  return false;
}
