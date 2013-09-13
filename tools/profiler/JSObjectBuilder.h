/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSOBJECTBUILDER_H
#define JSOBJECTBUILDER_H

#include "js/TypeDecls.h"
#include "js/RootingAPI.h"

class JSCustomArray;
class JSCustomObject;
class JSCustomObjectBuilder;
class nsAString;

/* this is handy wrapper around JSAPI to make it more pleasant to use.
 * We collect the JSAPI errors and so that callers don't need to */
class JSObjectBuilder
{
public:
  typedef JS::Handle<JSObject*> ObjectHandle;
  typedef JS::Handle<JSObject*> ArrayHandle;
  typedef JS::Rooted<JSObject*> RootedObject;
  typedef JS::Rooted<JSObject*> RootedArray;
  typedef JSObject* Object;
  typedef JSObject* Array;

  // We need to ensure that this object lives on the stack so that GC sees it properly
  explicit JSObjectBuilder(JSContext *aCx);
  ~JSObjectBuilder() {}

  void DefineProperty(JS::HandleObject aObject, const char *name, JS::HandleObject aValue);
  void DefineProperty(JS::HandleObject aObject, const char *name, int value);
  void DefineProperty(JS::HandleObject aObject, const char *name, double value);
  void DefineProperty(JS::HandleObject aObject, const char *name, nsAString &value);
  void DefineProperty(JS::HandleObject aObject, const char *name, const char *value, size_t valueLength);
  void DefineProperty(JS::HandleObject aObject, const char *name, const char *value);
  void ArrayPush(JS::HandleObject aArray, int value);
  void ArrayPush(JS::HandleObject aArray, const char *value);
  void ArrayPush(JS::HandleObject aArray, JS::HandleObject aObject);
  JSObject *CreateArray();
  JSObject *CreateObject();

  JSContext *context() const { return mCx; }

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
  int mOk;
};

#endif

