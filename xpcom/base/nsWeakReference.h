/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWeakReference_h__
#define nsWeakReference_h__

// nsWeakReference.h

// See mfbt/WeakPtr.h for a more typesafe C++ implementation of weak references

#include "nsIWeakReferenceUtils.h"

class nsWeakReference;

class nsSupportsWeakReference : public nsISupportsWeakReference {
 public:
  nsSupportsWeakReference() : mProxy(0) {}

  NS_DECL_NSISUPPORTSWEAKREFERENCE

 protected:
  inline ~nsSupportsWeakReference();

 private:
  friend class nsWeakReference;

  // Called (only) by an |nsWeakReference| from _its_ dtor.
  // The thread safety check is made by the caller.
  void NoticeProxyDestruction() { mProxy = nullptr; }

  nsWeakReference* MOZ_NON_OWNING_REF mProxy;

 protected:
  void ClearWeakReferences();
  bool HasWeakReferences() const { return !!mProxy; }
};

inline nsSupportsWeakReference::~nsSupportsWeakReference() {
  ClearWeakReferences();
}

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE \
  tmp->ClearWeakReferences();

#define NS_IMPL_CYCLE_COLLECTION_WEAK(class_, ...) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(class_)           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(class_)    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)   \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END              \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(class_)  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__) \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_WEAK_INHERITED(class_, super_, ...) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(class_)                             \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(class_, super_)    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                     \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE                   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(class_, super_)  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                   \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#endif
