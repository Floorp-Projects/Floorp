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
  
/*
	Text import module
*/

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsExternalProtocol.h"


static NS_DEFINE_CID(kExternalProtocolCID,    	NS_EXTERNALPROTOCOL_CID);


NS_GENERIC_FACTORY_CONSTRUCTOR( nsExternalProtocol)

static NS_IMETHODIMP nsExternalMailtoConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	nsresult rv = nsExternalProtocolConstructor( aOuter, aIID, aResult);
	if (NS_SUCCEEDED( rv)) {
		nsIExternalProtocol *pProto = (nsIExternalProtocol *) (*aResult);
		pProto->SetScheme( "mailto");
	}
	return( rv);
}

static nsModuleComponentInfo components[] = {
    {	"External protocol handler", 
		NS_EXTERNALPROTOCOL_CID,
		"@mozilla.org/externalprotocol/default;1", 
		nsExternalProtocolConstructor,
		nsnull,
		nsnull
	},
    {	"External mailto protocol handler", 
		NS_EXTERNALMAILTO_CID,
		NS_EXTERNALMAILTO_CONTRACTID, 
		nsExternalMailtoConstructor,
		nsnull,
		nsnull
	}
};


NS_IMPL_NSGETMODULE("nsExternalProtocolModule", components)


