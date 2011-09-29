/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   Brodie Thiesfield <brofield@jellycan.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "windows.h"

// For older version (<6.0) of the VC Compiler
#if (_MSC_VER == 1100)
#include <objbase.h>
DEFINE_OLEGUID(IID_IPersistFile, 0x0000010BL, 0, 0);
#endif

#include "shlobj.h"

#include <sys/stat.h>

typedef LPITEMIDLIST (WINAPI *ILCreateFromPathWPtr)(PCWSTR);
typedef HRESULT (WINAPI *SHOpenFolderAndSelectItemsPtr)(PCIDLIST_ABSOLUTE, UINT, 
                                                        PCUITEMID_CHILD_ARRAY,
                                                        DWORD);

class nsLocalFile : public nsILocalFileWin,
                    public nsIHashable
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
    nsLocalFile();

    static nsresult nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
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

    void MakeDirty() { mDirty = PR_TRUE; mShortWorkingPath.Truncate(); }

    nsresult ResolveAndStat();
    nsresult ResolveShortcut();

    void EnsureShortPath();
    
    nsresult CopyMove(nsIFile *newParentDir, const nsAString &newName,
                      bool followSymlinks, bool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest,
                            const nsAString &newName,
                            bool followSymlinks, bool move,
                            bool skipNtfsAclReset = false);

    nsresult SetModDate(PRInt64 aLastModifiedTime, const PRUnichar *filePath);
    nsresult HasFileAttribute(DWORD fileAttrib, bool *_retval);
    nsresult AppendInternal(const nsAFlatString &node,
                            bool multipleComponents);
    nsresult RevealClassic(); // Reveals the path using explorer.exe cmdline
    nsresult RevealUsingShell(); // Uses newer shell API to reveal the path

    static ILCreateFromPathWPtr sILCreateFromPathW;
    static SHOpenFolderAndSelectItemsPtr sSHOpenFolderAndSelectItems;
    static PRLibrary *sLibShell;
};

#endif
