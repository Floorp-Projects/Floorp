// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CFXJSE_VALUE_H_
#define FXJS_CFXJSE_VALUE_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "fxjs/cfxjse_isolatetracker.h"
#include "fxjs/cfxjse_runtimedata.h"
#include "v8/include/v8.h"

class CFXJSE_Class;
class CFXJSE_HostObject;

class CFXJSE_Value {
 public:
  explicit CFXJSE_Value(v8::Isolate* pIsolate);
  ~CFXJSE_Value();

  bool IsUndefined() const;
  bool IsNull() const;
  bool IsBoolean() const;
  bool IsString() const;
  bool IsNumber() const;
  bool IsInteger() const;
  bool IsObject() const;
  bool IsArray() const;
  bool IsFunction() const;
  bool IsDate() const;
  bool ToBoolean() const;
  FX_FLOAT ToFloat() const;
  double ToDouble() const;
  int32_t ToInteger() const;
  CFX_ByteString ToString() const;
  CFX_WideString ToWideString() const {
    return CFX_WideString::FromUTF8(ToString().AsStringC());
  }
  CFXJSE_HostObject* ToHostObject(CFXJSE_Class* lpClass) const;

  void SetUndefined();
  void SetNull();
  void SetBoolean(bool bBoolean);
  void SetInteger(int32_t nInteger);
  void SetDouble(double dDouble);
  void SetString(const CFX_ByteStringC& szString);
  void SetFloat(FX_FLOAT fFloat);
  void SetJSObject();

  void SetObject(CFXJSE_HostObject* lpObject, CFXJSE_Class* pClass);
  void SetHostObject(CFXJSE_HostObject* lpObject, CFXJSE_Class* lpClass);
  void SetArray(const std::vector<std::unique_ptr<CFXJSE_Value>>& values);
  void SetDate(double dDouble);

  bool GetObjectProperty(const CFX_ByteStringC& szPropName,
                         CFXJSE_Value* lpPropValue);
  bool SetObjectProperty(const CFX_ByteStringC& szPropName,
                         CFXJSE_Value* lpPropValue);
  bool GetObjectPropertyByIdx(uint32_t uPropIdx, CFXJSE_Value* lpPropValue);
  bool SetObjectProperty(uint32_t uPropIdx, CFXJSE_Value* lpPropValue);
  bool DeleteObjectProperty(const CFX_ByteStringC& szPropName);
  bool HasObjectOwnProperty(const CFX_ByteStringC& szPropName,
                            bool bUseTypeGetter);
  bool SetObjectOwnProperty(const CFX_ByteStringC& szPropName,
                            CFXJSE_Value* lpPropValue);
  bool SetFunctionBind(CFXJSE_Value* lpOldFunction, CFXJSE_Value* lpNewThis);
  bool Call(CFXJSE_Value* lpReceiver,
            CFXJSE_Value* lpRetValue,
            uint32_t nArgCount,
            CFXJSE_Value** lpArgs);

  v8::Isolate* GetIsolate() const { return m_pIsolate; }
  const v8::Global<v8::Value>& DirectGetValue() const { return m_hValue; }
  void ForceSetValue(v8::Local<v8::Value> hValue) {
    m_hValue.Reset(m_pIsolate, hValue);
  }
  void Assign(const CFXJSE_Value* lpValue) {
    ASSERT(lpValue);
    if (lpValue) {
      m_hValue.Reset(m_pIsolate, lpValue->m_hValue);
    } else {
      m_hValue.Reset();
    }
  }

 private:
  friend class CFXJSE_Class;
  friend class CFXJSE_Context;

  CFXJSE_Value();
  CFXJSE_Value(const CFXJSE_Value&);
  CFXJSE_Value& operator=(const CFXJSE_Value&);

  v8::Isolate* m_pIsolate;
  v8::Global<v8::Value> m_hValue;
};

#endif  // FXJS_CFXJSE_VALUE_H_
