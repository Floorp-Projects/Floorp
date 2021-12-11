/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#include "nsEnumeratorUtils.h"

#include "nsIStringEnumerator.h"
#include "nsSimpleEnumerator.h"

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

class EmptyEnumeratorImpl : public nsSimpleEnumerator,
                            public nsIUTF8StringEnumerator,
                            public nsIStringEnumerator {
 public:
  EmptyEnumeratorImpl() = default;

  // nsISupports interface. Not really inherited, but no mRefCnt.
  NS_DECL_ISUPPORTS_INHERITED

  // nsISimpleEnumerator
  NS_DECL_NSISIMPLEENUMERATOR
  NS_DECL_NSIUTF8STRINGENUMERATOR
  NS_DECL_NSISTRINGENUMERATORBASE
  // can't use NS_DECL_NSISTRINGENUMERATOR because they share the
  // HasMore() signature

  NS_IMETHOD GetNext(nsAString& aResult) override;

  static EmptyEnumeratorImpl* GetInstance() {
    static const EmptyEnumeratorImpl kInstance;
    return const_cast<EmptyEnumeratorImpl*>(&kInstance);
  }
};

// nsISupports interface
NS_IMETHODIMP_(MozExternalRefCountType)
EmptyEnumeratorImpl::AddRef(void) { return 2; }

NS_IMETHODIMP_(MozExternalRefCountType)
EmptyEnumeratorImpl::Release(void) { return 1; }

NS_IMPL_QUERY_INTERFACE_INHERITED(EmptyEnumeratorImpl, nsSimpleEnumerator,
                                  nsIUTF8StringEnumerator, nsIStringEnumerator)

// nsISimpleEnumerator interface
NS_IMETHODIMP
EmptyEnumeratorImpl::HasMoreElements(bool* aResult) {
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmptyEnumeratorImpl::HasMore(bool* aResult) {
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmptyEnumeratorImpl::GetNext(nsISupports** aResult) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
EmptyEnumeratorImpl::GetNext(nsACString& aResult) {
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
EmptyEnumeratorImpl::GetNext(nsAString& aResult) { return NS_ERROR_UNEXPECTED; }

NS_IMETHODIMP
EmptyEnumeratorImpl::StringIterator(nsIJSEnumerator** aRetVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult NS_NewEmptyEnumerator(nsISimpleEnumerator** aResult) {
  *aResult = EmptyEnumeratorImpl::GetInstance();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsSingletonEnumerator final : public nsSimpleEnumerator {
 public:
  // nsISimpleEnumerator methods
  NS_IMETHOD HasMoreElements(bool* aResult) override;
  NS_IMETHOD GetNext(nsISupports** aResult) override;

  explicit nsSingletonEnumerator(nsISupports* aValue);

 private:
  ~nsSingletonEnumerator() override;

 protected:
  nsCOMPtr<nsISupports> mValue;
  bool mConsumed;
};

nsSingletonEnumerator::nsSingletonEnumerator(nsISupports* aValue)
    : mValue(aValue) {
  mConsumed = (mValue ? false : true);
}

nsSingletonEnumerator::~nsSingletonEnumerator() = default;

NS_IMETHODIMP
nsSingletonEnumerator::HasMoreElements(bool* aResult) {
  MOZ_ASSERT(aResult != 0, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = !mConsumed;
  return NS_OK;
}

NS_IMETHODIMP
nsSingletonEnumerator::GetNext(nsISupports** aResult) {
  MOZ_ASSERT(aResult != 0, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mConsumed) {
    return NS_ERROR_FAILURE;
  }

  mConsumed = true;

  *aResult = mValue;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult NS_NewSingletonEnumerator(nsISimpleEnumerator** aResult,
                                   nsISupports* aSingleton) {
  RefPtr<nsSingletonEnumerator> enumer = new nsSingletonEnumerator(aSingleton);
  enumer.forget(aResult);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsUnionEnumerator final : public nsSimpleEnumerator {
 public:
  // nsISimpleEnumerator methods
  NS_IMETHOD HasMoreElements(bool* aResult) override;
  NS_IMETHOD GetNext(nsISupports** aResult) override;

  nsUnionEnumerator(nsISimpleEnumerator* aFirstEnumerator,
                    nsISimpleEnumerator* aSecondEnumerator);

 private:
  ~nsUnionEnumerator() override;

 protected:
  nsCOMPtr<nsISimpleEnumerator> mFirstEnumerator, mSecondEnumerator;
  bool mConsumed;
  bool mAtSecond;
};

nsUnionEnumerator::nsUnionEnumerator(nsISimpleEnumerator* aFirstEnumerator,
                                     nsISimpleEnumerator* aSecondEnumerator)
    : mFirstEnumerator(aFirstEnumerator),
      mSecondEnumerator(aSecondEnumerator),
      mConsumed(false),
      mAtSecond(false) {}

nsUnionEnumerator::~nsUnionEnumerator() = default;

NS_IMETHODIMP
nsUnionEnumerator::HasMoreElements(bool* aResult) {
  MOZ_ASSERT(aResult != 0, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  if (mConsumed) {
    *aResult = false;
    return NS_OK;
  }

  if (!mAtSecond) {
    rv = mFirstEnumerator->HasMoreElements(aResult);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (*aResult) {
      return NS_OK;
    }

    mAtSecond = true;
  }

  rv = mSecondEnumerator->HasMoreElements(aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (*aResult) {
    return NS_OK;
  }

  *aResult = false;
  mConsumed = true;
  return NS_OK;
}

NS_IMETHODIMP
nsUnionEnumerator::GetNext(nsISupports** aResult) {
  MOZ_ASSERT(aResult != 0, "null ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mConsumed) {
    return NS_ERROR_FAILURE;
  }

  if (!mAtSecond) {
    return mFirstEnumerator->GetNext(aResult);
  }

  return mSecondEnumerator->GetNext(aResult);
}

nsresult NS_NewUnionEnumerator(nsISimpleEnumerator** aResult,
                               nsISimpleEnumerator* aFirstEnumerator,
                               nsISimpleEnumerator* aSecondEnumerator) {
  *aResult = nullptr;
  if (!aFirstEnumerator) {
    *aResult = aSecondEnumerator;
  } else if (!aSecondEnumerator) {
    *aResult = aFirstEnumerator;
  } else {
    auto* enumer = new nsUnionEnumerator(aFirstEnumerator, aSecondEnumerator);
    *aResult = enumer;
  }
  NS_ADDREF(*aResult);
  return NS_OK;
}
