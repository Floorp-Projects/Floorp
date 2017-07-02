// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_ARGUMENTS_H_
#define FXJS_CFXJSE_ARGUMENTS_H_

#include <memory>

#include "fxjs/fxjse.h"

class CFXJSE_Class;

class CFXJSE_Arguments {
 public:
  CFXJSE_Arguments(const v8::FunctionCallbackInfo<v8::Value>* pInfo,
                   CFXJSE_Value* pRetValue)
      : m_pInfo(pInfo), m_pRetValue(pRetValue) {}

  v8::Isolate* GetRuntime() const;
  int32_t GetLength() const;
  std::unique_ptr<CFXJSE_Value> GetValue(int32_t index) const;
  bool GetBoolean(int32_t index) const;
  int32_t GetInt32(int32_t index) const;
  FX_FLOAT GetFloat(int32_t index) const;
  CFX_ByteString GetUTF8String(int32_t index) const;
  CFXJSE_HostObject* GetObject(int32_t index,
                               CFXJSE_Class* pClass = nullptr) const;
  CFXJSE_Value* GetReturnValue();

 private:
  const v8::FunctionCallbackInfo<v8::Value>* m_pInfo;
  CFXJSE_Value* m_pRetValue;
};

#endif  // FXJS_CFXJSE_ARGUMENTS_H_
