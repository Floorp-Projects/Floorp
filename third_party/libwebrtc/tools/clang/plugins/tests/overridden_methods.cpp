// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "overridden_methods.h"

// Fill in the implementations
void DerivedClass::SomeMethod() {}
void DerivedClass::SomeOtherMethod() {}
void DerivedClass::WebKitModifiedSomething() {}

class ImplementationInterimClass : public BaseClass {
 public:
  // Should not warn about pure virtual methods.
  virtual void SomeMethod() = 0;
};

class ImplementationDerivedClass : public ImplementationInterimClass,
                                   public webkit_glue::WebKitObserverImpl {
 public:
  // Should not warn about destructors.
  virtual ~ImplementationDerivedClass() {}
  // Should warn.
  virtual void SomeMethod();
  // Should not warn if marked as override.
  virtual void SomeOtherMethod() override;
  // Should not warn for inline implementations in implementation files.
  virtual void SomeInlineMethod() {}
  // Should not warn if overriding a method whose origin is WebKit.
  virtual void WebKitModifiedSomething();
  // Should warn if overridden method isn't pure.
  virtual void SomeNonPureBaseMethod() {}
};

int main() {
  DerivedClass something;
  ImplementationDerivedClass something_else;
}
