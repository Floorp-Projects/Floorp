// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_ICON_H_
#define FPDFSDK_JAVASCRIPT_ICON_H_

#include "fpdfsdk/javascript/JS_Define.h"

class CPDF_Stream;

class Icon : public CJS_EmbedObj {
 public:
  explicit Icon(CJS_Object* pJSObject);
  ~Icon() override;

  bool name(CJS_Runtime* pRuntime, CJS_PropValue& vp, CFX_WideString& sError);
  CFX_WideString GetIconName() const { return m_swIconName; }
  void SetIconName(CFX_WideString name) { m_swIconName = name; }

 private:
  CFX_WideString m_swIconName;
};

class CJS_Icon : public CJS_Object {
 public:
  explicit CJS_Icon(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Icon() override {}

  DECLARE_JS_CLASS();
  JS_STATIC_PROP(name, Icon);
};

#endif  // FPDFSDK_JAVASCRIPT_ICON_H_
