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
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsProperties)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsProperties)
    NS_INTERFACE_MAP_ENTRY(nsIProperties)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsProperties::Get(const char* prop, const nsIID & uuid, void* *result)
{
    NS_ENSURE_ARG(prop);

    nsCOMPtr<nsISupports> value;
    if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value))) {
        return NS_ERROR_FAILURE;
    }
    return (value) ? value->QueryInterface(uuid, result) : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsProperties::Set(const char* prop, nsISupports* value)
{
    NS_ENSURE_ARG(prop);

    return Put(prop, value) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsProperties::Undefine(const char* prop)
{
    NS_ENSURE_ARG(prop);

    nsCOMPtr<nsISupports> value;
    if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value)))
        return NS_ERROR_FAILURE;

    Remove(prop);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::Has(const char* prop, bool *result)
{
    NS_ENSURE_ARG(prop);

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

 PLDHashOperator
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
    NS_ENSURE_ARG(count);
    NS_ENSURE_ARG(keys);

    PRUint32 n = Count();
    char ** k = (char **) nsMemory::Alloc(n * sizeof(char *));
    NS_ENSURE_TRUE(k, NS_ERROR_OUT_OF_MEMORY);

    GetKeysEnumData gked;
    gked.keys = k;
    gked.next = 0;
    gked.res = NS_OK;

    EnumerateRead(GetKeysEnumerate, &gked);

    nsresult rv = gked.res;
    if (NS_FAILED(rv)) {
        // Free 'em all
        for (PRUint32 i = 0; i < gked.next; i++)
            nsMemory::Free(k[i]);
        nsMemory::Free(k);
        return rv;
    }

    *count = n;
    *keys = k;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
