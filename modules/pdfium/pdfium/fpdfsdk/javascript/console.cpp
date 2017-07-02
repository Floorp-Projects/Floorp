// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/console.h"

#include <vector>

#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_event_context.h"

JSConstSpec CJS_Console::ConstSpecs[] = {{0, JSConstSpec::Number, 0, 0}};

JSPropertySpec CJS_Console::PropertySpecs[] = {{0, 0, 0}};

JSMethodSpec CJS_Console::MethodSpecs[] = {{"clear", clear_static},
                                           {"hide", hide_static},
                                           {"println", println_static},
                                           {"show", show_static},
                                           {0, 0}};

IMPLEMENT_JS_CLASS(CJS_Console, console)

console::console(CJS_Object* pJSObject) : CJS_EmbedObj(pJSObject) {}

console::~console() {}

bool console::clear(CJS_Runtime* pRuntime,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  return true;
}

bool console::hide(CJS_Runtime* pRuntime,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError) {
  return true;
}

bool console::println(CJS_Runtime* pRuntime,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  if (params.size() < 1) {
    return false;
  }
  return true;
}

bool console::show(CJS_Runtime* pRuntime,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError) {
  return true;
}
