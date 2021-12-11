/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArray_h__
#define nsArray_h__

#include "nsIMutableArray.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

// {35C66FD1-95E9-4e0a-80C5-C3BD2B375481}
#define NS_ARRAY_CID                                 \
  {                                                  \
    0x35c66fd1, 0x95e9, 0x4e0a, {                    \
      0x80, 0xc5, 0xc3, 0xbd, 0x2b, 0x37, 0x54, 0x81 \
    }                                                \
  }

// nsArray without any refcounting declarations
class nsArrayBase : public nsIMutableArray {
 public:
  NS_DECL_NSIARRAY
  NS_DECL_NSIARRAYEXTENSIONS
  NS_DECL_NSIMUTABLEARRAY

  /* Both of these factory functions create a cycle-collectable array
     on the main thread and a non-cycle-collectable array on other
     threads.  */
  static already_AddRefed<nsIMutableArray> Create();
  /* Only for the benefit of the XPCOM module system, use Create()
     instead.  */
  static nsresult XPCOMConstructor(nsISupports* aOuter, const nsIID& aIID,
                                   void** aResult);

 protected:
  nsArrayBase() = default;
  nsArrayBase(const nsArrayBase& aOther);
  explicit nsArrayBase(const nsCOMArray_base& aBaseArray)
      : mArray(aBaseArray) {}
  virtual ~nsArrayBase();

  nsCOMArray_base mArray;
};

class nsArray final : public nsArrayBase {
  friend class nsArrayBase;

 public:
  NS_DECL_ISUPPORTS

 private:
  nsArray() : nsArrayBase() {}
  nsArray(const nsArray& aOther);
  explicit nsArray(const nsCOMArray_base& aBaseArray)
      : nsArrayBase(aBaseArray) {}
  ~nsArray() = default;
};

class nsArrayCC final : public nsArrayBase {
  friend class nsArrayBase;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsArrayCC, nsIMutableArray)

 private:
  nsArrayCC() : nsArrayBase() {}
  nsArrayCC(const nsArrayCC& aOther);
  explicit nsArrayCC(const nsCOMArray_base& aBaseArray)
      : nsArrayBase(aBaseArray) {}
  ~nsArrayCC() = default;
};

#endif
