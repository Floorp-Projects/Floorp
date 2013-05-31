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
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, JSCustomArray *aValue)
{
  DefineProperty(aObject, name, (JSCustomObject*)aValue);
}

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, JSCustomObject *aValue)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, OBJECT_TO_JSVAL((JSObject*)aValue), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, int value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, INT_TO_JSVAL(value), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, double value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, DOUBLE_TO_JSVAL(value), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, nsAString &value)
{
  if (!mOk)
    return;

  const nsString &flat = PromiseFlatString(value);
  JSString *string = JS_NewUCStringCopyN(mCx, static_cast<const jschar*>(flat.get()), flat.Length());
  if (!string)
    mOk = JS_FALSE;

  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, STRING_TO_JSVAL(string), nullptr, nullptr, JSPROP_ENUMERATE);
}

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, const char *value, size_t valueLength)
{
  if (!mOk)
    return;

  JSString *string = JS_InternStringN(mCx, value, valueLength);
  if (!string) {
    mOk = JS_FALSE;
    return;
  }

  mOk = JS_DefineProperty(mCx, (JSObject*)aObject, name, STRING_TO_JSVAL(string), nullptr, nullptr, JSPROP_ENUMERATE); }

void
JSObjectBuilder::DefineProperty(JSCustomObject *aObject, const char *name, const char *value)
{
  DefineProperty(aObject, name, value, strlen(value));
}

void
JSObjectBuilder::ArrayPush(JSCustomArray *aArray, int value)
{
  if (!mOk)
    return;

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, (JSObject*)aArray, &length);

  if (!mOk)
    return;

  JS::Rooted<JS::Value> objval(mCx, INT_TO_JSVAL(value));
  mOk = JS_SetElement(mCx, (JSObject*)aArray, length, objval.address());
}

void
JSObjectBuilder::ArrayPush(JSCustomArray *aArray, const char *value)
{
  if (!mOk)
    return;

  JS::RootedString string(mCx, JS_NewStringCopyN(mCx, value, strlen(value)));
  if (!string) {
    mOk = JS_FALSE;
    return;
  }

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, (JSObject*)aArray, &length);

  if (!mOk)
    return;

  JS::Rooted<JS::Value> objval(mCx, STRING_TO_JSVAL(string));
  mOk = JS_SetElement(mCx, (JSObject*)aArray, length, objval.address());
}

void
JSObjectBuilder::ArrayPush(JSCustomArray *aArray, JSCustomObject *aObject)
{
  if (!mOk)
    return;

  uint32_t length;
  mOk = JS_GetArrayLength(mCx, (JSObject*)aArray, &length);

  if (!mOk)
    return;

  JS::Rooted<JS::Value> objval(mCx, OBJECT_TO_JSVAL((JSObject*)aObject));
  mOk = JS_SetElement(mCx, (JSObject*)aArray, length, objval.address());
}

JSCustomArray*
JSObjectBuilder::CreateArray() {
  JSCustomArray *array = (JSCustomArray*)JS_NewArrayObject(mCx, 0, nullptr);
  if (!array)
    mOk = JS_FALSE;

  return array;
}

JSCustomObject*
JSObjectBuilder::CreateObject() {
  JSCustomObject *obj = (JSCustomObject*)JS_NewObject(mCx, nullptr, nullptr, nullptr);
  if (!obj)
    mOk = JS_FALSE;

  return obj;
}

