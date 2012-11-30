/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class JSObject;
class JSObjectBuilder;
class nsAString;

/* this is handy wrapper around JSAPI to make it more pleasant to use.
 * We collect the JSAPI errors and so that callers don't need to */
class JSObjectBuilder
{
public:

  // We need to ensure that this object lives on the stack so that GC sees it properly
  JSObjectBuilder(JSContext *aCx);

  void DefineProperty(JSObject *aObject, const char *name, JSObject *aValue);
  void DefineProperty(JSObject *aObject, const char *name, int value);
  void DefineProperty(JSObject *aObject, const char *name, double value);
  void DefineProperty(JSObject *aObject, const char *name, nsAString &value);
  void DefineProperty(JSObject *aObject, const char *name, const char *value, size_t valueLength);
  void DefineProperty(JSObject *aObject, const char *name, const char *value);
  void ArrayPush(JSObject *aArray, int value);
  void ArrayPush(JSObject *aArray, const char *value);
  void ArrayPush(JSObject *aArray, JSObject *aObject);
  JSObject *CreateArray();
  JSObject *CreateObject();

private:
  JSObjectBuilder(JSObjectBuilder&);

  JSContext *mCx;
  JSObject *mObj;
  int mOk;
};


