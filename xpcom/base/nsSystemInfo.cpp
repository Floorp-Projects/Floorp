/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Mike Shaver <shaver@mozilla.org>
 */

#include "prsystem.h"
#include "nsSystemInfo.h"
#include "nsMemory.h"

nsSystemInfo::nsSystemInfo()
{
    NS_INIT_REFCNT();
}

nsSystemInfo::~nsSystemInfo()
{
}

NS_IMPL_ISUPPORTS1(nsSystemInfo, nsISystemInfo)

#define SYSINFO_GETTER(name, cmd)                               \
NS_IMETHODIMP                                                   \
nsSystemInfo::Get##name(char **_retval)                         \
{                                                               \
    NS_ENSURE_ARG_POINTER(_retval);                             \
    char *buf = (char *)nsMemory::Alloc(256);                   \
    if (!buf)                                                   \
	return NS_ERROR_OUT_OF_MEMORY;                          \
    if (PR_GetSystemInfo((cmd), buf, 256) == PR_FAILURE) {      \
	nsMemory::Free(buf);                                    \
	return NS_ERROR_FAILURE;                                \
    }                                                           \
    *_retval = buf;                                             \
    return NS_OK;                                               \
}

SYSINFO_GETTER(Hostname, PR_SI_HOSTNAME)
SYSINFO_GETTER(OSName, PR_SI_SYSNAME)
SYSINFO_GETTER(OSVersion, PR_SI_RELEASE)
SYSINFO_GETTER(Architecture, PR_SI_ARCHITECTURE)

NS_METHOD
nsSystemInfo::Create(nsISupports *outer, const nsIID &aIID, void **aInstancePtr)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsSystemInfo *it = new nsSystemInfo();
    if (!it)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = it->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv)) {
        delete it;
        *aInstancePtr = 0;
    }

    return rv;
}
