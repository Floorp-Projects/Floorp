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
 *     Steve Dagley <sdagley@netscape.com>
 */

#ifndef _nsLocalFileMAC_H_
#define _nsLocalFileMAC_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIFactory.h"
#include "nsLocalFile.h"

#include <Files.h>

class NS_COM nsLocalFile : public nsILocalFile
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();
    virtual ~nsLocalFile();

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    // nsIFile interface
    NS_DECL_NSIFILE
    
    // nsILocalFile interface
    NS_DECL_NSILOCALFILE

private:

    // this is the flag which indicates if I can used cached information about the file
    PRBool mStatDirty;

    // this string will alway be in native format!
    nsCString mWorkingPath;
    
    // this will be the resolved path which will *NEVER* be returned to the user
    nsCString mResolvedPath;
    
    // The Mac data structure for a file system object
    FSSpec    mSpec;
    
    void MakeDirty();

    nsresult CopyMove(nsIFile *newParentDir, const char *newName, PRBool followSymlinks, PRBool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const char * newName, PRBool followSymlinks, PRBool move);

};

#endif

