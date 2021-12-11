/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringEnumerator_h
#define nsStringEnumerator_h

#include "nsIStringEnumerator.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

class nsStringEnumeratorBase : public nsIStringEnumerator,
                               public nsIUTF8StringEnumerator {
 public:
  NS_DECL_NSISTRINGENUMERATORBASE

  NS_IMETHOD GetNext(nsAString&) override;

  using nsIUTF8StringEnumerator::GetNext;

 protected:
  virtual ~nsStringEnumeratorBase() = default;
};

// nsIStringEnumerator/nsIUTF8StringEnumerator implementations
//
// Currently all implementations support both interfaces. The
// constructors below provide the most common interface for the given
// type (i.e. nsIStringEnumerator for char16_t* strings, and so
// forth) but any resulting enumerators can be queried to the other
// type. Internally, the enumerators will hold onto the type that was
// passed in and do conversion if GetNext() for the other type of
// string is called.

// There are a few different types of enumerators:

//
// These enumerators hold a pointer to the array. Be careful
// because modifying the array may confuse the iterator, especially if
// you insert or remove elements in the middle of the array.
//

// The non-adopting enumerator requires that the array sticks around
// at least as long as the enumerator does. These are for constant
// string arrays that the enumerator does not own, this could be used
// in VERY specialized cases such as when the provider KNOWS that the
// string enumerator will be consumed immediately, or will at least
// outlast the array.
// For example:
//
// nsTArray<nsCString> array;
// array.AppendCString("abc");
// array.AppendCString("def");
// NS_NewStringEnumerator(&enumerator, &array, true);
//
// // call some internal method which iterates the enumerator
// InternalMethod(enumerator);
// NS_RELEASE(enumerator);
//
[[nodiscard]] nsresult NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                                              const nsTArray<nsString>* aArray,
                                              nsISupports* aOwner);
[[nodiscard]] nsresult NS_NewUTF8StringEnumerator(
    nsIUTF8StringEnumerator** aResult, const nsTArray<nsCString>* aArray);

[[nodiscard]] nsresult NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                                              const nsTArray<nsString>* aArray);

// Adopting string enumerators assume ownership of the array and will
// call |operator delete| on the array when the enumerator is destroyed
// this is useful when the provider creates an array solely for the
// purpose of creating the enumerator.
// For example:
//
// nsTArray<nsCString>* array = new nsTArray<nsCString>;
// array->AppendString("abcd");
// NS_NewAdoptingStringEnumerator(&result, array);
[[nodiscard]] nsresult NS_NewAdoptingStringEnumerator(
    nsIStringEnumerator** aResult, nsTArray<nsString>* aArray);

[[nodiscard]] nsresult NS_NewAdoptingUTF8StringEnumerator(
    nsIUTF8StringEnumerator** aResult, nsTArray<nsCString>* aArray);

// these versions take a refcounted "owner" which will be addreffed
// when the enumerator is created, and destroyed when the enumerator
// is released. This allows providers to give non-owning pointers to
// ns*StringArray member variables without worrying about lifetime
// issues
// For example:
//
// nsresult MyClass::Enumerate(nsIUTF8StringEnumerator** aResult) {
//     mCategoryList->AppendString("abcd");
//     return NS_NewStringEnumerator(aResult, mCategoryList, this);
// }
//
[[nodiscard]] nsresult NS_NewUTF8StringEnumerator(
    nsIUTF8StringEnumerator** aResult, const nsTArray<nsCString>* aArray,
    nsISupports* aOwner);

#endif  // defined nsStringEnumerator_h
