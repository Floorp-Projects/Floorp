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
 * 
 * Contributor(s): 
 *   
 */


#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsExternalProtocol.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


nsExternalProtocol::nsExternalProtocol()
{
    NS_INIT_REFCNT();
	m_pData = nsnull;
	m_schemeName = "default";
}


nsExternalProtocol::~nsExternalProtocol()
{
	if (m_pData)
		delete [] m_pData;
}



NS_IMPL_ISUPPORTS(nsExternalProtocol, NS_GET_IID(nsIExternalProtocol));



NS_IMETHODIMP nsExternalProtocol::SetScheme(const char *scheme)
{
	m_schemeName = scheme;
	return( NS_OK);
}

/* readonly attribute string scheme; */
NS_IMETHODIMP nsExternalProtocol::GetScheme(char * *aScheme)
{
	*aScheme = m_schemeName.ToNewCString();

	return( NS_OK);
}

/* readonly attribute long defaultPort; */
NS_IMETHODIMP nsExternalProtocol::GetDefaultPort(PRInt32 *aDefaultPort)
{
	*aDefaultPort = 0;
    return( NS_OK);
}

/* nsIURI newURI (in string aSpec, in nsIURI aBaseURI); */
NS_IMETHODIMP nsExternalProtocol::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
    nsresult rv;

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
    } else {
        rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                                NS_GET_IID(nsIURI),
                                                (void**)&url);
    }
    if (NS_FAILED(rv))
        return rv;

    rv = url->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *_retval = url;
    return rv;
}

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP nsExternalProtocol::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
	if (NS_SUCCEEDED( LaunchViaHelper( aURI)))
		return( NS_ERROR_EXTERNAL_LAUNCH);

    return( DefaultLaunch( aURI));
}


nsresult nsExternalProtocol::LaunchViaHelper( nsIURI *pUri)
{
	// check the category manager for an installed external
	// handler for this protocol.  If one exists, get it
	// and try and launch the URI.  If it succeeds return NS_OK,
	// any other return value will cause the default to be called.

	return( NS_ERROR_FAILURE);
}



#ifdef XP_UNIX
// For some reason I cannot get the makefile for building the unix
// static lib to work properly.  Rather than wastew many many hours
// on it, just include the source here.  If anyone can figure out how
// to get the makefile in extprotocol/unix to work then remove this
// and add that library to the makefile in extprotocol/base - that is
// how the windows build works

#define UNIX_MAKEFILE_NO_WORKY	1

#include "../unix/nsExternalProtocolUnix.cpp"

#endif


