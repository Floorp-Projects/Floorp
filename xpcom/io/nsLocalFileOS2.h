/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Henry Sobotka <sobotka@axess.com>
 */

#ifndef _nsLocalFileOS2_H_
#define _nsLocalFileOS2_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIFactory.h"
#include "nsLocalFile.h"

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <errno.h>

#define NSRESULT_FOR_RETURN(ret) (!(ret) ? NS_OK : NSRESULT_FOR_ERRNO())

inline nsresult
nsresultForErrno(int err)
{
#ifdef DEBUG
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

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    // nsIFile interface
    NS_DECL_NSIFILE
    
    // nsILocalFile interface
    NS_DECL_NSILOCALFILE

private:

    // String guaranteed to be native path
    nsCString   mPath;
    nsresult    CheckDrive(const char* inPath);

    // Filehandling method
    nsresult    CopyMove(nsIFile *newParentDir, const char *newName, PRBool move);

    // stat caching members and inline methods
    PRBool      mHaveStatCached;
    struct stat mStatCache;
   
    void        SetNoStatCache() { mHaveStatCached = PR_FALSE; }

    nsresult    LoadStatCache() {
        if (stat((const char*)mPath, &mStatCache) == -1) {
#ifdef DEBUG
            fprintf(stderr, "stat(%s) failed; errno: %d\n", (const char *)mPath, errno);
#endif
            return NS_ERROR_FAILURE;
        }
        mHaveStatCached = PR_TRUE;
        return NS_OK;
    }

    // Check path for system-reserved chars (inPath = whatever follows drive colon, if any)
    PRBool      ValidatePath(const char* inPath) {
        return ((strpbrk(inPath, "<>:\"|") == NULL) ? PR_TRUE : PR_FALSE);
    }
};
#endif  // _nsLocalFileOS2_H_
