/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/**********************************************************************
*
* plugin.cpp
*
* Implementation of the plugin class
* 
***********************************************************************/

#include "xplat.h"
#include "plugin.h"
#include "instbase.h"

#include "dbg.h"

PRUint32 gPluginObjectCount = 0;
PRBool gPluginLocked = PR_FALSE;

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

CPlugin::CPlugin() :
  mPluginManager(NULL), 
  mServiceManager(NULL)
{
  NS_INIT_REFCNT();

  if(nsComponentManager::CreateInstance(kCPluginManagerCID, NULL, kIPluginManagerIID, (void**)&mPluginManager) != NS_OK)
    return;

  gPluginObjectCount++;
  dbgOut2("CPlugin::CPlugin(), gPluginObjectCount = %lu", gPluginObjectCount);
}

CPlugin::~CPlugin()
{
  if(mPluginManager)
    mPluginManager->Release();

  if(mServiceManager)
    mServiceManager->Release();

  gPluginObjectCount--;

  dbgOut2("CPlugin::~CPlugin(), gPluginObjectCount = %lu", gPluginObjectCount);
}

NS_METHOD CPlugin::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  dbgOut1("CPlugin::QueryInterface");

  if (NULL == aInstancePtr) 
    return NS_ERROR_NULL_POINTER; 

  if (aIID.Equals(kISupportsIID))
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,this);
  else if (aIID.Equals(kIFactoryIID))
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(nsIFactory*,this));
  else if (aIID.Equals(kIPluginIID))
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(nsIPlugin*,this));
  else
  {
    *aInstancePtr = nsnull;
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*,*aInstancePtr));
  return NS_OK;
}

NS_IMPL_ADDREF(CPlugin);
NS_IMPL_RELEASE(CPlugin);

NS_METHOD CPlugin::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  dbgOut1("CPlugin::CreateInstance");

  CPluginInstance * inst = Platform_CreatePluginInstance(aOuter, aIID, nsnull);

  if (inst == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;

  inst->AddRef();
  *aResult = (void *)inst;

  return NS_OK;
}

NS_METHOD CPlugin::CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, const char* aPluginMIMEType, void **aResult)
{
  dbgOut1("CreatePluginInstance");

  CPluginInstance * inst = Platform_CreatePluginInstance(aOuter, aIID, aPluginMIMEType);

  if (inst == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;

  inst->AddRef();
  *aResult = (void *)inst;

  return NS_OK;
}

NS_METHOD CPlugin::LockFactory(PRBool aLock)
{
  dbgOut1("CPlugin::LockFactory");

  gPluginLocked = aLock;
  return NS_OK;
}

NS_METHOD CPlugin::Initialize()
{
  dbgOut1("CPlugin::Initialize");

  return NS_OK;
}

NS_METHOD CPlugin::Shutdown(void)
{
  dbgOut1("CPlugin::Shutdown");

  return NS_OK;
}

NS_METHOD CPlugin::GetMIMEDescription(const char **result)
{
  dbgOut1("CPlugin::GetMIMEDescription");

  *result = PLUGIN_MIMEDESCRIPTION;
  return NS_OK;
}

NS_METHOD CPlugin::GetValue(nsPluginVariable variable, void *value)
{
  dbgOut1("");

  nsresult rv = NS_OK;
  if (variable == nsPluginVariable_NameString)
    *((char **)value) = PLUGIN_NAME;
  else if (variable == nsPluginVariable_DescriptionString)
    *((char **)value) = PLUGIN_DESCRIPTION;
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}
