/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsExternalProtocol_h___
#define nsExternalProtocol_h___

#include "nsIProtocolHandler.h"
#include "nsIExternalProtocol.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_EXTERNALPROTOCOL_CID					\
{ /* 99BD73D1-F835-11d3-AA27-00A0CC26DA63 */    \
	0x99bd73d1, 0xf835, 0x11d3,					\
	{0xaa, 0x27, 0x0, 0xa0, 0xcc, 0x26, 0xda, 0x63 }}



class nsExternalProtocol : public nsIExternalProtocol
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER
	NS_DECL_NSIEXTERNALPROTOCOL

	nsExternalProtocol();
	virtual ~nsExternalProtocol();
  /* additional members */

protected:
		// Launch via an installed helper, if the helper fails
		// then we will try and launch using our the default
		// external app mechanism.  (If that fails too, then 
		// we fall through to the default handler.)
	nsresult	LaunchViaHelper( nsIURI *pUri);
	
	nsresult	DefaultLaunch( nsIURI *pUri);

	nsCString	m_schemeName;
	char *		m_pData;	// some data to use if you need it
};


#endif // nsExternalProtocol_h__

