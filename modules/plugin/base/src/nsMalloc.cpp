/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

////////////////////////////////////////////////////////////////////////////////
// Implementation of nsIMalloc using NSPR
////////////////////////////////////////////////////////////////////////////////

#include "nsMalloc.h"

static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID);

nsMalloc::nsMalloc(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

nsMalloc::~nsMalloc(void)
{
}

NS_IMPL_AGGREGATED(nsMalloc);

NS_METHOD
nsMalloc::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }
	 if (aIID.Equals(NS_GET_IID(nsISupports)))
	     *aInstancePtr = GetInner();                                                                      
    else if (aIID.Equals(kIMallocIID))
        *aInstancePtr = NS_STATIC_CAST(nsIMalloc*, this);
	 else {
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
	 }

	 NS_ADDREF((nsISupports*)*aInstancePtr);
	 return NS_OK; 
}

NS_METHOD
nsMalloc::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (!aInstancePtr)
	     return NS_ERROR_INVALID_POINTER;
    if (outer && !aIID.Equals(NS_GET_IID(nsISupports)))
        return NS_ERROR_INVALID_ARG;
    nsMalloc* mm = new nsMalloc(outer);
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

	 nsresult rv = mm->AggregatedQueryInterface(aIID, aInstancePtr);
	 if (NS_FAILED(rv)) {
	     delete mm;
		  return rv;
	 }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(void*)
nsMalloc::Alloc(PRUint32 size)
{
    return PR_Malloc(size);
}

NS_METHOD_(void*)
nsMalloc::Realloc(void* ptr, PRUint32 size)
{
    return PR_Realloc(ptr, size);
}

NS_METHOD_(void)
nsMalloc::Free(void* ptr)
{
    PR_Free(ptr);
}

NS_METHOD_(PRInt32)
nsMalloc::GetSize(void* ptr)
{
    return -1;
}

NS_METHOD_(PRBool)
nsMalloc::DidAlloc(void* ptr)
{
    return PR_TRUE;
}

NS_METHOD_(void)
nsMalloc::HeapMinimize(void)
{
#ifdef XP_MAC
    // obsolete
#endif
}

////////////////////////////////////////////////////////////////////////////////
