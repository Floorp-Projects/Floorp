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
#define INCL_DOSSESMGR
#define INCL_DOSMODULEMGR
#define INCL_DOSNLS
#define INCL_DOSMISC
#define INCL_WINCOUNTRY
#define INCL_WINWORKPLACE

#include <os2.h>

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

public:
    static void GlobalInit();
    static void GlobalShutdown();

private:

    // this is the flag which indicates if I can used cached information about the file
    PRPackedBool mDirty;
    PRPackedBool mLastResolution;
    PRPackedBool mFollowSymlinks;

    // this string will alway be in native format!
    nsCString mWorkingPath;
    
    // this will be the resolve path which will *NEVER* be return to the user
    nsCString mResolvedPath;

    PRFileInfo64  mFileInfo64;
    
    void MakeDirty();
    nsresult ResolveAndStat(PRBool resolveTerminal);
    nsresult ResolvePath(const char* workingPath, PRBool resolveTerminal, char** resolvedPath);
    
    nsresult CopyMove(nsIFile *newParentDir, const nsACString &newName, PRBool followSymlinks, PRBool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const nsACString &newName, PRBool followSymlinks, PRBool move);

    nsresult SetModDate(PRInt64 aLastModifiedTime, PRBool resolveTerminal);
};

#endif
