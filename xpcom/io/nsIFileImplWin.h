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

#ifndef _NSIFILEIMPLWIN_H_
#define _NSIFILEIMPLWIN_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIFactory.h"
#include "nsIFileImpl.h"

#include <sys/stat.h>

class NS_COM nsIFileImpl : public nsIFile
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_IFILEIMPL_CID)
    
    nsIFileImpl();
    virtual ~nsIFileImpl();

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    // nsIFile interface
    NS_DECL_NSIFILE

private:

    // this string will alway be in native format!
    nsCString mWorkingPath;

    // this is the flag which indicates if I can used cached information about the files
    // resolved stat.
    PRBool mResolutionDirty;
    
    // this will be the resolve path which will *NEVER* be return to the user
    nsCString mResolvedPath;
    
    // this is the flag which indicates if I can used cached information about the file's
    // stat information
    PRBool mStatDirty;

    // for buffered stat calls.
    struct stat mBuffered_st;
    int mBuffered_stat_rv;
    
    nsresult resolveWorkingPath();
    
    nsresult bufferedStat(struct stat *st);
    void makeDirty();

    nsresult copymove(nsIFile *newParentDir, const char *newName, PRBool followSymlinks, PRBool move);

};

#endif