/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Mike Shaver <shaver@mozilla.org>
 */

/*
 * Implementation of nsIDirectoryEnumerator for ``Unixy'' systems.
 */

#include "nsIDirEnumeratorImplUnix.h"
#include "nsIFileImplUnix.h"	// NSRESULT_FOR_ERRNO, etc.
#include "nsFileUtils.h"

nsIDirEnumeratorImpl::nsIDirEnumeratorImpl() :
  mDir(nsnull), mEntry(nsnull)
{
    NS_INIT_REFCNT();
}

nsIDirEnumeratorImpl::~nsIDirEnumeratorImpl()
{
    if (mDir)
	closedir(mDir);
}

NS_IMPL_ISUPPORTS1(nsIDirEnumeratorImpl, nsIDirectoryEnumerator)

NS_METHOD
nsIDirEnumeratorImpl::Create(nsISupports* outer, const nsIID& aIID,
			     void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);
    
    *aInstancePtr = 0;

    nsCOMPtr<nsIDirectoryEnumerator> inst = new nsIDirEnumeratorImpl();
    if (inst.get() == 0)
        return NS_ERROR_OUT_OF_MEMORY;
    return inst->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsIDirEnumeratorImpl::Init(nsIFile *parent, PRBool resolveSymlinks /*ignored*/)
{
    nsXPIDLCString dirPath;
    if (NS_FAILED(parent->GetPath(nsIFile::UNIX_PATH,
				  getter_Copies(dirPath))) ||
	(const char *)dirPath == 0)
	return NS_ERROR_FILE_INVALID_PATH;

    mParent = parent;
    mDir = opendir(dirPath);
    if (!mDir)
	return NSRESULT_FOR_ERRNO();
    return getNextEntry();
}

NS_IMETHODIMP
nsIDirEnumeratorImpl::HasMoreElements(PRBool *result)
{
    *result = mDir && mEntry;
    return NS_OK;
}

NS_IMETHODIMP
nsIDirEnumeratorImpl::GetNext(nsISupports **_retval)
{
    nsresult rv;
    if (!mDir || !mEntry) {
	*_retval = nsnull;
	return NS_OK;
    }

    nsIFile *file = nsnull;
    if (NS_FAILED(rv = NS_NewFile(&file)))
	return rv;

    if (NS_FAILED(rv = file->InitWithFile(mParent)) ||
	NS_FAILED(rv = file->AppendPath(mEntry->d_name))) {
	NS_RELEASE(file);
	return rv;
    }
    *_retval = NS_STATIC_CAST(nsISupports *, file);
    return getNextEntry();
}

NS_IMETHODIMP
nsIDirEnumeratorImpl::getNextEntry()
{
    errno = 0;
    mEntry = readdir(mDir);
    if (mEntry)
	return NS_OK;
    return NSRESULT_FOR_ERRNO();
}

