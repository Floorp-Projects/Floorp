/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHashPropertyBag.h"
#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsIVariant.h"
#include "nsIProperty.h"
#include "nsThreadUtils.h"
#include "nsVariant.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

/*
 * nsHashPropertyBagBase implementation.
 */

NS_IMETHODIMP
nsHashPropertyBagBase::HasKey(const nsAString& aName, bool* aResult)
{
  *aResult = mPropertyHash.Get(aName, nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBagBase::Get(const nsAString& aName, nsIVariant** aResult)
{
  if (!mPropertyHash.Get(aName, aResult)) {
    *aResult = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBagBase::GetProperty(const nsAString& aName, nsIVariant** aResult)
{
  bool isFound = mPropertyHash.Get(aName, aResult);
  if (!isFound) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBagBase::SetProperty(const nsAString& aName, nsIVariant* aValue)
{
  if (NS_WARN_IF(!aValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  mPropertyHash.Put(aName, aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBagBase::DeleteProperty(const nsAString& aName)
{
  return mPropertyHash.Remove(aName) ? NS_OK : NS_ERROR_FAILURE;
}


//
// nsSimpleProperty class and impl; used for GetEnumerator
//

class nsSimpleProperty final : public nsIProperty
{
  ~nsSimpleProperty() {}

public:
  nsSimpleProperty(const nsAString& aName, nsIVariant* aValue)
    : mName(aName)
    , mValue(aValue)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY
protected:
  nsString mName;
  nsCOMPtr<nsIVariant> mValue;
};

NS_IMPL_ISUPPORTS(nsSimpleProperty, nsIProperty)

NS_IMETHODIMP
nsSimpleProperty::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleProperty::GetValue(nsIVariant** aValue)
{
  NS_IF_ADDREF(*aValue = mValue);
  return NS_OK;
}

// end nsSimpleProperty

NS_IMETHODIMP
nsHashPropertyBagBase::GetEnumerator(nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIMutableArray> propertyArray = nsArray::Create();
  if (!propertyArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (auto iter = mPropertyHash.Iter(); !iter.Done(); iter.Next()) {
    const nsAString& key = iter.Key();
    nsIVariant* data = iter.UserData();
    nsSimpleProperty* sprop = new nsSimpleProperty(key, data);
    propertyArray->AppendElement(sprop, false);
  }

  return NS_NewArrayEnumerator(aResult, propertyArray);
}

#define IMPL_GETSETPROPERTY_AS(Name, Type) \
NS_IMETHODIMP \
nsHashPropertyBagBase::GetPropertyAs ## Name (const nsAString & prop, Type *_retval) \
{ \
    nsIVariant* v = mPropertyHash.GetWeak(prop); \
    if (!v) \
        return NS_ERROR_NOT_AVAILABLE; \
    return v->GetAs ## Name(_retval); \
} \
\
NS_IMETHODIMP \
nsHashPropertyBagBase::SetPropertyAs ## Name (const nsAString & prop, Type value) \
{ \
    nsCOMPtr<nsIWritableVariant> var = new nsVariant(); \
    var->SetAs ## Name(value); \
    return SetProperty(prop, var); \
}

IMPL_GETSETPROPERTY_AS(Int32, int32_t)
IMPL_GETSETPROPERTY_AS(Uint32, uint32_t)
IMPL_GETSETPROPERTY_AS(Int64, int64_t)
IMPL_GETSETPROPERTY_AS(Uint64, uint64_t)
IMPL_GETSETPROPERTY_AS(Double, double)
IMPL_GETSETPROPERTY_AS(Bool, bool)


NS_IMETHODIMP
nsHashPropertyBagBase::GetPropertyAsAString(const nsAString& aProp,
                                            nsAString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsAString(aResult);
}

NS_IMETHODIMP
nsHashPropertyBagBase::GetPropertyAsACString(const nsAString& aProp,
                                             nsACString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsACString(aResult);
}

NS_IMETHODIMP
nsHashPropertyBagBase::GetPropertyAsAUTF8String(const nsAString& aProp,
                                                nsACString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsAUTF8String(aResult);
}

NS_IMETHODIMP
nsHashPropertyBagBase::GetPropertyAsInterface(const nsAString& aProp,
                                              const nsIID& aIID,
                                              void** aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsCOMPtr<nsISupports> val;
  nsresult rv = v->GetAsISupports(getter_AddRefs(val));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!val) {
    // We have a value, but it's null
    *aResult = nullptr;
    return NS_OK;
  }
  return val->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsHashPropertyBagBase::SetPropertyAsAString(const nsAString& aProp,
                                            const nsAString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsAString(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBagBase::SetPropertyAsACString(const nsAString& aProp,
                                             const nsACString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsACString(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBagBase::SetPropertyAsAUTF8String(const nsAString& aProp,
                                                const nsACString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsAUTF8String(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBagBase::SetPropertyAsInterface(const nsAString& aProp,
                                              nsISupports* aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsISupports(aValue);
  return SetProperty(aProp, var);
}


/*
 * nsHashPropertyBag implementation.
 */

NS_IMPL_ADDREF(nsHashPropertyBag)
NS_IMPL_RELEASE(nsHashPropertyBag)

NS_INTERFACE_MAP_BEGIN(nsHashPropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIPropertyBag, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsIPropertyBag2)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag2)
NS_INTERFACE_MAP_END

/*
 * We need to ensure that the hashtable is destroyed on the main thread, as
 * the nsIVariant values are main-thread only objects.
 */
class ProxyHashtableDestructor final : public mozilla::Runnable
{
public:
  using HashtableType = nsInterfaceHashtable<nsStringHashKey, nsIVariant>;
  explicit ProxyHashtableDestructor(HashtableType&& aTable)
    : mozilla::Runnable("ProxyHashtableDestructor")
    , mPropertyHash(mozilla::Move(aTable))
  {}

  NS_IMETHODIMP
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    HashtableType table(mozilla::Move(mPropertyHash));
    return NS_OK;
  }

private:
  HashtableType mPropertyHash;
};

nsHashPropertyBag::~nsHashPropertyBag()
{
  if (!NS_IsMainThread()) {
    RefPtr<ProxyHashtableDestructor> runnable =
      new ProxyHashtableDestructor(mozilla::Move(mPropertyHash));
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
  }
}

/*
 * nsHashPropertyBagCC implementation.
 */

NS_IMPL_CYCLE_COLLECTION(nsHashPropertyBagCC, mPropertyHash)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHashPropertyBagCC)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHashPropertyBagCC)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsHashPropertyBagCC)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIPropertyBag, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsIPropertyBag2)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag2)
NS_INTERFACE_MAP_END
