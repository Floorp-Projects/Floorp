// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_JS_OBJECT_H_
#define FPDFSDK_JAVASCRIPT_JS_OBJECT_H_

#include <map>
#include <memory>

#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fxjs/fxjs_v8.h"

class CJS_EventContext;
class CJS_Object;
class CPDFSDK_FormFillEnvironment;

class CJS_EmbedObj {
 public:
  explicit CJS_EmbedObj(CJS_Object* pJSObject);
  virtual ~CJS_EmbedObj();

  CJS_Object* GetJSObject() const { return m_pJSObject; }

 protected:
  CJS_Object* const m_pJSObject;
};

class CJS_Object {
 public:
  explicit CJS_Object(v8::Local<v8::Object> pObject);
  virtual ~CJS_Object();

  void MakeWeak();
  void Dispose();

  virtual void InitInstance(IJS_Runtime* pIRuntime);

  v8::Local<v8::Object> ToV8Object() { return m_pV8Object.Get(m_pIsolate); }

  // Takes ownership of |pObj|.
  void SetEmbedObject(CJS_EmbedObj* pObj) { m_pEmbedObj.reset(pObj); }
  CJS_EmbedObj* GetEmbedObject() const { return m_pEmbedObj.get(); }

  v8::Isolate* GetIsolate() const { return m_pIsolate; }

 protected:
  std::unique_ptr<CJS_EmbedObj> m_pEmbedObj;
  v8::Global<v8::Object> m_pV8Object;
  v8::Isolate* m_pIsolate;
};


#endif  // FPDFSDK_JAVASCRIPT_JS_OBJECT_H_
