/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsLocalFileMac_h__
#define nsLocalFileMac_h__

#include "nsILocalFileMac.h"
#include "nsLocalFile.h"
#include "nsString.h"

#include <deque>
using namespace std;

class nsDirEnumerator;

//*****************************************************************************
//  nsLocalFile
//*****************************************************************************

class NS_COM nsLocalFile : public nsILocalFileMac
{
    friend class nsDirEnumerator;
    
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
                        nsLocalFile();
    virtual             ~nsLocalFile();

    static NS_METHOD    nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFILE
    NS_DECL_NSILOCALFILE
    NS_DECL_NSILOCALFILEMAC

public:

    static void         GlobalInit();
    static void         GlobalShutdown();
    
protected:
                        nsLocalFile(const FSRef& aFSRef, const nsAString& aRelativePath);
                        nsLocalFile(const nsLocalFile& src);
    
    nsresult            Resolve();
    nsresult            GetFSRefInternal(FSRef& aFSSpec);
    nsresult            GetPathInternal(nsACString& path);  // Returns path WRT mFollowLinks
    nsresult            ResolveNonExtantNodes(PRBool aCreateDirs);

    nsresult            MoveCopy(nsIFile* newParentDir, const nsAString &newName, PRBool isCopy, PRBool followLinks);

    static PRInt64      HFSPlustoNSPRTime(const UTCDateTime& utcTime);
    static void         NSPRtoHFSPlusTime(PRInt64 nsprTime, UTCDateTime& utcTime);
    
protected:
    FSRef               mFSRef;
    deque<nsString>     mNonExtantNodes;
    
    FSRef               mTargetFSRef; // If mFSRef is an alias file, its target

    PRPackedBool        mFollowLinks;

    PRPackedBool        mIdentityDirty;
    PRPackedBool        mFollowLinksDirty;

    static PRInt64      kJanuaryFirst1970Seconds;    
    static PRUnichar    kPathSepUnichar;
    static char         kPathSepChar;
    static FSRef        kInvalidFSRef;
    static FSRef        kRootFSRef;
};

#endif // nsLocalFileMac_h__
