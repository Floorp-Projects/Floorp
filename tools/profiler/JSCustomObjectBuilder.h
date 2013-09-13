/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSCUSTOMOBJECTBUILDER_H
#define JSCUSTOMOBJECTBUILDER_H

#include <ostream>
#include <stdlib.h>
#include "js/RootingAPI.h"

class JSCustomObject;
class JSCustomArray;

class JSCustomObjectBuilder
{
public:
  typedef JSCustomObject* Object;
  typedef JSCustomArray* Array;
  typedef JSCustomObject* ObjectHandle;
  typedef JSCustomArray* ArrayHandle;
  typedef js::FakeRooted<JSCustomObject*> RootedObject;
  typedef js::FakeRooted<JSCustomArray*> RootedArray;

  // We need to ensure that this object lives on the stack so that GC sees it properly
  JSCustomObjectBuilder();

  void Serialize(JSCustomObject* aObject, std::ostream& stream);

  void DefineProperty(JSCustomObject *aObject, const char *name, JSCustomObject *aValue);
  void DefineProperty(JSCustomObject *aObject, const char *name, JSCustomArray *aValue);
  void DefineProperty(JSCustomObject *aObject, const char *name, int value);
  void DefineProperty(JSCustomObject *aObject, const char *name, double value);
  void DefineProperty(JSCustomObject *aObject, const char *name, const char *value, size_t valueLength);
  void DefineProperty(JSCustomObject *aObject, const char *name, const char *value);
  void ArrayPush(JSCustomArray *aArray, int value);
  void ArrayPush(JSCustomArray *aArray, const char *value);
  void ArrayPush(JSCustomArray *aArray, JSCustomObject *aObject);
  JSCustomArray  *CreateArray();
  JSCustomObject *CreateObject();

  // Delete this object and all of its descendant
  void DeleteObject(JSCustomObject* aObject);

  JSContext *context() const { return nullptr; }

private:
  // This class can't be copied
  JSCustomObjectBuilder(const JSCustomObjectBuilder&);
  JSCustomObjectBuilder& operator=(const JSCustomObjectBuilder&);

  void* operator new(size_t);
  void* operator new[](size_t);
  void operator delete(void*) {
    // Since JSCustomObjectBuilder has a virtual destructor the compiler
    // has to provide a destructor in the object file that will call
    // operate delete in case there is a derived class since its
    // destructor wont know how to free this instance.
    abort();
  }
  void operator delete[](void*);
};

#endif
