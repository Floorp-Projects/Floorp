// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OVERRIDDEN_METHODS_H_
#define OVERRIDDEN_METHODS_H_

// Should warn about overriding of methods.
class BaseClass {
 public:
  virtual ~BaseClass() {}
  virtual void SomeMethod() = 0;
  virtual void SomeOtherMethod() = 0;
  virtual void SomeInlineMethod() = 0;
  virtual void SomeNonPureBaseMethod() {}
};

class InterimClass : public BaseClass {
  // Should not warn about pure virtual methods.
  virtual void SomeMethod() = 0;
};

namespace WebKit {
class WebKitObserver {
 public:
  virtual void WebKitModifiedSomething() {};
};
}  // namespace WebKit

namespace webkit_glue {
class WebKitObserverImpl : WebKit::WebKitObserver {
 public:
  virtual void WebKitModifiedSomething() {};
};
}  // namespace webkit_glue

class DerivedClass : public InterimClass,
                     public webkit_glue::WebKitObserverImpl {
 public:
  // Should not warn about destructors.
  virtual ~DerivedClass() {}
  // Should warn.
  virtual void SomeMethod();
  // Should not warn if marked as override.
  virtual void SomeOtherMethod() override;
  // Should warn for inline implementations.
  virtual void SomeInlineMethod() {}
  // Should not warn if overriding a method whose origin is WebKit.
  virtual void WebKitModifiedSomething();
  // Should warn if overridden method isn't pure.
  virtual void SomeNonPureBaseMethod() {}
};

#endif  // OVERRIDDEN_METHODS_H_
