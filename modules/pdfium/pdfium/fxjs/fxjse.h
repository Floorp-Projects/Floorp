// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_FXJSE_H_
#define FXJS_FXJSE_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "v8/include/v8.h"

class CFXJSE_Arguments;
class CFXJSE_Value;

// C++ object which can be wrapped by CFXJSE_value.
class CFXJSE_HostObject {
 public:
  virtual ~CFXJSE_HostObject() {}
};

typedef void (*FXJSE_FuncCallback)(CFXJSE_Value* pThis,
                                   const CFX_ByteStringC& szFuncName,
                                   CFXJSE_Arguments& args);
typedef void (*FXJSE_PropAccessor)(CFXJSE_Value* pObject,
                                   const CFX_ByteStringC& szPropName,
                                   CFXJSE_Value* pValue);
typedef int32_t (*FXJSE_PropTypeGetter)(CFXJSE_Value* pObject,
                                        const CFX_ByteStringC& szPropName,
                                        bool bQueryIn);
typedef bool (*FXJSE_PropDeleter)(CFXJSE_Value* pObject,
                                  const CFX_ByteStringC& szPropName);

enum FXJSE_ClassPropTypes {
  FXJSE_ClassPropType_None,
  FXJSE_ClassPropType_Property,
  FXJSE_ClassPropType_Method
};

struct FXJSE_FUNCTION_DESCRIPTOR {
  const FX_CHAR* name;
  FXJSE_FuncCallback callbackProc;
};

struct FXJSE_PROPERTY_DESCRIPTOR {
  const FX_CHAR* name;
  FXJSE_PropAccessor getProc;
  FXJSE_PropAccessor setProc;
};

struct FXJSE_CLASS_DESCRIPTOR {
  const FX_CHAR* name;
  FXJSE_FuncCallback constructor;
  const FXJSE_PROPERTY_DESCRIPTOR* properties;
  const FXJSE_FUNCTION_DESCRIPTOR* methods;
  int32_t propNum;
  int32_t methNum;
  FXJSE_PropTypeGetter dynPropTypeGetter;
  FXJSE_PropAccessor dynPropGetter;
  FXJSE_PropAccessor dynPropSetter;
  FXJSE_PropDeleter dynPropDeleter;
  FXJSE_FuncCallback dynMethodCall;
};

void FXJSE_Initialize();
void FXJSE_Finalize();

v8::Isolate* FXJSE_Runtime_Create_Own();
void FXJSE_Runtime_Release(v8::Isolate* pIsolate);

void FXJSE_ThrowMessage(const CFX_ByteStringC& utf8Message);

#endif  // FXJS_FXJSE_H_
