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

#ifndef nsICookieStorage_h___
#define nsICookieStorage_h___

#include "nsISupports.h"

// {c8c05100-cfdb-11d2-bab8-b088e084e5bc}
#define NS_ICOOKIESTORAGE_IID \
{ 0xc8c05100, 0xcfdb, 0x11d2, { 0xba, 0xb8, 0xb0, 0x88, 0xe0, 0x84, 0xe5, 0xbc } }

// {c8c05101-cfdb-11d2-bab8-b088e084e5bc}
#define NS_COOKIESTORAGE_CID \
{ 0xc8c05101, 0xcfdb, 0x11d2, { 0xba, 0xb8, 0xb0, 0x88, 0xe0, 0x84, 0xe5, 0xbc } }

/**
 * Stores/retrieves cookies. Cookies are strings associated with specific URLs.
 * They can be up to 4096 bytes each. This is intended to be used by plugins
 * to access browser cookies.
 */
class nsICookieStorage : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOOKIESTORAGE_IID)

	/**
	 * Retrieves a cookie from the browser's persistent cookie store.
	 * @param inCookieURL        URL string to look up cookie with.
	 * @param inOutCookieBuffer  buffer large enough to accomodate cookie data.
	 * @param inOutCookieSize    on input, size of the cookie buffer, on output cookie's size.
	 */
	NS_IMETHOD
	GetCookie(const char* inCookieURL, void* inOutCookieBuffer, PRUint32& inOutCookieSize) = 0;

	
	/**
	 * Stores a cookie in the browser's persistent cookie store.
	 * @param inCookieURL        URL string store cookie with.
	 * @param inCookieBuffer     buffer containing cookie data.
	 * @param inCookieSize       specifies  size of cookie data.
	 */
	NS_IMETHOD
	SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize) = 0;
};

#endif /* nsICookieStorage_h___ */
