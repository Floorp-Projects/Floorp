/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

nsresult
NS_NewHashPropertyBag(nsIWritablePropertyBag* *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsHashPropertyBag *hpb = new nsHashPropertyBag();
    if (!hpb)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*_retval = hpb);
    return NS_OK;
}

/*
 * nsHashPropertyBag impl
 */

NS_IMPL_ISUPPORTS2(nsHashPropertyBag, nsIWritablePropertyBag, nsIPropertyBag)

NS_IMETHODIMP
nsHashPropertyBag::GetProperty(const nsAString& name, nsIVariant* *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    if (!mPropertyHash.IsInitialized())
        return NS_ERROR_FAILURE;

    PRBool isFound = mPropertyHash.Get(name, _retval);
    if (!isFound)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::SetProperty(const nsAString& name, nsIVariant *value)
{
    NS_ENSURE_ARG_POINTER(value);

    if (!mPropertyHash.IsInitialized()) {
        // we can only assume that Init will fail only due to OOM.
        if (!mPropertyHash.Init())
            return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool success = mPropertyHash.Put(name, value);
    if (!success)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsHashPropertyBag::DeleteProperty(const nsAString& name)
{
    if (!mPropertyHash.IsInitialized())
        return NS_ERROR_FAILURE;

    // is it too much to ask for ns*Hashtable to return
    // a boolean indicating whether RemoveEntry succeeded
    // or not?!?!
    PRBool isFound = mPropertyHash.Get(name, nsnull);
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
    NS_ENSURE_ARG_POINTER(aValue);
    NS_IF_ADDREF(*aValue = mValue);
    return NS_OK;
}

// end nsSimpleProperty

PR_STATIC_CALLBACK(PLDHashOperator)
PropertyHashToArrayFunc (const nsAString &aKey,
                         nsIVariant* aData,
                         void *userArg)
{
    nsIMutableArray *propertyArray =
        NS_STATIC_CAST(nsIMutableArray *, userArg);
    nsSimpleProperty *sprop = new nsSimpleProperty(aKey, aData);
    propertyArray->AppendElement(sprop, PR_FALSE);
    return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsHashPropertyBag::GetEnumerator(nsISimpleEnumerator* *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult rv;

    nsCOMPtr<nsIMutableArray> propertyArray;
    rv = NS_NewArray (getter_AddRefs(propertyArray));
    if (NS_FAILED(rv))
        return rv;

    mPropertyHash.EnumerateRead(PropertyHashToArrayFunc, propertyArray.get());

    return NS_NewArrayEnumerator(_retval, propertyArray);
}

