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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Doug Turner <dougt@netscape.com>
 */

#ifndef _nsLocalFileWIN_H_
#define _nsLocalFileWIN_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIFactory.h"

#include "windows.h"

// For older version (<6.0) of the VC Compiler
#if (_MSC_VER == 1100)
#include <objbase.h>
DEFINE_OLEGUID(IID_IPersistFile, 0x0000010BL, 0, 0);
#endif

#include "shlobj.h"

#include <sys/stat.h>

class nsLocalFile : public nsILocalFile
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();

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
    nsLocalFile(const nsLocalFile& other);
    ~nsLocalFile() {}

    // this is the flag which indicates if I can used cached information about the file
    PRPackedBool mDirty;
    PRPackedBool mLastResolution;
    
    PRPackedBool mFollowSymlinks;
    
    // this string will alway be in native format!
    nsCString mWorkingPath;
    
    // this will be the resolve path which will *NEVER* be return to the user
    nsCString mResolvedPath;
    
    PRFileInfo64 mFileInfo64;

    static PRBool mFSCharsetIsUTF8;

    void MakeDirty() { mDirty = PR_TRUE; }
    nsresult ResolveAndStat(PRBool resolveTerminal);
    nsresult ResolvePath(const char* workingPath, PRBool resolveTerminal, char** resolvedPath);
    
    nsresult CopyMove(nsIFile *newParentDir, const nsACString &newName, PRBool followSymlinks, PRBool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const nsACString &newName, PRBool followSymlinks, PRBool move);

    nsresult SetModDate(PRInt64 aLastModifiedTime, PRBool resolveTerminal);
};

#endif
