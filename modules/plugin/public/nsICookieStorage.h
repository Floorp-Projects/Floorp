/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
