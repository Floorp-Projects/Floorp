// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cfxjse_class.h"

#include <memory>

#include "fxjs/cfxjse_context.h"
#include "fxjs/cfxjse_value.h"

namespace {

void V8FunctionCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_FUNCTION_DESCRIPTOR* lpFunctionInfo =
      static_cast<FXJSE_FUNCTION_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpFunctionInfo)
    return;

  CFX_ByteStringC szFunctionName(lpFunctionInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpFunctionInfo->callbackProc(lpThisValue.get(), szFunctionName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty())
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
}

void V8ClassGlobalConstructorCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition =
      static_cast<FXJSE_CLASS_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpClassDefinition)
    return;

  CFX_ByteStringC szFunctionName(lpClassDefinition->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpClassDefinition->constructor(lpThisValue.get(), szFunctionName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty())
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
}

void V8GetterCallback_Wrapper(v8::Local<v8::String> property,
                              const v8::PropertyCallbackInfo<v8::Value>& info) {
  const FXJSE_PROPERTY_DESCRIPTOR* lpPropertyInfo =
      static_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpPropertyInfo)
    return;

  CFX_ByteStringC szPropertyName(lpPropertyInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  std::unique_ptr<CFXJSE_Value> lpPropValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  lpPropertyInfo->getProc(lpThisValue.get(), szPropertyName, lpPropValue.get());
  info.GetReturnValue().Set(lpPropValue->DirectGetValue());
}

void V8SetterCallback_Wrapper(v8::Local<v8::String> property,
                              v8::Local<v8::Value> value,
                              const v8::PropertyCallbackInfo<void>& info) {
  const FXJSE_PROPERTY_DESCRIPTOR* lpPropertyInfo =
      static_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpPropertyInfo)
    return;

  CFX_ByteStringC szPropertyName(lpPropertyInfo->name);
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  std::unique_ptr<CFXJSE_Value> lpPropValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  lpPropValue->ForceSetValue(value);
  lpPropertyInfo->setProc(lpThisValue.get(), szPropertyName, lpPropValue.get());
}

void V8ConstructorCallback_Wrapper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall())
    return;

  const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition =
      static_cast<FXJSE_CLASS_DESCRIPTOR*>(
          info.Data().As<v8::External>()->Value());
  if (!lpClassDefinition)
    return;

  ASSERT(info.This()->InternalFieldCount());
  info.This()->SetAlignedPointerInInternalField(0, nullptr);
}

void Context_GlobalObjToString(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  if (!lpClass)
    return;

  if (info.This() == info.Holder() && lpClass->name) {
    CFX_ByteString szStringVal;
    szStringVal.Format("[object %s]", lpClass->name);
    info.GetReturnValue().Set(v8::String::NewFromUtf8(
        info.GetIsolate(), szStringVal.c_str(), v8::String::kNormalString,
        szStringVal.GetLength()));
    return;
  }
  v8::Local<v8::String> local_str =
      info.This()
          ->ObjectProtoToString(info.GetIsolate()->GetCurrentContext())
          .FromMaybe(v8::Local<v8::String>());
  info.GetReturnValue().Set(local_str);
}

void DynPropGetterAdapter_MethodCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> hCallBackInfo = info.Data().As<v8::Object>();
  FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      hCallBackInfo->GetAlignedPointerFromInternalField(0));
  v8::Local<v8::String> hPropName =
      hCallBackInfo->GetInternalField(1).As<v8::String>();
  ASSERT(lpClass && !hPropName.IsEmpty());
  v8::String::Utf8Value szPropName(hPropName);
  CFX_ByteStringC szFxPropName = *szPropName;
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(info.This());
  std::unique_ptr<CFXJSE_Value> lpRetValue(new CFXJSE_Value(info.GetIsolate()));
  CFXJSE_Arguments impl(&info, lpRetValue.get());
  lpClass->dynMethodCall(lpThisValue.get(), szFxPropName, impl);
  if (!lpRetValue->DirectGetValue().IsEmpty())
    info.GetReturnValue().Set(lpRetValue->DirectGetValue());
}

void DynPropGetterAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                          CFXJSE_Value* pObject,
                          const CFX_ByteStringC& szPropName,
                          CFXJSE_Value* pValue) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, false);
  if (nPropType == FXJSE_ClassPropType_Property) {
    if (lpClass->dynPropGetter)
      lpClass->dynPropGetter(pObject, szPropName, pValue);
  } else if (nPropType == FXJSE_ClassPropType_Method) {
    if (lpClass->dynMethodCall && pValue) {
      v8::Isolate* pIsolate = pValue->GetIsolate();
      v8::HandleScope hscope(pIsolate);
      v8::Local<v8::ObjectTemplate> hCallBackInfoTemplate =
          v8::ObjectTemplate::New(pIsolate);
      hCallBackInfoTemplate->SetInternalFieldCount(2);
      v8::Local<v8::Object> hCallBackInfo =
          hCallBackInfoTemplate->NewInstance();
      hCallBackInfo->SetAlignedPointerInInternalField(
          0, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClass));
      hCallBackInfo->SetInternalField(
          1, v8::String::NewFromUtf8(
                 pIsolate, reinterpret_cast<const char*>(szPropName.raw_str()),
                 v8::String::kNormalString, szPropName.GetLength()));
      pValue->ForceSetValue(
          v8::Function::New(pValue->GetIsolate()->GetCurrentContext(),
                            DynPropGetterAdapter_MethodCallback, hCallBackInfo,
                            0, v8::ConstructorBehavior::kThrow)
              .ToLocalChecked());
    }
  }
}

void DynPropSetterAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                          CFXJSE_Value* pObject,
                          const CFX_ByteStringC& szPropName,
                          CFXJSE_Value* pValue) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, false);
  if (nPropType != FXJSE_ClassPropType_Method) {
    if (lpClass->dynPropSetter)
      lpClass->dynPropSetter(pObject, szPropName, pValue);
  }
}

bool DynPropQueryAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                         CFXJSE_Value* pObject,
                         const CFX_ByteStringC& szPropName) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, true);
  return nPropType != FXJSE_ClassPropType_None;
}

bool DynPropDeleterAdapter(const FXJSE_CLASS_DESCRIPTOR* lpClass,
                           CFXJSE_Value* pObject,
                           const CFX_ByteStringC& szPropName) {
  ASSERT(lpClass);
  int32_t nPropType =
      lpClass->dynPropTypeGetter == nullptr
          ? FXJSE_ClassPropType_Property
          : lpClass->dynPropTypeGetter(pObject, szPropName, false);
  if (nPropType != FXJSE_ClassPropType_Method) {
    if (lpClass->dynPropDeleter)
      return lpClass->dynPropDeleter(pObject, szPropName);
    return nPropType != FXJSE_ClassPropType_Property;
  }
  return false;
}

void NamedPropertyQueryCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::HandleScope scope(pIsolate);
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  if (DynPropQueryAdapter(lpClass, lpThisValue.get(), szFxPropName)) {
    info.GetReturnValue().Set(v8::DontDelete);
    return;
  }
  const int32_t iV8Absent = 64;
  info.GetReturnValue().Set(iV8Absent);
}

void NamedPropertyDeleterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::HandleScope scope(pIsolate);
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  info.GetReturnValue().Set(
      !!DynPropDeleterAdapter(lpClass, lpThisValue.get(), szFxPropName));
}

void NamedPropertyGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);
  std::unique_ptr<CFXJSE_Value> lpNewValue(new CFXJSE_Value(info.GetIsolate()));
  DynPropGetterAdapter(lpClass, lpThisValue.get(), szFxPropName,
                       lpNewValue.get());
  info.GetReturnValue().Set(lpNewValue->DirectGetValue());
}

void NamedPropertySetterCallback(
    v8::Local<v8::Name> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> thisObject = info.This();
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::String::Utf8Value szPropName(property);
  CFX_ByteStringC szFxPropName(*szPropName, szPropName.length());
  std::unique_ptr<CFXJSE_Value> lpThisValue(
      new CFXJSE_Value(info.GetIsolate()));
  lpThisValue->ForceSetValue(thisObject);

  std::unique_ptr<CFXJSE_Value> lpNewValue(new CFXJSE_Value(info.GetIsolate()));
  lpNewValue->ForceSetValue(value);
  DynPropSetterAdapter(lpClass, lpThisValue.get(), szFxPropName,
                       lpNewValue.get());
  info.GetReturnValue().Set(value);
}

void NamedPropertyEnumeratorCallback(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  const FXJSE_CLASS_DESCRIPTOR* lpClass = static_cast<FXJSE_CLASS_DESCRIPTOR*>(
      info.Data().As<v8::External>()->Value());
  v8::Isolate* pIsolate = info.GetIsolate();
  v8::Local<v8::Array> newArray = v8::Array::New(pIsolate, lpClass->propNum);
  for (int i = 0; i < lpClass->propNum; i++) {
    newArray->Set(
        i, v8::String::NewFromUtf8(pIsolate, lpClass->properties[i].name));
  }
  info.GetReturnValue().Set(newArray);
}

}  // namespace

// static
CFXJSE_Class* CFXJSE_Class::Create(
    CFXJSE_Context* lpContext,
    const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition,
    bool bIsJSGlobal) {
  if (!lpContext || !lpClassDefinition)
    return nullptr;

  CFXJSE_Class* pClass =
      GetClassFromContext(lpContext, lpClassDefinition->name);
  if (pClass)
    return pClass;

  v8::Isolate* pIsolate = lpContext->m_pIsolate;
  pClass = new CFXJSE_Class(lpContext);
  pClass->m_szClassName = lpClassDefinition->name;
  pClass->m_lpClassDefinition = lpClassDefinition;
  CFXJSE_ScopeUtil_IsolateHandleRootContext scope(pIsolate);
  v8::Local<v8::FunctionTemplate> hFunctionTemplate = v8::FunctionTemplate::New(
      pIsolate, bIsJSGlobal ? 0 : V8ConstructorCallback_Wrapper,
      v8::External::New(
          pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)));
  hFunctionTemplate->SetClassName(
      v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name));
  hFunctionTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  v8::Local<v8::ObjectTemplate> hObjectTemplate =
      hFunctionTemplate->InstanceTemplate();
  SetUpNamedPropHandler(pIsolate, hObjectTemplate, lpClassDefinition);

  if (lpClassDefinition->propNum) {
    for (int32_t i = 0; i < lpClassDefinition->propNum; i++) {
      hObjectTemplate->SetNativeDataProperty(
          v8::String::NewFromUtf8(pIsolate,
                                  lpClassDefinition->properties[i].name),
          lpClassDefinition->properties[i].getProc ? V8GetterCallback_Wrapper
                                                   : nullptr,
          lpClassDefinition->properties[i].setProc ? V8SetterCallback_Wrapper
                                                   : nullptr,
          v8::External::New(pIsolate, const_cast<FXJSE_PROPERTY_DESCRIPTOR*>(
                                          lpClassDefinition->properties + i)),
          static_cast<v8::PropertyAttribute>(v8::DontDelete));
    }
  }
  if (lpClassDefinition->methNum) {
    for (int32_t i = 0; i < lpClassDefinition->methNum; i++) {
      v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
          pIsolate, V8FunctionCallback_Wrapper,
          v8::External::New(pIsolate, const_cast<FXJSE_FUNCTION_DESCRIPTOR*>(
                                          lpClassDefinition->methods + i)));
      fun->RemovePrototype();
      hObjectTemplate->Set(
          v8::String::NewFromUtf8(pIsolate, lpClassDefinition->methods[i].name),
          fun,
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
    }
  }
  if (lpClassDefinition->constructor) {
    if (bIsJSGlobal) {
      hObjectTemplate->Set(
          v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name),
          v8::FunctionTemplate::New(
              pIsolate, V8ClassGlobalConstructorCallback_Wrapper,
              v8::External::New(pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(
                                              lpClassDefinition))),
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
    } else {
      v8::Local<v8::Context> hLocalContext =
          v8::Local<v8::Context>::New(pIsolate, lpContext->m_hContext);
      FXJSE_GetGlobalObjectFromContext(hLocalContext)
          ->Set(v8::String::NewFromUtf8(pIsolate, lpClassDefinition->name),
                v8::Function::New(
                    pIsolate, V8ClassGlobalConstructorCallback_Wrapper,
                    v8::External::New(pIsolate,
                                      const_cast<FXJSE_CLASS_DESCRIPTOR*>(
                                          lpClassDefinition))));
    }
  }
  if (bIsJSGlobal) {
    v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(
        pIsolate, Context_GlobalObjToString,
        v8::External::New(
            pIsolate, const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)));
    fun->RemovePrototype();
    hObjectTemplate->Set(v8::String::NewFromUtf8(pIsolate, "toString"), fun);
  }
  pClass->m_hTemplate.Reset(lpContext->m_pIsolate, hFunctionTemplate);
  lpContext->m_rgClasses.push_back(std::unique_ptr<CFXJSE_Class>(pClass));
  return pClass;
}

// static
CFXJSE_Class* CFXJSE_Class::GetClassFromContext(CFXJSE_Context* pContext,
                                                const CFX_ByteStringC& szName) {
  for (const auto& pClass : pContext->m_rgClasses) {
    if (pClass->m_szClassName == szName)
      return pClass.get();
  }
  return nullptr;
}

// static
void CFXJSE_Class::SetUpNamedPropHandler(
    v8::Isolate* pIsolate,
    v8::Local<v8::ObjectTemplate>& hObjectTemplate,
    const FXJSE_CLASS_DESCRIPTOR* lpClassDefinition) {
  v8::NamedPropertyHandlerConfiguration configuration(
      lpClassDefinition->dynPropGetter ? NamedPropertyGetterCallback : 0,
      lpClassDefinition->dynPropSetter ? NamedPropertySetterCallback : 0,
      lpClassDefinition->dynPropTypeGetter ? NamedPropertyQueryCallback : 0,
      lpClassDefinition->dynPropDeleter ? NamedPropertyDeleterCallback : 0,
      NamedPropertyEnumeratorCallback,
      v8::External::New(pIsolate,
                        const_cast<FXJSE_CLASS_DESCRIPTOR*>(lpClassDefinition)),
      v8::PropertyHandlerFlags::kNonMasking);
  hObjectTemplate->SetHandler(configuration);
}

CFXJSE_Class::CFXJSE_Class(CFXJSE_Context* lpContext)
    : m_lpClassDefinition(nullptr), m_pContext(lpContext) {}

CFXJSE_Class::~CFXJSE_Class() {}
