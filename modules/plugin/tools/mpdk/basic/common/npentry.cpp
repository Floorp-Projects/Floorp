/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * 
 */

/**********************************************************************
*
* npentry.cpp
*
* Netscape entry points for XPCom registration
* 
***********************************************************************/

#include "xplat.h"
#include "plugin.h"
#include "nsString.h"

#include "dbg.h"

char szAppName[] = "*** Basic60";

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kBasicPluginCID, NS_BASICPLUGIN_CID);

extern PRUint32 gPluginObjectCount;
extern PRBool gPluginLocked;

extern "C" NS_EXPORT 
nsresult NSGetFactory(nsISupports* serviceMgr,
                                   const nsCID &aCID,
                                   const char *aClassName,
                                   const char *aContractID,
                                   nsIFactory **aFactory)
{
  dbgOut1("NSGetFactory");

  if(aFactory == nsnull)
    return NS_ERROR_NULL_POINTER; 

  *aFactory = nsnull;
  
  CPlugin* inst;

  if (aCID.Equals(kBasicPluginCID)) 
    inst = new CPlugin();
  else if (aCID.Equals(kPluginCID)) 
    inst = new CPlugin();
  else
	  return NS_NOINTERFACE;

  if(inst == nsnull)
    return NS_ERROR_OUT_OF_MEMORY; 

    nsresult rv = inst->QueryInterface(kIFactoryIID, (void **)aFactory); 

    if(NS_FAILED(rv)) 
      delete inst; 

		return rv;

  return 0;
}

extern "C" NS_EXPORT 
PRBool NSCanUnload(nsISupports* serviceMgr)
{
  dbgOut1("NSCanUnload");

  return (gPluginObjectCount == 1 && !gPluginLocked);
}

extern "C" NS_EXPORT 
nsresult NSRegisterSelf(nsISupports *serviceMgr, const char *path)
{
  dbgOut1("NSRegisterSelf");

  nsresult rv = NS_OK;

  char buf[255];

  nsCString contractID(NS_INLINE_PLUGIN_CONTRACTID_PREFIX);

  // We will use the service manager to obtain the component
  // manager, which will enable us to register a component
  // with a ContractID (text string) instead of just the CID.
  nsIServiceManager *sm;

  // We can get the IID of an interface with the static GetIID() method as well
  rv = serviceMgr->QueryInterface(NS_GET_IID(nsIServiceManager), (void **)&sm);

  if(NS_FAILED(rv))
      return rv;

  nsIComponentManager *cm;

  rv = sm->GetService(kComponentManagerCID, NS_GET_IID(nsIComponentManager), (nsISupports **)&cm);

  if(NS_FAILED(rv)) 
  {
    NS_RELEASE(sm);
    return rv;
  }

  contractID += PLUGIN_MIME_TYPE;
  contractID.ToCString(buf, 255);

  rv = cm->RegisterComponent(kBasicPluginCID, PLUGIN_NAME, buf, path, PR_TRUE, PR_TRUE);

  sm->ReleaseService(kComponentManagerCID, cm);

  NS_RELEASE(sm);

  return rv;
}

extern "C" NS_EXPORT 
nsresult NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
  dbgOut1("NSUnregisterSelf");

	nsresult rv = NS_OK;

	nsIServiceManager *sm;

	rv = serviceMgr->QueryInterface(NS_GET_IID(nsIServiceManager), (void **)&sm);

	if(NS_FAILED(rv))
		return rv;

	nsIComponentManager *cm;

	rv = sm->GetService(kComponentManagerCID, NS_GET_IID(nsIComponentManager), (nsISupports **)&cm);

	if(NS_FAILED(rv)) 
  {
		NS_RELEASE(sm);
		return rv;
	}

	rv = cm->UnregisterComponent(kBasicPluginCID, path);

	sm->ReleaseService(kComponentManagerCID, cm);

	NS_RELEASE(sm);

	return rv;
}
