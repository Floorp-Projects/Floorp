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
 * Implementation of nsIFile for ``Unixy'' systems.
 */

#ifndef _nsLocalFileUNIX_H_
#define _nsLocalFileUNIX_H_

#include <sys/stat.h>
#include <errno.h>

#include "nscore.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsLocalFile.h"
#include "nsXPIDLString.h"

#define NSRESULT_FOR_RETURN(ret) (((ret) < 0) ? NSRESULT_FOR_ERRNO() : NS_OK)

inline nsresult
nsresultForErrno(int err)
{
#ifdef DEBUG_shaver
    if (err)
        fprintf(stderr, "errno %d\n", err);
#endif
    switch(err) {
      case 0:
        return NS_OK;
      case ENOENT:
        return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
      case ENOTDIR:
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
#ifdef ENOLINK
      case ENOLINK:
        return NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
#endif /* ENOLINK */
      case EEXIST:
        return NS_ERROR_FILE_ALREADY_EXISTS;
      case EACCES:
      default:
        return NS_ERROR_FAILURE;
    }
}

#define NSRESULT_FOR_ERRNO() nsresultForErrno(errno)

class NS_COM nsLocalFile : public nsILocalFile
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();
    virtual ~nsLocalFile();

    static NS_METHOD nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIFile
    NS_DECL_NSIFILE

    // nsILocalFile
    NS_DECL_NSILOCALFILE

protected:
    PRBool mHaveCachedStat;
    struct stat mCachedStat;
    nsXPIDLCString mPath;
    
    nsresult CreateAllAncestors(PRUint32 permissions);
    nsresult GetLeafNameRaw(const char **_retval);
    nsresult GetTargetPathName(nsIFile *newParent, const char *newName,
                               char **_retval);

    void InvalidateCache() { mHaveCachedStat = PR_FALSE; }

    nsresult FillStatCache() {
	if (stat(mPath, &mCachedStat) == -1) {
#ifdef DEBUG_shaver
	    fprintf(stderr, "stat(%s) -> %d\n", (const char *)mPath, errno);
#endif
	    return NS_ERROR_FAILURE;
	}
	mHaveCachedStat = PR_TRUE;
	return NS_OK;
    }

};

#endif /* _nsLocalFileUNIX_H_ */
