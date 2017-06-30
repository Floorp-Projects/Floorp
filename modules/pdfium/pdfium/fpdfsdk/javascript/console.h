// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_CONSOLE_H_
#define FPDFSDK_JAVASCRIPT_CONSOLE_H_

#include <vector>

#include "fpdfsdk/javascript/JS_Define.h"

class console : public CJS_EmbedObj {
 public:
  explicit console(CJS_Object* pJSObject);
  ~console() override;

 public:
  bool clear(CJS_Runtime* pRuntime,
             const std::vector<CJS_Value>& params,
             CJS_Value& vRet,
             CFX_WideString& sError);
  bool hide(CJS_Runtime* pRuntime,
            const std::vector<CJS_Value>& params,
            CJS_Value& vRet,
            CFX_WideString& sError);
  bool println(CJS_Runtime* pRuntime,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError);
  bool show(CJS_Runtime* pRuntime,
            const std::vector<CJS_Value>& params,
            CJS_Value& vRet,
            CFX_WideString& sError);
};

class CJS_Console : public CJS_Object {
 public:
  explicit CJS_Console(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Console() override {}

  DECLARE_JS_CLASS();

  JS_STATIC_METHOD(clear, console);
  JS_STATIC_METHOD(hide, console);
  JS_STATIC_METHOD(println, console);
  JS_STATIC_METHOD(show, console);
};

#endif  // FPDFSDK_JAVASCRIPT_CONSOLE_H_
