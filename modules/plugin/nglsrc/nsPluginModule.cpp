/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIGenericFactory.h"
#include "nsIPluginManager.h"
#include "nsPluginsCID.h"
#include "nsPluginHostImpl.h"
#include "nsPluginDocLoaderFactory.h"
#include "ns4xPlugin.h"

static nsModuleComponentInfo gComponentInfo[] = {
  { "Plugin Host",
    NS_PLUGIN_HOST_CID,
    "@mozilla.org/plugin/host;1",
    nsPluginHostImpl::Create },

  { "Plugin Manager",
    NS_PLUGINMANAGER_CID,
    "@mozilla.org/plugin/manager;1",
    nsPluginHostImpl::Create },

  { "Plugin Doc Loader Factory",
    NS_PLUGINDOCLOADERFACTORY_CID,
    "@mozilla.org/plugin/doc-loader/factory;1",
    nsPluginDocLoaderFactory::Create },
};

PR_STATIC_CALLBACK(void)
nsPluginModuleDtor(nsIModule *self)
{
  ns4xPlugin::ReleaseStatics();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsPluginModule, gComponentInfo,
                              nsPluginModuleDtor);
