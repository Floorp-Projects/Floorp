/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef ____nsindexedtohtml___h___
#define ____nsindexedtohtml___h___

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIStreamConverter.h"
#include "nsXPIDLString.h"
#include "nsIDirIndexListener.h"
#include "nsIDateTimeFormat.h"
#include "nsIStringBundle.h"

#define NS_NSINDEXEDTOHTMLCONVERTER_CID \
{ 0xcf0f71fd, 0xfafd, 0x4e2b, {0x9f, 0xdc, 0x13, 0x4d, 0x97, 0x2e, 0x16, 0xe2} }


class nsIndexedToHTML : public nsIStreamConverter,
                        public nsIDirIndexListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIDIRINDEXLISTENER

    nsIndexedToHTML();
    virtual ~nsIndexedToHTML();

    nsresult Init(nsIStreamListener *aListener);

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsIndexedToHTML* _s = new nsIndexedToHTML();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = _s->QueryInterface(aIID, aResult);
        return rv;
    }

protected:
    
    void FormatSizeString(PRUint32 inSize, nsString& outSizeString);

protected:
    nsCOMPtr<nsIDirIndexParser>     mParser;
    nsCOMPtr<nsIStreamListener>     mListener; // final listener (consumer)

    nsCOMPtr<nsIDateTimeFormat> mDateTime;
    nsCOMPtr<nsIStringBundle> mBundle;
};

#endif

