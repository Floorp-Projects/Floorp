/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

 #include "nsDirectoryService.h"


nsDirectoryService::nsDirectoryService(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

NS_METHOD
nsDirectoryService::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	 NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsDirectoryService* props = new nsDirectoryService(outer);
    if (props == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = props->AggregatedQueryInterface(aIID, aResult);
	 if (NS_FAILED(rv))
	     delete props;
    return rv;
}

PRBool
nsDirectoryService::ReleaseValues(nsHashKey* key, void* data, void* closure)
{
    nsISupports* value = (nsISupports*)data;
    NS_IF_RELEASE(value);
    return PR_TRUE;
}

nsDirectoryService::~nsDirectoryService()
{
    Enumerate(ReleaseValues);
}

NS_IMPL_AGGREGATED(nsDirectoryService);

NS_METHOD
nsDirectoryService::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
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
nsDirectoryService::DefineProperty(const char* prop, nsISupports* initialValue)
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
nsDirectoryService::UndefineProperty(const char* prop)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Remove(&key);
    NS_IF_RELEASE(prevValue);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::GetProperty(const char* prop, nsISupports* *result)
{
    nsStringKey key(prop);
    if (!Exists(&key))
    {
        // check to see if it is one of our defaults
        //
        //  code here..
        //   
        //
        // else
        //
        return NS_ERROR_FAILURE;
    }

    nsISupports* value = (nsISupports*)Get(&key);
    NS_IF_ADDREF(value);
    *result = value;
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::SetProperty(const char* prop, nsISupports* value)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, value);
    NS_IF_RELEASE(prevValue);
    NS_IF_ADDREF(value);
    return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::HasProperty(const char* prop, nsISupports* expectedValue)
{
    nsISupports* value;
    nsresult rv = GetProperty(prop, &value);
    if (NS_FAILED(rv)) return rv;
    rv = (value == expectedValue) ? NS_OK : NS_ERROR_FAILURE;
    NS_IF_RELEASE(value);
    return rv;
}
