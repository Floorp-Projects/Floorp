/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsProperties.h"
#include "nsString.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsProperties)

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
nsProperties::Get(const char* prop, const nsIID & uuid, void* *result)
{
    nsCOMPtr<nsISupports> value;
    if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value))) {
        return NS_ERROR_FAILURE;
    }
    return value->QueryInterface(uuid, result);
}

NS_IMETHODIMP
nsProperties::Set(const char* prop, nsISupports* value)
{
    return Put(prop, value) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsProperties::Undefine(const char* prop)
{
    nsCOMPtr<nsISupports> value;
    if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value)))
        return NS_ERROR_FAILURE;

    Remove(prop);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Has(const char* prop, PRBool *result)
{
    nsCOMPtr<nsISupports> value;
    *result = nsProperties_HashBase::Get(prop,
                                         getter_AddRefs(value));
    return NS_OK;
}

struct GetKeysEnumData
{
    char **keys;
    PRUint32 next;
    nsresult res;
};

PR_CALLBACK PLDHashOperator
GetKeysEnumerate(const char *key, nsISupports* data,
                 void *arg)
{
    GetKeysEnumData *gkedp = (GetKeysEnumData *)arg;
    gkedp->keys[gkedp->next] = nsCRT::strdup(key);

    if (!gkedp->keys[gkedp->next]) {
        gkedp->res = NS_ERROR_OUT_OF_MEMORY;
        return PL_DHASH_STOP;
    }

    gkedp->next++;
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP 
nsProperties::GetKeys(PRUint32 *count, char ***keys)
{
    PRUint32 n = Count();
    char ** k = (char **) nsMemory::Alloc(n * sizeof(char *));
    if (!k)
        return NS_ERROR_OUT_OF_MEMORY;

    GetKeysEnumData gked;
    gked.keys = k;
    gked.next = 0;
    gked.res = NS_OK;

    EnumerateRead(GetKeysEnumerate, &gked);

    if (NS_FAILED(gked.res)) {
        // Free 'em all
        for (PRUint32 i = 0; i < gked.next; i++)
            nsMemory::Free(k[i]);
        nsMemory::Free(k);
        return gked.res;
    }

    *count = n;
    *keys = k;
    return NS_OK;
}

NS_METHOD
nsProperties::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_PROPER_AGGREGATION(aOuter, aIID);

    nsProperties* props = new nsProperties(aOuter);
    if (props == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(props);
    nsresult rv = props->Init();
    if (NS_SUCCEEDED(rv))
        rv = props->AggregatedQueryInterface(aIID, aResult);

    NS_RELEASE(props);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
