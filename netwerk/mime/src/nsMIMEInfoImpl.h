/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef __nsmimeinfoimpl_h___
#define __nsmimeinfoimpl_h___

#include "nsIMIMEInfo.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

class nsMIMEInfoImpl : public nsIMIMEInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMIMEINFO

    // nsMIMEInfoImpl methods
    nsMIMEInfoImpl();
    nsMIMEInfoImpl(const char *aMIMEType);
    virtual ~nsMIMEInfoImpl() {};
    PRUint32 GetExtCount();           // returns the number of extensions associated.

    // member variables
    nsCStringArray      mExtensions; // array of file extensions associated w/ this MIME obj
    nsAutoString        mDescription; // human readable description
    nsCOMPtr<nsIURI>    mURI;         // URI pointing to data associated w/ this obj      
		PRUint32						mMacType, mMacCreator; // Mac file type and creator
protected:
    nsCString					     mMIMEType;
    nsCOMPtr<nsIFile>      mPreferredApplication; // preferred application associated with this type.
    nsMIMEInfoHandleAction mPreferredAction; // preferred action to associate with this type
    nsString               mPreferredAppDescription;

    void CheckPrefForMimeType(const char * prefName, PRBool * aMimeTypeIsPresent);
    void SetRememberPrefForMimeType(const char * prefName);
};

#endif //__nsmimeinfoimpl_h___
