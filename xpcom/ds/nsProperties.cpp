/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#define NS_IMPL_IDS

#include "nsProperties.h"

//#include <iostream.h>

////////////////////////////////////////////////////////////////////////////////

nsProperties::nsProperties(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

NS_METHOD
nsProperties::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsProperties* props = new nsProperties(outer);
    if (props == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = props->AggregatedQueryInterface(aIID, aResult);
    if (NS_FAILED(rv))
        delete props;
    return rv;
}

PRBool
nsProperties::ReleaseValues(nsHashKey* key, void* data, void* closure)
{
    nsISupports* value = (nsISupports*)data;
    NS_IF_RELEASE(value);
    return PR_TRUE;
}

nsProperties::~nsProperties()
{
    Enumerate(ReleaseValues);
}

NS_IMPL_AGGREGATED(nsProperties);

NS_METHOD
nsProperties::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    if (aIID.Equals(NS_GET_IID(nsISupports)))
        *aInstancePtr = GetInner();
    else if (aIID.Equals(NS_GET_IID(nsIProperties)))
        *aInstancePtr = NS_STATIC_CAST(nsIProperties*, this);
    else {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    } 

    NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Define(const char* prop, nsISupports* initialValue)
{
    nsStringKey key(prop);
    if (Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, initialValue);
    NS_ASSERTION(prevValue == NULL, "hashtable error");
    NS_IF_ADDREF(initialValue);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Undefine(const char* prop)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Remove(&key);
    NS_IF_RELEASE(prevValue);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Get(const char* prop, const nsIID & uuid, void* *result)
{
    nsresult rv;
    nsStringKey key(prop);
    nsISupports* value = (nsISupports*)nsHashtable::Get(&key);
    if (value) {
        rv = value->QueryInterface(uuid, result);
    }
    else {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

NS_IMETHODIMP
nsProperties::Set(const char* prop, nsISupports* value)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, value);
    NS_RELEASE(prevValue);
    NS_IF_ADDREF(value);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Has(const char* prop, const nsIID & uuid, 
                  nsISupports* expectedValue, PRBool *result)
{
    nsCOMPtr<nsISupports> value;
    nsresult rv = Get(prop, uuid, getter_AddRefs(value));
    if (NS_SUCCEEDED(rv)) {
        if (value == expectedValue) {
            *result = PR_TRUE;
            return NS_OK;
        }
    }
    *result = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
