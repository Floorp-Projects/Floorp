/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Muizelaar <jmuizelaar@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsapi.h"

/* this is handy wrapper around JSAPI to make it more pleasant to use.
 * We collect the JSAPI errors and so that callers don't need to */
class JSObjectBuilder
{
  public:

  void DefineProperty(JSObject *aObject, const char *name, JSObject *aValue)
  {
    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, aObject, name, OBJECT_TO_JSVAL(aValue), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(JSObject *aObject, const char *name, int value)
  {
    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, aObject, name, INT_TO_JSVAL(value), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(JSObject *aObject, const char *name, double value)
  {
    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, aObject, name, DOUBLE_TO_JSVAL(value), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(JSObject *aObject, const char *name, nsAString &value)
  {
    if (!mOk)
      return;

    const nsString &flat = PromiseFlatString(value);
    JSString *string = JS_NewUCStringCopyN(mCx, static_cast<const jschar*>(flat.get()), flat.Length());
    if (!string)
      mOk = JS_FALSE;

    if (!mOk)
      return;

    mOk = JS_DefineProperty(mCx, aObject, name, STRING_TO_JSVAL(string), NULL, NULL, JSPROP_ENUMERATE);
  }

  void DefineProperty(JSObject *aObject, const char *name, const char *value)
  {
    nsAutoString string = NS_ConvertASCIItoUTF16(value);
    DefineProperty(aObject, name, string);
  }

  void ArrayPush(JSObject *aArray, int value)
  {
    if (!mOk)
      return;

    jsval objval = INT_TO_JSVAL(value);
    uint32_t length;
    mOk = JS_GetArrayLength(mCx, aArray, &length);

    if (!mOk)
      return;

    mOk = JS_SetElement(mCx, aArray, length, &objval);
  }


  void ArrayPush(JSObject *aArray, JSObject *aObject)
  {
    if (!mOk)
      return;

    jsval objval = OBJECT_TO_JSVAL(aObject);
    uint32_t length;
    mOk = JS_GetArrayLength(mCx, aArray, &length);

    if (!mOk)
      return;

    mOk = JS_SetElement(mCx, aArray, length, &objval);
  }

  JSObject *CreateArray() {
    JSObject *array = JS_NewArrayObject(mCx, 0, NULL);
    if (!array)
      mOk = JS_FALSE;

    return array;
  }

  JSObject *CreateObject() {
    JSObject *obj = JS_NewObject(mCx, NULL, NULL, NULL);
    if (!obj)
      mOk = JS_FALSE;

    return obj;
  }


  // We need to ensure that this object lives on the stack so that GC sees it properly
  JSObjectBuilder(JSContext *aCx) : mCx(aCx), mOk(JS_TRUE)
  {
  }
  private:
  JSObjectBuilder(JSObjectBuilder&);

  JSContext *mCx;
  JSObject *mObj;
  JSBool mOk;
};


