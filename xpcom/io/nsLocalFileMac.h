/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Steve Dagley <sdagley@netscape.com>
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

#ifndef _nsLocalFileMAC_H_
#define _nsLocalFileMAC_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsILocalFileMac.h"
#include "nsIFactory.h"

#include <Processes.h>

class NS_COM nsLocalFile : public nsILocalFileMac
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

    // nsILocalFileMac interface
    NS_DECL_NSILOCALFILEMAC

public:

    static void GlobalInit();
    static void GlobalShutdown();

private:
    ~nsLocalFile() {}

protected:
    void        MakeDirty();
    nsresult    ResolveAndStat();
    nsresult    UpdateCachedCatInfo(PRBool forceUpdate);

    nsresult    FindAppOnLocalVolumes(OSType sig, FSSpec &outSpec);

    nsresult    FindRunningAppBySignature(OSType sig, FSSpec& outSpec, ProcessSerialNumber& outPsn);
    nsresult    FindRunningAppByFSSpec(const FSSpec& appSpec, ProcessSerialNumber& outPsn);

    nsresult    MyLaunchAppWithDoc(const FSSpec& appSpec, const FSSpec* aDocToLoad, PRBool aLaunchInBackground);

    nsresult    TestFinderFlag(PRUint16 flagMask, PRBool *outFlagSet, PRBool testTargetSpec = PR_TRUE);

    nsresult    MoveCopy( nsIFile* newParentDir, const nsACString &newName, PRBool isCopy, PRBool followLinks );

    // Passing nsnull for the extension uses leaf name
    nsresult    SetOSTypeAndCreatorFromExtension(const char* extension = nsnull);

    nsresult    ExtensionIsOnExceptionList(const char *extension, PRBool *onList);

private:
    nsLocalFile(const nsLocalFile& srcFile);
    nsLocalFile(const FSSpec& aSpec, const nsACString& aAppendedPath);
    
    // Copies all members except mRefCnt, copies mCachedCatInfo only if it's valid.
    nsLocalFile& operator=(const nsLocalFile& rhs);

    PRPackedBool    mFollowLinks;
    PRPackedBool    mFollowLinksDirty;

    // The object we specify is always: mSpec + mAppendedPath.
    // mSpec is a valid spec in that its parent is known to exist.
    // Appending nodes with Append() will update mSpec immediately
    // if the appended node exists. If not, appended nodes are stored
    // in mAppendedPath.

    PRPackedBool    mSpecDirty;    
    FSSpec          mSpec;
    nsCString       mAppendedPath;
    FSSpec          mTargetSpec;        // If mSpec is an alias, this is the resolved alias.

    CInfoPBRec      mCachedCatInfo;     // cached file info, for the GetFSSpec() spec
    PRPackedBool    mCatInfoDirty;      // does this need to be updated?

    OSType          mType, mCreator;

    static void     InitClassStatics();

    static OSType   sCurrentProcessSignature;
    static PRBool   sHasHFSPlusAPIs;
    static PRBool   sRunningOSX;
};

#endif

