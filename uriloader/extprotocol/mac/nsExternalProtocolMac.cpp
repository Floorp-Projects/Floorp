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
#include "nsExternalProtocol.h"
#include "nsXPIDLString.h"

#include <InternetConfig.h>

nsresult nsExternalProtocol::DefaultLaunch( nsIURI *pUri)
{
	nsresult rv = NS_ERROR_FAILURE;

	// use internet config to launch the uri
	nsXPIDLCString	uriStr;
	if (pUri) {
		pUri->GetSpec( getter_Copies( uriStr));
	}
	else {
		uriStr = m_schemeName;
	}
	

	OSStatus err;
	ICInstance inst;
	long startSel;
	long endSel = nsCRT::strlen( (const char *)uriStr);

	err = ICStart(&inst, 'MOSS');
	if (err == noErr) {
#if !TARGET_CARBON
		err = ICFindConfigFile( inst, 0, nil);
#endif
		if (err == noErr) {
			startSel = 0;
			err = ICLaunchURL(inst, "\p", (char *)((const char *) uriStr), endSel, &startSel, &endSel);
			if (err == noErr)
				rv = NS_ERROR_EXTERNAL_LAUNCH;
		}
 		(void) ICStop(inst);
 	}
     	
	return( rv);
}

