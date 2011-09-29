/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
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

#ifndef nsJARURI_h__
#define nsJARURI_h__

#include "nsIJARURI.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsINestedURI.h"

#define NS_THIS_JARURI_IMPL_CID                      \
{ /* 9a55f629-730b-4d08-b75b-fa7d9570a691 */         \
    0x9a55f629,                                      \
    0x730b,                                          \
    0x4d08,                                          \
    {0xb7, 0x5b, 0xfa, 0x7d, 0x95, 0x70, 0xa6, 0x91} \
}

#define NS_JARURI_CLASSNAME \
    "nsJARURI"
#define NS_JARURI_CID                                \
{ /* 245abae2-b947-4ded-a46d-9829d3cca462 */         \
    0x245abae2,                                      \
    0xb947,                                          \
    0x4ded,                                          \
    {0xa4, 0x6d, 0x98, 0x29, 0xd3, 0xcc, 0xa4, 0x62} \
}


class nsJARURI : public nsIJARURI,
                 public nsISerializable,
                 public nsIClassInfo,
                 public nsINestedURI
{
public:    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURL
    NS_DECL_NSIJARURI
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSINESTEDURI

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_THIS_JARURI_IMPL_CID)

    // nsJARURI
    nsJARURI();
    virtual ~nsJARURI();
   
    nsresult Init(const char *charsetHint);
    nsresult FormatSpec(const nsACString &entryPath, nsACString &result,
                        bool aIncludeScheme = true);
    nsresult CreateEntryURL(const nsACString& entryFilename,
                            const char* charset,
                            nsIURL** url);
    nsresult SetSpecWithBase(const nsACString& aSpec, nsIURI* aBaseURL);

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

    // Helper to share code between Clone methods.
    nsresult CloneWithJARFileInternal(nsIURI *jarFile,
                                      RefHandlingEnum refHandlingMode,
                                      nsIJARURI **result);
    nsCOMPtr<nsIURI> mJARFile;
    // mJarEntry stored as a URL so that we can easily access things
    // like extensions, refs, etc.
    nsCOMPtr<nsIURL> mJAREntry;
    nsCString        mCharsetHint;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsJARURI, NS_THIS_JARURI_IMPL_CID)

#endif // nsJARURI_h__
