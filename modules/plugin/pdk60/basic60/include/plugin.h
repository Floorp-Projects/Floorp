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

#ifndef __PLUGIN_HPP__
#define __PLUGIN_HPP__

#include "nsplugin.h"
#include "nsIServiceManager.h"

#include "basiccid.h"

/*******************************************************************
*
* CPlugin class is the class for all our plugins. We crteate all 
* our plugin instances using one instance of this class.
*
********************************************************************/

class CPlugin : public nsIPlugin
{
protected:
  nsIPluginManager* mPluginManager;
	nsIServiceManager* mServiceManager;

public:
  NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, const char* aPluginMIMEType, void **aResult);
  NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);
  NS_IMETHOD LockFactory(PRBool aLock);

  // This call initializes the plugin and will be called before any new
  // instances are created. It is passed browserInterfaces on which QueryInterface
  // may be used to obtain an nsIPluginManager, and other interfaces.
  NS_IMETHOD Initialize(void);

  // (Corresponds to NPP_Shutdown.)
  // Called when the browser is done with the plugin factory, or when
  // the plugin is disabled by the user.
  NS_IMETHOD Shutdown(void);

  // (Corresponds to NPP_GetMIMEDescription.)
  NS_IMETHOD GetMIMEDescription(const char* *result);

  // (Corresponds to NPP_GetValue.)
  NS_IMETHOD GetValue(nsPluginVariable variable, void *value);

  CPlugin();
  virtual ~CPlugin(void);

  NS_DECL_ISUPPORTS

  nsIPluginManager* GetPluginManager(void) { return mPluginManager; }
};

#define PLUGIN_NAME             "Basic Plugin"
#define PLUGIN_DESCRIPTION      "Basic Plugin Example"
#define PLUGIN_MIMEDESCRIPTION  "application/basicplugin-test:bpl:Basic Plugin";
#define PLUGIN_MIME_TYPE "application/basicplugin-test"

#endif // __PLUGIN_HPP__
