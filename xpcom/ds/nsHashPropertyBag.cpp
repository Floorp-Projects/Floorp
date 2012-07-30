/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHashPropertyBag.h"
#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsComponentManagerUtils.h"
#include "nsIVariant.h"
#include "nsIProperty.h"
#include "nsVariant.h"
#include "mozilla/Attributes.h"

nsresult
NS_NewHashPropertyBag(nsIWritablePropertyBag* *_retval)
{
    nsHashPropertyBag *hpb = new nsHashPropertyBag();
    if (!hpb)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(hpb);

    nsresult rv = hpb->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(hpb);
        return rv;
    }

    *_retval = hpb;
    return NS_OK;
}

/*
 * nsHashPropertyBag impl
 */

NS_IMPL_THREADSAFE_ADDREF(nsHashPropertyBag)
NS_IMPL_THREADSAFE_RELEASE(nsHashPropertyBag)
NS_INTERFACE_MAP_BEGIN(nsHashPropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIPropertyBag, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsIPropertyBag2)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag2)
NS_INTERFACE_MAP_END

nsresult
nsHashPropertyBag::Init()
{
    mPropertyHash.Init();
    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::HasKey(const nsAString& name, bool *aResult)
{
    *aResult = mPropertyHash.Get(name, nullptr);

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::Get(const nsAString& name, nsIVariant* *_retval)
{
    if (!mPropertyHash.Get(name, _retval))
        *_retval = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::GetProperty(const nsAString& name, nsIVariant* *_retval)
{
    bool isFound = mPropertyHash.Get(name, _retval);
    if (!isFound)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::SetProperty(const nsAString& name, nsIVariant *value)
{
    NS_ENSURE_ARG_POINTER(value);

    mPropertyHash.Put(name, value);

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::DeleteProperty(const nsAString& name)
{
    // is it too much to ask for ns*Hashtable to return
    // a boolean indicating whether RemoveEntry succeeded
    // or not?!?!
    bool isFound = mPropertyHash.Get(name, nullptr);
    if (!isFound)
        return NS_ERROR_FAILURE;

    // then from the hash
    mPropertyHash.Remove(name);

    return NS_OK;
}


//
// nsSimpleProperty class and impl; used for GetEnumerator
//

class nsSimpleProperty MOZ_FINAL : public nsIProperty {
public:
    nsSimpleProperty(const nsAString& aName, nsIVariant* aValue)
        : mName(aName), mValue(aValue)
    {
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROPERTY
protected:
    nsString mName;
    nsCOMPtr<nsIVariant> mValue;
};

NS_IMPL_ISUPPORTS1(nsSimpleProperty, nsIProperty)

NS_IMETHODIMP
nsSimpleProperty::GetName(nsAString& aName)
{
    aName.Assign(mName);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleProperty::GetValue(nsIVariant* *aValue)
{
    NS_IF_ADDREF(*aValue = mValue);
    return NS_OK;
}

// end nsSimpleProperty

static PLDHashOperator
PropertyHashToArrayFunc (const nsAString &aKey,
                         nsIVariant* aData,
                         void *userArg)
{
    nsIMutableArray *propertyArray =
        static_cast<nsIMutableArray *>(userArg);
    nsSimpleProperty *sprop = new nsSimpleProperty(aKey, aData);
    propertyArray->AppendElement(sprop, false);
    return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsHashPropertyBag::GetEnumerator(nsISimpleEnumerator* *_retval)
{
    nsCOMPtr<nsIMutableArray> propertyArray = new nsArray();
    if (!propertyArray)
        return NS_ERROR_OUT_OF_MEMORY;

    mPropertyHash.EnumerateRead(PropertyHashToArrayFunc, propertyArray.get());

    return NS_NewArrayEnumerator(_retval, propertyArray);
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
    if (!var) \
        return NS_ERROR_OUT_OF_MEMORY; \
    var->SetAs ## Name(value); \
    return SetProperty(prop, var); \
}

IMPL_GETSETPROPERTY_AS(Int32, PRInt32)
IMPL_GETSETPROPERTY_AS(Uint32, PRUint32)
IMPL_GETSETPROPERTY_AS(Int64, PRInt64)
IMPL_GETSETPROPERTY_AS(Uint64, PRUint64)
IMPL_GETSETPROPERTY_AS(Double, double)
IMPL_GETSETPROPERTY_AS(Bool, bool)


NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsAString(const nsAString & prop, nsAString & _retval)
{
    nsIVariant* v = mPropertyHash.GetWeak(prop);
    if (!v)
        return NS_ERROR_NOT_AVAILABLE;
    return v->GetAsAString(_retval);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsACString(const nsAString & prop, nsACString & _retval)
{
    nsIVariant* v = mPropertyHash.GetWeak(prop);
    if (!v)
        return NS_ERROR_NOT_AVAILABLE;
    return v->GetAsACString(_retval);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsAUTF8String(const nsAString & prop, nsACString & _retval)
{
    nsIVariant* v = mPropertyHash.GetWeak(prop);
    if (!v)
        return NS_ERROR_NOT_AVAILABLE;
    return v->GetAsAUTF8String(_retval);
}

NS_IMETHODIMP
nsHashPropertyBag::GetPropertyAsInterface(const nsAString & prop,
                                          const nsIID & aIID,
                                          void** _retval)
{
    nsIVariant* v = mPropertyHash.GetWeak(prop);
    if (!v)
        return NS_ERROR_NOT_AVAILABLE;
    nsCOMPtr<nsISupports> val;
    nsresult rv = v->GetAsISupports(getter_AddRefs(val));
    if (NS_FAILED(rv))
        return rv;
    if (!val) {
        // We have a value, but it's null
        *_retval = nullptr;
        return NS_OK;
    }
    return val->QueryInterface(aIID, _retval);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsAString(const nsAString & prop, const nsAString & value)
{
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (!var)
        return NS_ERROR_OUT_OF_MEMORY;
    var->SetAsAString(value);
    return SetProperty(prop, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsACString(const nsAString & prop, const nsACString & value)
{
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (!var)
        return NS_ERROR_OUT_OF_MEMORY;
    var->SetAsACString(value);
    return SetProperty(prop, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsAUTF8String(const nsAString & prop, const nsACString & value)
{
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (!var)
        return NS_ERROR_OUT_OF_MEMORY;
    var->SetAsAUTF8String(value);
    return SetProperty(prop, var);
}

NS_IMETHODIMP
nsHashPropertyBag::SetPropertyAsInterface(const nsAString & prop, nsISupports* value)
{
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (!var)
        return NS_ERROR_OUT_OF_MEMORY;
    var->SetAsISupports(value);
    return SetProperty(prop, var);
}

