/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
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
#include "nsIFactory.h"

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
    ~nsLocalFile() {}

    nsLocalFile(const nsLocalFile& other);

    // this is the flag which indicates if I can used cached information about the file
    PRPackedBool mDirty;

    // this string will alway be in native format!
    nsCString mWorkingPath;
    
    PRFileInfo64  mFileInfo64;
    
    void MakeDirty();
    nsresult Stat();
    
    nsresult CopyMove(nsIFile *newParentDir, const nsACString &newName, PRBool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const nsACString &newName, PRBool move);

    nsresult SetModDate(PRInt64 aLastModifiedTime);
};

#endif
