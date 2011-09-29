/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHashPropertyBag.h"
#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsComponentManagerUtils.h"
#include "nsIVariant.h"
#include "nsIProperty.h"
#include "nsVariant.h"

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
    // we can only assume that Init will fail only due to OOM.
    if (!mPropertyHash.Init())
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::HasKey(const nsAString& name, bool *aResult)
{
    *aResult = mPropertyHash.Get(name, nsnull);

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::Get(const nsAString& name, nsIVariant* *_retval)
{
    if (!mPropertyHash.Get(name, _retval))
        *_retval = nsnull;

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

    bool success = mPropertyHash.Put(name, value);
    if (!success)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::DeleteProperty(const nsAString& name)
{
    // is it too much to ask for ns*Hashtable to return
    // a boolean indicating whether RemoveEntry succeeded
    // or not?!?!
    bool isFound = mPropertyHash.Get(name, nsnull);
    if (!isFound)
        return NS_ERROR_FAILURE;

    // then from the hash
    mPropertyHash.Remove(name);

    return NS_OK;
}


//
// nsSimpleProperty class and impl; used for GetEnumerator
//

class nsSimpleProperty : public nsIProperty {
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
    propertyArray->AppendElement(sprop, PR_FALSE);
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
        *_retval = nsnull;
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

