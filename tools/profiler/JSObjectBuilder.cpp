/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsStringGlue.h"
#include "JSObjectBuilder.h"

JSObjectBuilder::JSObjectBuilder(JSContext *aCx) : mCx(aCx), mOk(true)
{}

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, JS::HandleObject aValue)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, OBJECT_TO_JSVAL(aValue), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, int value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, INT_TO_JSVAL(value), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, double value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, DOUBLE_TO_JSVAL(value), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, nsAString &value)
{
  if (!mOk)
    return;

  const nsString &flat = PromiseFlatString(value);
  JSString *string = JS_NewUCStringCopyN(mCx, static_cast<const jschar*>(flat.get()), flat.Length());
  if (!string)
    mOk = false;

  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, aObject, name, STRING_TO_JSVAL(string), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, const char *value, size_t valueLength)
{
  if (!mOk)
    return;

  JSString *string = JS_InternStringN(mCx, value, valueLength);
  if (!string) {
    mOk = false;
    return;
  }

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, STRING_TO_JSVAL(string), nullptr, nullptr, JSPROP_ENUMERATE); }

void
JSObjectBuilder::DefineProperty(JS::HandleObject aObject, const char *name, const char *value)
{
  DefineProperty(aObject, name, value, strlen(value));
}

void
JSObjectBuilder::ArrayPush(JS::HandleObject aArray, int value)
{
  if (!mOk)
    return;

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, (JSObject*)aArray, &length);

  if (!mOk)
    return;

  JS::RootedValue objval(mCx, INT_TO_JSVAL(value));
  mOk = JS_SetElement(mCx, aArray, length, &objval);
}

void
JSObjectBuilder::ArrayPush(JS::HandleObject aArray, const char *value)
{
  if (!mOk)
    return;

  JS::RootedString string(mCx, JS_NewStringCopyN(mCx, value, strlen(value)));
  if (!string) {
    mOk = false;
    return;
  }

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, (JSObject*)aArray, &length);

  if (!mOk)
    return;

  JS::RootedValue objval(mCx, STRING_TO_JSVAL(string));
  mOk = JS_SetElement(mCx, aArray, length, &objval);
}

void
JSObjectBuilder::ArrayPush(JS::HandleObject aArray, JS::HandleObject aObject)
{
  if (!mOk)
    return;

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, aArray, &length);

  if (!mOk)
    return;

  JS::RootedValue objval(mCx, OBJECT_TO_JSVAL(aObject));
  mOk = JS_SetElement(mCx, aArray, length, &objval);
}

JSObject*
JSObjectBuilder::CreateArray() {
  JSObject *array = JS_NewArrayObject(mCx, 0, nullptr);
  if (!array)
    mOk = false;

  return array;
}

JSObject*
JSObjectBuilder::CreateObject() {
  JSObject *obj = JS_NewObject(mCx, nullptr, nullptr, nullptr);
  if (!obj)
    mOk = false;

  return obj;
}

