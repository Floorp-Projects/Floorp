/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SimpleEnumerator_h
#define mozilla_SimpleEnumerator_h

#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"

namespace mozilla {

/**
 * A wrapper class around nsISimpleEnumerator to support ranged iteration. This
 * requires every element in the enumeration to implement the same interface, T.
 * If any element does not implement this interface, the enumeration ends at
 * that element, and triggers an assertion in debug builds.
 *
 * Typical usage looks something like:
 *
 *   for (auto& docShell : SimpleEnumerator<nsIDocShell>(docShellEnum)) {
 *     docShell.LoadURI(...);
 *   }
 */

template <typename T>
class SimpleEnumerator final {
 public:
  explicit SimpleEnumerator(nsISimpleEnumerator* aEnum) : mEnum(aEnum) {}

  class Entry {
   public:
    explicit Entry(T* aPtr) : mPtr(aPtr) {}

    explicit Entry(nsISimpleEnumerator& aEnum) : mEnum(&aEnum) { ++*this; }

    const nsCOMPtr<T>& operator*() {
      MOZ_ASSERT(mPtr);
      return mPtr;
    }

    Entry& operator++() {
      MOZ_ASSERT(mEnum);
      nsCOMPtr<nsISupports> next;
      if (NS_SUCCEEDED(mEnum->GetNext(getter_AddRefs(next)))) {
        mPtr = do_QueryInterface(next);
        MOZ_ASSERT(mPtr);
      } else {
        mPtr = nullptr;
      }
      return *this;
    }

    bool operator!=(const Entry& aOther) const { return mPtr != aOther.mPtr; }

   private:
    nsCOMPtr<T> mPtr;
    nsCOMPtr<nsISimpleEnumerator> mEnum;
  };

  Entry begin() { return Entry(*mEnum); }

  Entry end() { return Entry(nullptr); }

 private:
  nsCOMPtr<nsISimpleEnumerator> mEnum;
};

}  // namespace mozilla

#endif  // mozilla_SimpleEnumerator_h
