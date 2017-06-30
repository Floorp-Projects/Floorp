// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_CONSTS_H_
#define FPDFSDK_JAVASCRIPT_CONSTS_H_

#include "fpdfsdk/javascript/JS_Define.h"

class CJS_Border : public CJS_Object {
 public:
  explicit CJS_Border(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Border() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Display : public CJS_Object {
 public:
  explicit CJS_Display(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Display() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Font : public CJS_Object {
 public:
  explicit CJS_Font(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Font() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Highlight : public CJS_Object {
 public:
  explicit CJS_Highlight(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Highlight() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Position : public CJS_Object {
 public:
  explicit CJS_Position(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Position() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_ScaleHow : public CJS_Object {
 public:
  explicit CJS_ScaleHow(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_ScaleHow() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_ScaleWhen : public CJS_Object {
 public:
  explicit CJS_ScaleWhen(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_ScaleWhen() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Style : public CJS_Object {
 public:
  explicit CJS_Style(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Style() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_Zoomtype : public CJS_Object {
 public:
  explicit CJS_Zoomtype(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Zoomtype() override {}

  DECLARE_JS_CLASS_CONST();
};

class CJS_GlobalConsts : public CJS_Object {
 public:
  static void DefineJSObjects(CJS_Runtime* pRuntime);
};

class CJS_GlobalArrays : public CJS_Object {
 public:
  static void DefineJSObjects(CJS_Runtime* pRuntmie);
};

#endif  // FPDFSDK_JAVASCRIPT_CONSTS_H_
