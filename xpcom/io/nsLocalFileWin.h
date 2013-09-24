/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsLocalFileWIN_H_
#define _nsLocalFileWIN_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIFactory.h"
#include "nsILocalFileWin.h"
#include "nsIHashable.h"
#include "nsIClassInfoImpl.h"
#include "prio.h"

#include "mozilla/Attributes.h"

#include "windows.h"
#include "shlobj.h"

#include <sys/stat.h>

class nsLocalFile MOZ_FINAL : public nsILocalFileWin,
                              public nsIHashable
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();

    static nsresult nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_THREADSAFE_ISUPPORTS
    
    // nsIFile interface
    NS_DECL_NSIFILE
    
    // nsILocalFile interface
    NS_DECL_NSILOCALFILE

    // nsILocalFileWin interface
    NS_DECL_NSILOCALFILEWIN

    // nsIHashable interface
    NS_DECL_NSIHASHABLE

public:
    static void GlobalInit();
    static void GlobalShutdown();

private:
    nsLocalFile(const nsLocalFile& other);
    ~nsLocalFile() {}

    bool mDirty;            // cached information can only be used when this is false
    bool mResolveDirty;
    bool mFollowSymlinks;   // should we follow symlinks when working on this file
    
    // this string will always be in native format!
    nsString mWorkingPath;
    
    // this will be the resolved path of shortcuts, it will *NEVER* 
    // be returned to the user
    nsString mResolvedPath;

    // this string, if not empty, is the *short* pathname that represents
    // mWorkingPath
    nsString mShortWorkingPath;

    PRFileInfo64 mFileInfo64;

    void MakeDirty() 
    { 
      mDirty = true;
      mResolveDirty = true;
      mShortWorkingPath.Truncate();
    }

    nsresult ResolveAndStat();
    nsresult Resolve();
    nsresult ResolveShortcut();

    void EnsureShortPath();
    
    nsresult CopyMove(nsIFile *newParentDir, const nsAString &newName,
                      bool followSymlinks, bool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest,
                            const nsAString &newName,
                            bool followSymlinks, bool move,
                            bool skipNtfsAclReset = false);

    nsresult SetModDate(int64_t aLastModifiedTime, const PRUnichar *filePath);
    nsresult HasFileAttribute(DWORD fileAttrib, bool *_retval);
    nsresult AppendInternal(const nsAFlatString &node,
                            bool multipleComponents);
};

#endif
