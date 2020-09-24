// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_REFCOUNTED_H_
#define BASE_REFCOUNTED_H_

namespace base {

template <typename T>
class RefCounted {
 public:
  RefCounted() {}
  ~RefCounted() {}
};

template <typename T>
class RefCountedThreadSafe {
 public:
  RefCountedThreadSafe() {}
  ~RefCountedThreadSafe() {}
};

}  // namespace base

// Ignore classes whose inheritance tree ends in WebKit's RefCounted base
// class. Though prone to error, this pattern is very prevalent in WebKit
// code, so do not issue any warnings.
namespace WebKit {

template <typename T>
class RefCounted {
 public:
  RefCounted() {}
  ~RefCounted() {}
};

}  // namespace WebKit

// Unsafe; should error.
class PublicRefCountedDtorInHeader
    : public base::RefCounted<PublicRefCountedDtorInHeader> {
 public:
  PublicRefCountedDtorInHeader() {}
  ~PublicRefCountedDtorInHeader() {}

 private:
  friend class base::RefCounted<PublicRefCountedDtorInHeader>;
};

// Unsafe; should error.
class PublicRefCountedThreadSafeDtorInHeader
    : public base::RefCountedThreadSafe<
          PublicRefCountedThreadSafeDtorInHeader> {
 public:
  PublicRefCountedThreadSafeDtorInHeader() {}
  ~PublicRefCountedThreadSafeDtorInHeader() {}

 private:
  friend class base::RefCountedThreadSafe<
      PublicRefCountedThreadSafeDtorInHeader>;
};

// Safe; should not have errors.
class ProtectedRefCountedDtorInHeader
    : public base::RefCounted<ProtectedRefCountedDtorInHeader> {
 public:
  ProtectedRefCountedDtorInHeader() {}

 protected:
  ~ProtectedRefCountedDtorInHeader() {}

 private:
  friend class base::RefCounted<ProtectedRefCountedDtorInHeader>;
};

// Safe; should not have errors.
class PrivateRefCountedDtorInHeader
    : public base::RefCounted<PrivateRefCountedDtorInHeader> {
 public:
  PrivateRefCountedDtorInHeader() {}

 private:
  ~PrivateRefCountedDtorInHeader() {}
  friend class base::RefCounted<PrivateRefCountedDtorInHeader>;
};

// Unsafe; A grandchild class ends up exposing their parent and grandparent's
// destructors.
class DerivedProtectedToPublicInHeader
    : public ProtectedRefCountedDtorInHeader {
 public:
  DerivedProtectedToPublicInHeader() {}
  ~DerivedProtectedToPublicInHeader() {}
};

// Unsafe; A grandchild ends up implicitly exposing their parent and
// grantparent's destructors.
class ImplicitDerivedProtectedToPublicInHeader
    : public ProtectedRefCountedDtorInHeader {
 public:
  ImplicitDerivedProtectedToPublicInHeader() {}
};

// Unsafe-but-ignored; should not have errors.
class WebKitPublicDtorInHeader
    : public WebKit::RefCounted<WebKitPublicDtorInHeader> {
 public:
  WebKitPublicDtorInHeader() {}
  ~WebKitPublicDtorInHeader() {}
};

// Unsafe-but-ignored; should not have errors.
class WebKitDerivedPublicDtorInHeader
    : public WebKitPublicDtorInHeader {
 public:
  WebKitDerivedPublicDtorInHeader() {}
  ~WebKitDerivedPublicDtorInHeader() {}
};

#endif  // BASE_REFCOUNTED_H_
