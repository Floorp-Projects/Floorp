/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringEnumerator.h"
#include "nsSimpleEnumerator.h"
#include "nsSupportsPrimitives.h"
#include "mozilla/Attributes.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"
#include "mozilla/dom/IteratorResultBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

class JSStringEnumerator final : public nsIJSEnumerator {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSENUMERATOR

  explicit JSStringEnumerator(nsIStringEnumerator* aEnumerator)
      : mEnumerator(aEnumerator) {
    MOZ_ASSERT(mEnumerator);
  }

 private:
  ~JSStringEnumerator() = default;

  nsCOMPtr<nsIStringEnumerator> mEnumerator;
};

}  // anonymous namespace

nsresult JSStringEnumerator::Iterator(nsIJSEnumerator** aResult) {
  RefPtr<JSStringEnumerator> result(this);
  result.forget(aResult);
  return NS_OK;
}

nsresult JSStringEnumerator::Next(JSContext* aCx,
                                  JS::MutableHandleValue aResult) {
  RootedDictionary<IteratorResult> result(aCx);

  nsAutoString elem;
  if (NS_FAILED(mEnumerator->GetNext(elem))) {
    result.mDone = true;
  } else {
    result.mDone = false;

    if (!ToJSValue(
            aCx, elem,
            JS::MutableHandleValue::fromMarkedLocation(&result.mValue))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!ToJSValue(aCx, result, aResult)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(JSStringEnumerator, nsIJSEnumerator)

//
// nsStringEnumeratorBase
//

nsresult nsStringEnumeratorBase::GetNext(nsAString& aResult) {
  nsAutoCString str;
  MOZ_TRY(GetNext(str));

  CopyUTF8toUTF16(str, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsStringEnumeratorBase::StringIterator(nsIJSEnumerator** aRetVal) {
  auto result = MakeRefPtr<JSStringEnumerator>(this);
  result.forget(aRetVal);
  return NS_OK;
}

//
// nsStringEnumerator
//

class nsStringEnumerator final : public nsSimpleEnumerator,
                                 public nsIStringEnumerator,
                                 public nsIUTF8StringEnumerator {
 public:
  nsStringEnumerator(const nsTArray<nsString>* aArray, bool aOwnsArray)
      : mArray(aArray), mIndex(0), mOwnsArray(aOwnsArray), mIsUnicode(true) {}

  nsStringEnumerator(const nsTArray<nsCString>* aArray, bool aOwnsArray)
      : mCArray(aArray), mIndex(0), mOwnsArray(aOwnsArray), mIsUnicode(false) {}

  nsStringEnumerator(const nsTArray<nsString>* aArray, nsISupports* aOwner)
      : mArray(aArray),
        mIndex(0),
        mOwner(aOwner),
        mOwnsArray(false),
        mIsUnicode(true) {}

  nsStringEnumerator(const nsTArray<nsCString>* aArray, nsISupports* aOwner)
      : mCArray(aArray),
        mIndex(0),
        mOwner(aOwner),
        mOwnsArray(false),
        mIsUnicode(false) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIUTF8STRINGENUMERATOR
  NS_DECL_NSISTRINGENUMERATORBASE

  // have to declare nsIStringEnumerator manually, because of
  // overlapping method names
  NS_IMETHOD GetNext(nsAString& aResult) override;
  NS_DECL_NSISIMPLEENUMERATOR

  const nsID& DefaultInterface() override {
    if (mIsUnicode) {
      return NS_GET_IID(nsISupportsString);
    }
    return NS_GET_IID(nsISupportsCString);
  }

 private:
  ~nsStringEnumerator() {
    if (mOwnsArray) {
      // const-casting is safe here, because the NS_New*
      // constructors make sure mOwnsArray is consistent with
      // the constness of the objects
      if (mIsUnicode) {
        delete const_cast<nsTArray<nsString>*>(mArray);
      } else {
        delete const_cast<nsTArray<nsCString>*>(mCArray);
      }
    }
  }

  union {
    const nsTArray<nsString>* mArray;
    const nsTArray<nsCString>* mCArray;
  };

  inline uint32_t Count() {
    return mIsUnicode ? mArray->Length() : mCArray->Length();
  }

  uint32_t mIndex;

  // the owner allows us to hold a strong reference to the object
  // that owns the array. Having a non-null value in mOwner implies
  // that mOwnsArray is false, because we rely on the real owner
  // to release the array
  nsCOMPtr<nsISupports> mOwner;
  bool mOwnsArray;
  bool mIsUnicode;
};

NS_IMPL_ISUPPORTS_INHERITED(nsStringEnumerator, nsSimpleEnumerator,
                            nsIStringEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
nsStringEnumerator::HasMore(bool* aResult) {
  *aResult = mIndex < Count();
  return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::HasMoreElements(bool* aResult) { return HasMore(aResult); }

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsISupports** aResult) {
  if (mIndex >= mArray->Length()) {
    return NS_ERROR_FAILURE;
  }

  if (mIsUnicode) {
    nsSupportsString* stringImpl = new nsSupportsString();

    stringImpl->SetData(mArray->ElementAt(mIndex++));
    *aResult = stringImpl;
  } else {
    nsSupportsCString* cstringImpl = new nsSupportsCString();

    cstringImpl->SetData(mCArray->ElementAt(mIndex++));
    *aResult = cstringImpl;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsAString& aResult) {
  if (NS_WARN_IF(mIndex >= Count())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mIsUnicode) {
    aResult = mArray->ElementAt(mIndex++);
  } else {
    CopyUTF8toUTF16(mCArray->ElementAt(mIndex++), aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsACString& aResult) {
  if (NS_WARN_IF(mIndex >= Count())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mIsUnicode) {
    CopyUTF16toUTF8(mArray->ElementAt(mIndex++), aResult);
  } else {
    aResult = mCArray->ElementAt(mIndex++);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::StringIterator(nsIJSEnumerator** aRetVal) {
  auto result = MakeRefPtr<JSStringEnumerator>(this);
  result.forget(aRetVal);
  return NS_OK;
}

template <class T>
static inline nsresult StringEnumeratorTail(T** aResult) {
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}

//
// constructors
//

nsresult NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                                const nsTArray<nsString>* aArray,
                                nsISupports* aOwner) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, aOwner);
  return StringEnumeratorTail(aResult);
}

nsresult NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                                    const nsTArray<nsCString>* aArray,
                                    nsISupports* aOwner) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, aOwner);
  return StringEnumeratorTail(aResult);
}

nsresult NS_NewAdoptingStringEnumerator(nsIStringEnumerator** aResult,
                                        nsTArray<nsString>* aArray) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, true);
  return StringEnumeratorTail(aResult);
}

nsresult NS_NewAdoptingUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                                            nsTArray<nsCString>* aArray) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, true);
  return StringEnumeratorTail(aResult);
}

// const ones internally just forward to the non-const equivalents
nsresult NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                                const nsTArray<nsString>* aArray) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, false);
  return StringEnumeratorTail(aResult);
}

nsresult NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                                    const nsTArray<nsCString>* aArray) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = new nsStringEnumerator(aArray, false);
  return StringEnumeratorTail(aResult);
}
