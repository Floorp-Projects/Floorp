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
#include "nsString.h"
#include "nsIHashable.h"

class nsDirEnumerator;

//*****************************************************************************
//  nsLocalFile
//
// The native charset of this implementation is UTF-8. The Unicode used by the
// Mac OS file system is decomposed, so "Native" versions of these routines will
// always use decomposed Unicode (NFD). Their "non-Native" counterparts are 
// intended to be simple wrappers which call the "Native" version and convert 
// between UTF-8 and UTF-16. All the work is done on the "Native" side except
// for the conversion to NFC (composed Unicode) done in "non-Native" getters.
//*****************************************************************************

class NS_COM nsLocalFile : public nsILocalFileMac,
                           public nsIHashable
{
    friend class nsDirEnumerator;
    
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)
    
                        nsLocalFile();

    static NS_METHOD    nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFILE
    NS_DECL_NSILOCALFILE
    NS_DECL_NSILOCALFILEMAC
    NS_DECL_NSIHASHABLE

public:

    static void         GlobalInit();
    static void         GlobalShutdown();
    
private:
                        ~nsLocalFile();

protected:
                        nsLocalFile(const nsLocalFile& src);
    
    nsresult            SetBaseRef(CFURLRef aCFURLRef); // Does CFRetain on aCFURLRef
    nsresult            UpdateTargetRef();
    
    nsresult            GetFSRefInternal(FSRef& aFSSpec, PRBool bForceUpdateCache = PR_TRUE);
    nsresult            GetPathInternal(nsACString& path);  // Returns path WRT mFollowLinks
    nsresult            EqualsInternal(nsISupports* inFile,
                                       PRBool aUpdateCache, PRBool *_retval);

    nsresult            CopyInternal(nsIFile* newParentDir,
                                     const nsAString& newName,
                                     PRBool followLinks);

    static PRInt64      HFSPlustoNSPRTime(const UTCDateTime& utcTime);
    static void         NSPRtoHFSPlusTime(PRInt64 nsprTime, UTCDateTime& utcTime);
    static nsresult     CFStringReftoUTF8(CFStringRef aInStrRef, nsACString& aOutStr);

protected:
    CFURLRef            mBaseRef;           // The FS object we represent
    CFURLRef            mTargetRef;         // If mBaseRef is an alias, its target

    FSRef               mCachedFSRef;
    PRPackedBool        mCachedFSRefValid;
    
    PRPackedBool        mFollowLinks;
    PRPackedBool        mFollowLinksDirty;

    static const char         kPathSepChar;
    static const PRUnichar    kPathSepUnichar;
    static const PRInt64      kJanuaryFirst1970Seconds;    
};

#endif // nsLocalFileMac_h__
