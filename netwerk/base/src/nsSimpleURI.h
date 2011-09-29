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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSimpleURI_h__
#define nsSimpleURI_h__

#include "nsIURL.h"
#include "nsAgg.h"
#include "nsISerializable.h"
#include "nsIIPCSerializable.h"
#include "nsString.h"
#include "nsIClassInfo.h"
#include "nsIMutable.h"

#define NS_THIS_SIMPLEURI_IMPLEMENTATION_CID         \
{ /* 0b9bb0c2-fee6-470b-b9b9-9fd9462b5e19 */         \
    0x0b9bb0c2,                                      \
    0xfee6,                                          \
    0x470b,                                          \
    {0xb9, 0xb9, 0x9f, 0xd9, 0x46, 0x2b, 0x5e, 0x19} \
}

class nsSimpleURI : public nsIURI,
                    public nsISerializable,
                    public nsIIPCSerializable,
                    public nsIClassInfo,
                    public nsIMutable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSIIPCSERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSIMUTABLE

    // nsSimpleURI methods:

    nsSimpleURI();
    virtual ~nsSimpleURI();

protected:
    // enum used in a few places to specify how .ref attribute should be handled
    enum RefHandlingEnum {
        eIgnoreRef,
        eHonorRef
    };

    // Helper to share code between Equals methods.
    virtual nsresult EqualsInternal(nsIURI* other,
                                    RefHandlingEnum refHandlingMode,
                                    bool* result);

    // Helper to be used by inherited classes who want to test
    // equality given an assumed nsSimpleURI.  This must NOT check
    // the passed-in other for QI to our CID.
    bool EqualsInternal(nsSimpleURI* otherUri, RefHandlingEnum refHandlingMode);

    // NOTE: This takes the refHandlingMode as an argument because
    // nsSimpleNestedURI's specialized version needs to know how to clone
    // its inner URI.
    virtual nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode);

    // Helper to share code between Clone methods.
    virtual nsresult CloneInternal(RefHandlingEnum refHandlingMode,
                                   nsIURI** clone);
    
    nsCString mScheme;
    nsCString mPath; // NOTE: mPath does not include ref, as an optimization
    nsCString mRef;  // so that URIs with different refs can share string data.
    bool mMutable;
    bool mIsRefValid; // To distinguish between empty-ref and no-ref.
};

#endif // nsSimpleURI_h__
