/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Doug Turner <dougt@netscape.com>
 */


#include "nsIComponentManager.h"
#include "nsIDirEnumeratorImpl.h"
#include "nsIDirEnumeratorImplWin.h"
#include "nsFileUtils.h"

nsIDirEnumeratorImpl::nsIDirEnumeratorImpl()
{
    NS_INIT_REFCNT();
    mDir = nsnull;
}

nsIDirEnumeratorImpl::~nsIDirEnumeratorImpl()
{
    if (mDir)
	    PR_CloseDir(mDir);    
}


/* nsISupports interface implementation. */
NS_IMPL_ISUPPORTS1(nsIDirEnumeratorImpl, nsIDirectoryEnumerator)

NS_METHOD
nsIDirEnumeratorImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsIDirEnumeratorImpl* inst = new nsIDirEnumeratorImpl();
    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv))
    {
        delete inst;
        return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP  
nsIDirEnumeratorImpl::Init(nsIFile *parent, PRBool resolveSymlinks)
{
    char* filepath;
    parent->GetPath(nsIFile::NSPR_PATH, &filepath);

    if (filepath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mDir = PR_OpenDir(filepath);

    if (mDir == nsnull)    // not a directory
        return NS_ERROR_FAILURE;

    nsAllocator::Free(filepath);

    mParent   = parent;

    mResolveSymlinks = resolveSymlinks;
    
    return NS_NewFile(&mNext);
}

/* boolean HasMoreElements (); */
NS_IMETHODIMP  
nsIDirEnumeratorImpl::HasMoreElements(PRBool *result) 
{
    nsresult rv;
    *result = PR_FALSE;
    
    if (mDir) 
    {
        PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
        if (entry == nsnull) 
            return NS_OK;
        
        mNext->InitWithFile(mParent);
        rv = mNext->AppendPath(entry->name);

        if (NS_FAILED(rv)) 
            return rv;
    }
    *result = PR_TRUE;
    return NS_OK;
}

/* nsISupports GetNext (); */
NS_IMETHODIMP  
nsIDirEnumeratorImpl::GetNext(nsISupports **_retval)
{
    nsresult rv;
    PRBool hasMore;
    
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) 
        return rv;
    
    nsIFile *file;
    rv = NS_NewFile(&file);
    
    if (NS_FAILED(rv)) 
        return rv;

    rv = file->InitWithFile(mNext);
    
    *_retval = file;        // TODO: QI needed?
    return rv;
}