/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
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
 * The Original Code is nsChromeURL.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#ifndef nsChromeURL_h_
#define nsChromeURL_h_

#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsIChromeURL.h"
#include "nsCOMPtr.h"

// 5f0dbeb4-6dde-4219-b516-f160a1639064
#define NS_CHROMEURL_CID \
  { 0x5f0dbeb4, 0x6dde, 0x4219, \
    { 0xb5, 0x16, 0xf1, 0x60, 0xa1, 0x63, 0x90, 0x64 } }

// 5f4b99a7-7217-4307-91b9-a3ea6c7c369b
#define NS_CHROMEURL_IMPL_IID \
  { 0x5f4b99a7, 0x7217, 0x4307, \
    { 0x91, 0xb9, 0xa3, 0xea, 0x6c, 0x7c, 0x36, 0x9b } }

/**
 * nsChromeURL is a wrapper for nsStandardURL that implements all the
 * interfaces that nsStandardURL does just to override the Equals method
 * and provide a mechanism for getting at the resolved URL.
 */
class nsChromeURL : public nsIChromeURL,
                    public nsIStandardURL,
                    public nsISerializable,
                    public nsIClassInfo
{
public:
    // The constructor doesn't do anything; callers are required to call
    // either |Init| or |Read| and ensure it succeeds before doing
    // anything else.
    nsChromeURL() {}
    nsresult Init(const nsACString &aSpec, const char *aCharset,
                  nsIURI *aBaseURI);

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CHROMEURL_IMPL_IID);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIURL
    NS_DECL_NSICHROMEURL
    NS_DECL_NSISTANDARDURL
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO

protected:
    // Canonical, but still in the "chrome" scheme.  Most methods
    // forwarded here.
    nsCOMPtr<nsIURL> mChromeURL;

    // Converted to some other scheme (e.g., "jar").
    nsCOMPtr<nsIURI> mConvertedURI;
};

#endif /* nsChromeURL_h_ */
