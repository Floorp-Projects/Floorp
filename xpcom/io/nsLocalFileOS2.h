/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/26/2000       IBM Corp.      Make more like Windows.
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
#define INCL_DOSNLS
#define INCL_WINCOUNTRY
#include <os2.h>

#if 0 // OLDWAY
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <errno.h>

#ifdef XP_OS2_VACPP
#define ENOTDIR EBADPOS
#endif

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
#endif

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

    // this is the flag which indicates if I can used cached information about the file
    PRPackedBool mDirty;
    PRPackedBool mLastResolution;
    PRPackedBool mFollowSymlinks;

    // this string will alway be in native format!
    nsCString mWorkingPath;
    
    // this will be the resolve path which will *NEVER* be return to the user
    nsCString mResolvedPath;

#if defined(XP_PC) && !defined(XP_OS2)    
    IPersistFile* mPersistFile; 
    IShellLink*   mShellLink;
#endif
    
    PRFileInfo64  mFileInfo64;
    
    void MakeDirty();
    nsresult ResolveAndStat(PRBool resolveTerminal);
    nsresult ResolvePath(const char* workingPath, PRBool resolveTerminal, char** resolvedPath);
    
    nsresult CopyMove(nsIFile *newParentDir, const char *newName, PRBool followSymlinks, PRBool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const char * newName, PRBool followSymlinks, PRBool move);

    nsresult SetModDate(PRInt64 aLastModificationDate, PRBool resolveTerminal);
};

#endif
