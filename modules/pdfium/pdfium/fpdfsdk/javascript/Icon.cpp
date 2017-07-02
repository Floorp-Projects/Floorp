// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/Icon.h"

#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"

JSConstSpec CJS_Icon::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Icon::PropertySpecs[] = {
    {"name", get_name_static, set_name_static},
    {0, 0, 0}};

JSMethodSpec CJS_Icon::MethodSpecs[] = {{0, 0}};

IMPLEMENT_JS_CLASS(CJS_Icon, Icon)

Icon::Icon(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_swIconName(L"") {}

Icon::~Icon() {}

bool Icon::name(CJS_Runtime* pRuntime,
                CJS_PropValue& vp,
                CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  vp << m_swIconName;
  return true;
}
