/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsStringGlue.h"
#include "JSObjectBuilder.h"

JSObjectBuilder::JSObjectBuilder(JSContext *aCx) : mCx(aCx), mOk(JS_TRUE)
{}

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, JSObject *aValue)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, OBJECT_TO_JSVAL(aValue), NULL, NULL, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, int value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, INT_TO_JSVAL(value), NULL, NULL, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, double value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, DOUBLE_TO_JSVAL(value), NULL, NULL, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, nsAString &value)
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

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, const char *value, size_t valueLength)
{
  if (!mOk)
    return;

  JSString *string = JS_InternStringN(mCx, value, valueLength);
  if (!string) {
    mOk = JS_FALSE;
    return;
  }

  mOk = JS_DefineProperty(mCx, aObject, name, STRING_TO_JSVAL(string), NULL, NULL, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSObject *aObject, const char *name, const char *value)
{
  DefineProperty(aObject, name, value, strlen(value));
}

void
JSObjectBuilder::ArrayPush(JSObject *aArray, int value)
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

void
JSObjectBuilder::ArrayPush(JSObject *aArray, const char *value)
{
  if (!mOk)
    return;

  JSString *string = JS_NewStringCopyN(mCx, value, strlen(value));
  if (!string) {
    mOk = JS_FALSE;
    return;
  }

  jsval objval = STRING_TO_JSVAL(string);
  uint32_t length;
  mOk = JS_GetArrayLength(mCx, aArray, &length);

  if (!mOk)
    return;

  mOk = JS_SetElement(mCx, aArray, length, &objval);
}

void
JSObjectBuilder::ArrayPush(JSObject *aArray, JSObject *aObject)
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

JSObject*
JSObjectBuilder::CreateArray() {
  JSObject *array = JS_NewArrayObject(mCx, 0, NULL);
  if (!array)
    mOk = JS_FALSE;

  return array;
}

JSObject*
JSObjectBuilder::CreateObject() {
  JSObject *obj = JS_NewObject(mCx, NULL, NULL, NULL);
  if (!obj)
    mOk = JS_FALSE;

  return obj;
}

