/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5StreamParserPtr_h
#define nsHtml5StreamParserPtr_h

#include "nsHtml5StreamParser.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/DocGroup.h"

class nsHtml5StreamParserReleaser : public mozilla::Runnable {
 private:
  nsHtml5StreamParser* mPtr;

 public:
  explicit nsHtml5StreamParserReleaser(nsHtml5StreamParser* aPtr)
      : mozilla::Runnable("nsHtml5StreamParserReleaser"), mPtr(aPtr) {}
  NS_IMETHOD Run() override {
    mPtr->Release();
    return NS_OK;
  }
};

/**
 * Like nsRefPtr except release is proxied to the main
 * thread. Mostly copied from nsRefPtr.
 */
class nsHtml5StreamParserPtr {
 private:
  void assign_with_AddRef(nsHtml5StreamParser* rawPtr) {
    if (rawPtr) rawPtr->AddRef();
    assign_assuming_AddRef(rawPtr);
  }
  void** begin_assignment() {
    assign_assuming_AddRef(0);
    return reinterpret_cast<void**>(&mRawPtr);
  }
  void assign_assuming_AddRef(nsHtml5StreamParser* newPtr) {
    nsHtml5StreamParser* oldPtr = mRawPtr;
    mRawPtr = newPtr;
    if (oldPtr) release(oldPtr);
  }
  void release(nsHtml5StreamParser* aPtr) {
    nsCOMPtr<nsIRunnable> releaser = new nsHtml5StreamParserReleaser(aPtr);
    if (NS_FAILED(aPtr->DispatchToMain(releaser.forget()))) {
      NS_WARNING("Failed to dispatch releaser event.");
    }
  }

 private:
  nsHtml5StreamParser* mRawPtr;

 public:
  ~nsHtml5StreamParserPtr() {
    if (mRawPtr) release(mRawPtr);
  }
  // Constructors
  nsHtml5StreamParserPtr()
      : mRawPtr(0)
  // default constructor
  {}
  nsHtml5StreamParserPtr(const nsHtml5StreamParserPtr& aSmartPtr)
      : mRawPtr(aSmartPtr.mRawPtr)
  // copy-constructor
  {
    if (mRawPtr) mRawPtr->AddRef();
  }
  explicit nsHtml5StreamParserPtr(nsHtml5StreamParser* aRawPtr)
      : mRawPtr(aRawPtr)
  // construct from a raw pointer (of the right type)
  {
    if (mRawPtr) mRawPtr->AddRef();
  }
  // Assignment operators
  nsHtml5StreamParserPtr& operator=(const nsHtml5StreamParserPtr& rhs)
  // copy assignment operator
  {
    assign_with_AddRef(rhs.mRawPtr);
    return *this;
  }
  nsHtml5StreamParserPtr& operator=(nsHtml5StreamParser* rhs)
  // assign from a raw pointer (of the right type)
  {
    assign_with_AddRef(rhs);
    return *this;
  }
  // Other pointer operators
  void swap(nsHtml5StreamParserPtr& rhs)
  // ...exchange ownership with |rhs|; can save a pair of refcount operations
  {
    nsHtml5StreamParser* temp = rhs.mRawPtr;
    rhs.mRawPtr = mRawPtr;
    mRawPtr = temp;
  }
  void swap(nsHtml5StreamParser*& rhs)
  // ...exchange ownership with |rhs|; can save a pair of refcount operations
  {
    nsHtml5StreamParser* temp = rhs;
    rhs = mRawPtr;
    mRawPtr = temp;
  }
  template <typename I>
  void forget(I** rhs)
  // Set the target of rhs to the value of mRawPtr and null out mRawPtr.
  // Useful to avoid unnecessary AddRef/Release pairs with "out"
  // parameters where rhs bay be a T** or an I** where I is a base class
  // of T.
  {
    NS_ASSERTION(rhs, "Null pointer passed to forget!");
    *rhs = mRawPtr;
    mRawPtr = 0;
  }
  nsHtml5StreamParser* get() const
  /*
            Prefer the implicit conversion provided automatically by |operator
     nsHtml5StreamParser*() const|. Use |get()| to resolve ambiguity or to get a
     castable pointer.
          */
  {
    return const_cast<nsHtml5StreamParser*>(mRawPtr);
  }
  operator nsHtml5StreamParser*() const
  /*
            ...makes an |nsHtml5StreamParserPtr| act like its underlying raw
     pointer type whenever it is used in a context where a raw pointer is
     expected.  It is this operator that makes an |nsHtml5StreamParserPtr|
     substitutable for a raw pointer. Prefer the implicit use of this operator
     to calling |get()|, except where necessary to resolve ambiguity.
          */
  {
    return get();
  }
  nsHtml5StreamParser* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsHtml5StreamParserPtr with "
               "operator->().");
    return get();
  }
  nsHtml5StreamParserPtr* get_address()
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }
  const nsHtml5StreamParserPtr* get_address() const
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }

 public:
  nsHtml5StreamParser& operator*() const {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsHtml5StreamParserPtr with "
               "operator*().");
    return *get();
  }
  nsHtml5StreamParser** StartAssignment() {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
    return reinterpret_cast<nsHtml5StreamParser**>(begin_assignment());
#else
    assign_assuming_AddRef(0);
    return reinterpret_cast<nsHtml5StreamParser**>(&mRawPtr);
#endif
  }
};

inline nsHtml5StreamParserPtr* address_of(nsHtml5StreamParserPtr& aPtr) {
  return aPtr.get_address();
}

inline const nsHtml5StreamParserPtr* address_of(
    const nsHtml5StreamParserPtr& aPtr) {
  return aPtr.get_address();
}

class nsHtml5StreamParserPtrGetterAddRefs
/*
      ...
      This class is designed to be used for anonymous temporary objects in the
      argument list of calls that return COM interface pointers, e.g.,
        nsHtml5StreamParserPtr<IFoo> fooP;
        ...->GetAddRefedPointer(getter_AddRefs(fooP))
      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()|
   instead. When initialized with a |nsHtml5StreamParserPtr|, as in the example
   above, it returns a |void**|, a |T**|, or an |nsISupports**| as needed, that
   the outer call (|GetAddRefedPointer| in this case) can fill in. This type
   should be a nested class inside |nsHtml5StreamParserPtr<T>|.
    */
{
 public:
  explicit nsHtml5StreamParserPtrGetterAddRefs(
      nsHtml5StreamParserPtr& aSmartPtr)
      : mTargetSmartPtr(aSmartPtr) {
    // nothing else to do
  }
  operator void**() {
    return reinterpret_cast<void**>(mTargetSmartPtr.StartAssignment());
  }
  operator nsHtml5StreamParser**() { return mTargetSmartPtr.StartAssignment(); }
  nsHtml5StreamParser*& operator*() {
    return *(mTargetSmartPtr.StartAssignment());
  }

 private:
  nsHtml5StreamParserPtr& mTargetSmartPtr;
};

inline nsHtml5StreamParserPtrGetterAddRefs getter_AddRefs(
    nsHtml5StreamParserPtr& aSmartPtr)
/*
      Used around a |nsHtml5StreamParserPtr| when
      ...makes the class |nsHtml5StreamParserPtrGetterAddRefs| invisible.
    */
{
  return nsHtml5StreamParserPtrGetterAddRefs(aSmartPtr);
}

// Comparing an |nsHtml5StreamParserPtr| to |0|

inline bool operator==(const nsHtml5StreamParserPtr& lhs, decltype(nullptr)) {
  return lhs.get() == nullptr;
}

inline bool operator==(decltype(nullptr), const nsHtml5StreamParserPtr& rhs) {
  return nullptr == rhs.get();
}

inline bool operator!=(const nsHtml5StreamParserPtr& lhs, decltype(nullptr)) {
  return lhs.get() != nullptr;
}

inline bool operator!=(decltype(nullptr), const nsHtml5StreamParserPtr& rhs) {
  return nullptr != rhs.get();
}

#endif  // !defined(nsHtml5StreamParserPtr_h)
