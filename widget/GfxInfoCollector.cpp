/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfoCollector.h"
#include "jsapi.h"
#include "nsString.h"

using namespace mozilla;
using namespace widget;

void
InfoObject::DefineProperty(const char *name, int value)
{
  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, mObj, name, value, JSPROP_ENUMERATE);
}

void
InfoObject::DefineProperty(const char *name, nsAString &value)
{
  if (!mOk)
    return;

  const nsString &flat = PromiseFlatString(value);
  JS::Rooted<JSString*> string(mCx, JS_NewUCStringCopyN(mCx, static_cast<const char16_t*>(flat.get()),
                                                        flat.Length()));
  if (!string)
    mOk = false;

  if (!mOk)
    return;

  mOk = JS_DefineProperty(mCx, mObj, name, string, JSPROP_ENUMERATE);
}

void
InfoObject::DefineProperty(const char *name, const char *value)
{
  nsAutoString string = NS_ConvertASCIItoUTF16(value);
  DefineProperty(name, string);
}

InfoObject::InfoObject(JSContext *aCx) : mCx(aCx), mObj(aCx), mOk(true)
{
  mObj = JS_NewObject(mCx, nullptr, JS::NullPtr(), JS::NullPtr());
  if (!mObj)
    mOk = false;
}
