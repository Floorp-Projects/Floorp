/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsAboutCache_h__
#define nsAboutCache_h__

#include "nsIAboutModule.h"

#include "nsString.h"
#include "nsIOutputStream.h"

#include "nsICacheVisitor.h"
#include "nsCOMPtr.h"

class nsAboutCache : public nsIAboutModule 
                   , public nsICacheVisitor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSICACHEVISITOR

    nsAboutCache() { NS_INIT_REFCNT(); }
    virtual ~nsAboutCache() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsresult  ParseURI(nsIURI * uri, nsCString &deviceID);

    nsCOMPtr<nsIOutputStream> mStream;
    nsCString                 mDeviceID;
    nsCString mBuffer;
};

#define NS_ABOUT_CACHE_MODULE_CID                    \
{ /* 9158c470-86e4-11d4-9be2-00e09872a416 */         \
    0x9158c470,                                      \
    0x86e4,                                          \
    0x11d4,                                          \
    {0x9b, 0xe2, 0x00, 0xe0, 0x98, 0x72, 0xa4, 0x16} \
}

#endif // nsAboutCache_h__
