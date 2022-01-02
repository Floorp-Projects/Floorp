/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHashPropertyBag_h___
#define nsHashPropertyBag_h___

#include "nsIVariant.h"
#include "nsIWritablePropertyBag.h"
#include "nsIWritablePropertyBag2.h"

#include "nsCycleCollectionParticipant.h"
#include "nsInterfaceHashtable.h"

class nsHashPropertyBagBase : public nsIWritablePropertyBag,
                              public nsIWritablePropertyBag2 {
 public:
  nsHashPropertyBagBase() = default;

  void CopyFrom(const nsHashPropertyBagBase* aOther);
  void CopyFrom(nsIPropertyBag* aOther);
  static void CopyFrom(nsIWritablePropertyBag* aTo, nsIPropertyBag* aFrom);

  NS_DECL_NSIPROPERTYBAG
  NS_DECL_NSIPROPERTYBAG2

  NS_DECL_NSIWRITABLEPROPERTYBAG
  NS_DECL_NSIWRITABLEPROPERTYBAG2

 protected:
  // a hash table of string -> nsIVariant
  nsInterfaceHashtable<nsStringHashKey, nsIVariant> mPropertyHash;
};

class nsHashPropertyBag : public nsHashPropertyBagBase {
 public:
  nsHashPropertyBag() = default;
  NS_DECL_THREADSAFE_ISUPPORTS

 protected:
  virtual ~nsHashPropertyBag();
};

/*
 * Off Main Thread variant of nsHashPropertyBag. Instances of this class
 * should not be created on a main thread, nor should it contain main thread
 * only objects, such as XPCVariants. The purpose of this class is to provide a
 * way to use the property bag off main thread.
 * Note: this class needs to be created and destroyed on the same thread and
 * should be used single threaded.
 */
class nsHashPropertyBagOMT final : public nsHashPropertyBagBase {
 public:
  nsHashPropertyBagOMT();
  NS_DECL_ISUPPORTS

 protected:
  // Doesn't need to dispatch to main thread because it cannot contain
  // XPCVariants
  virtual ~nsHashPropertyBagOMT() = default;
};

/* A cycle collected nsHashPropertyBag for main-thread-only use. */
class nsHashPropertyBagCC final : public nsHashPropertyBagBase {
 public:
  nsHashPropertyBagCC() = default;
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsHashPropertyBagCC,
                                           nsIWritablePropertyBag)
 protected:
  virtual ~nsHashPropertyBagCC() = default;
};

inline nsISupports* ToSupports(nsHashPropertyBagBase* aPropertyBag) {
  return static_cast<nsIWritablePropertyBag*>(aPropertyBag);
}

#endif /* nsHashPropertyBag_h___ */
