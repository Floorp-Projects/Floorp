// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cfxjse_arguments.h"

#include "fxjs/cfxjse_context.h"
#include "fxjs/cfxjse_value.h"

v8::Isolate* CFXJSE_Arguments::GetRuntime() const {
  return m_pRetValue->GetIsolate();
}

int32_t CFXJSE_Arguments::GetLength() const {
  return m_pInfo->Length();
}

std::unique_ptr<CFXJSE_Value> CFXJSE_Arguments::GetValue(int32_t index) const {
  std::unique_ptr<CFXJSE_Value> lpArgValue(
      new CFXJSE_Value(v8::Isolate::GetCurrent()));
  lpArgValue->ForceSetValue((*m_pInfo)[index]);
  return lpArgValue;
}

bool CFXJSE_Arguments::GetBoolean(int32_t index) const {
  return (*m_pInfo)[index]->BooleanValue();
}

int32_t CFXJSE_Arguments::GetInt32(int32_t index) const {
  return static_cast<int32_t>((*m_pInfo)[index]->NumberValue());
}

FX_FLOAT CFXJSE_Arguments::GetFloat(int32_t index) const {
  return static_cast<FX_FLOAT>((*m_pInfo)[index]->NumberValue());
}

CFX_ByteString CFXJSE_Arguments::GetUTF8String(int32_t index) const {
  v8::Local<v8::String> hString = (*m_pInfo)[index]->ToString();
  v8::String::Utf8Value szStringVal(hString);
  return CFX_ByteString(*szStringVal);
}

CFXJSE_HostObject* CFXJSE_Arguments::GetObject(int32_t index,
                                               CFXJSE_Class* pClass) const {
  v8::Local<v8::Value> hValue = (*m_pInfo)[index];
  ASSERT(!hValue.IsEmpty());
  if (!hValue->IsObject())
    return nullptr;
  return FXJSE_RetrieveObjectBinding(hValue.As<v8::Object>(), pClass);
}

CFXJSE_Value* CFXJSE_Arguments::GetReturnValue() {
  return m_pRetValue;
}
