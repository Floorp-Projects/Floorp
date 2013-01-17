/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSOBJECTBUILDER_H
#define JSOBJECTBUILDER_H

#include "JSAObjectBuilder.h"

class JSCustomObject;
class JSCustomObjectBuilder;
struct JSContext;
class nsAString;

/* this is handy wrapper around JSAPI to make it more pleasant to use.
 * We collect the JSAPI errors and so that callers don't need to */
class JSObjectBuilder : public JSAObjectBuilder
{
public:
  // We need to ensure that this object lives on the stack so that GC sees it properly
  explicit JSObjectBuilder(JSContext *aCx);
  ~JSObjectBuilder() {}

  void DefineProperty(JSCustomObject *aObject, const char *name, JSCustomObject *aValue);
  void DefineProperty(JSCustomObject *aObject, const char *name, JSCustomArray *aValue);
  void DefineProperty(JSCustomObject *aObject, const char *name, int value);
  void DefineProperty(JSCustomObject *aObject, const char *name, double value);
  void DefineProperty(JSCustomObject *aObject, const char *name, nsAString &value);
  void DefineProperty(JSCustomObject *aObject, const char *name, const char *value, size_t valueLength);
  void DefineProperty(JSCustomObject *aObject, const char *name, const char *value);
  void ArrayPush(JSCustomArray *aArray, int value);
  void ArrayPush(JSCustomArray *aArray, const char *value);
  void ArrayPush(JSCustomArray *aArray, JSCustomArray *aObject);
  void ArrayPush(JSCustomArray *aArray, JSCustomObject *aObject);
  JSCustomArray *CreateArray();
  JSCustomObject *CreateObject();

  JSObject* GetJSObject(JSCustomObject* aObject) { return (JSObject*)aObject; }

private:
  JSObjectBuilder(const JSObjectBuilder&);
  JSObjectBuilder& operator=(const JSObjectBuilder&);

  void* operator new(size_t);
  void* operator new[](size_t);
  void operator delete(void*) {
    // Since JSObjectBuilder has a virtual destructor the compiler
    // has to provide a destructor in the object file that will call
    // operate delete in case there is a derived class since its
    // destructor wont know how to free this instance.
    abort();
  }
  void operator delete[](void*);

  JSContext *mCx;
  JSObject *mObj;
  int mOk;
};

#endif

