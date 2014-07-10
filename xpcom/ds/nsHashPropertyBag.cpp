/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHashPropertyBag.h"
#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsIVariant.h"
#include "nsIProperty.h"
#include "nsVariant.h"
#include "mozilla/Attributes.h"

nsresult
NS_NewHashPropertyBag(nsIWritablePropertyBag** aResult)
{
  nsRefPtr<nsHashPropertyBag> hpb = new nsHashPropertyBag();
  hpb.forget(aResult);
  return NS_OK;
}

/*
 * nsHashPropertyBag impl
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

NS_IMETHODIMP
nsHashPropertyBag::HasKey(const nsAString& aName, bool* aResult)
{
  *aResult = mPropertyHash.Get(aName, nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::Get(const nsAString& aName, nsIVariant** aResult)
{
  if (!mPropertyHash.Get(aName, aResult)) {
    *aResult = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::GetProperty(const nsAString& aName, nsIVariant** aResult)
{
  bool isFound = mPropertyHash.Get(aName, aResult);
  if (!isFound) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::SetProperty(const nsAString& aName, nsIVariant* aValue)
{
  if (NS_WARN_IF(!aValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  mPropertyHash.Put(aName, aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::DeleteProperty(const nsAString& aName)
{
  // is it too much to ask for ns*Hashtable to return
  // a boolean indicating whether RemoveEntry succeeded
  // or not?!?!
  bool isFound = mPropertyHash.Get(aName, nullptr);
  if (!isFound) {
    return NS_ERROR_FAILURE;
  }

  // then from the hash
  mPropertyHash.Remove(aName);

  return NS_OK;
}


//
// nsSimpleProperty class and impl; used for GetEnumerator
//

class nsSimpleProperty MOZ_FINAL : public nsIProperty
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

static PLDHashOperator
PropertyHashToArrayFunc(const nsAString& aKey,
                        nsIVariant* aData,
                        void* aUserArg)
{
  nsIMutableArray* propertyArray = static_cast<nsIMutableArray*>(aUserArg);
  nsSimpleProperty* sprop = new nsSimpleProperty(aKey, aData);
  propertyArray->AppendElement(sprop, false);
  return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsHashPropertyBag::GetEnumerator(nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIMutableArray> propertyArray = nsArray::Create();
  if (!propertyArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mPropertyHash.EnumerateRead(PropertyHashToArrayFunc, propertyArray.get());

  return NS_NewArrayEnumerator(aResult, propertyArray);
}

#define IMPL_GETSETPROPERTY_AS(Name, Type) \
NS_IMETHODIMP \
nsHashPropertyBag::GetPropertyAs ## Name (const nsAString & prop, Type *_retval) \
{ \
    nsIVariant* v = mPropertyHash.GetWeak(prop); \
    if (!v) \
        return NS_ERROR_NOT_AVAILABLE; \
    return v->GetAs ## Name(_retval); \
} \
\
NS_IMETHODIMP \
nsHashPropertyBag::SetPropertyAs ## Name (const nsAString & prop, Type value) \
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
nsHashPropertyBag::GetPropertyAsAString(const nsAString& aProp,
                                        nsAString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsAString(aResult);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsACString(const nsAString& aProp,
                                         nsACString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsACString(aResult);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsAUTF8String(const nsAString& aProp,
                                            nsACString& aResult)
{
  nsIVariant* v = mPropertyHash.GetWeak(aProp);
  if (!v) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return v->GetAsAUTF8String(aResult);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsInterface(const nsAString& aProp,
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
nsHashPropertyBag::SetPropertyAsAString(const nsAString& aProp,
                                        const nsAString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsAString(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsACString(const nsAString& aProp,
                                         const nsACString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsACString(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsAUTF8String(const nsAString& aProp,
                                            const nsACString& aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsAUTF8String(aValue);
  return SetProperty(aProp, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsInterface(const nsAString& aProp,
                                          nsISupports* aValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  var->SetAsISupports(aValue);
  return SetProperty(aProp, var);
}

