/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <string.h>

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsJAR.h"
#include "nsIJARFactory.h"
#include "nsRecyclingAllocator.h"
#include "nsXPTZipLoader.h"
#include "nsJARProtocolHandler.h"

extern nsRecyclingAllocator *gZlibAllocator;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPTZipLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAR)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZipReaderCache)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsJARProtocolHandler, Init)

// The list of components we register
static const nsModuleComponentInfo components[] = 
{
    { "XPT Zip Reader",
      NS_XPTZIPREADER_CID,
      NS_XPTLOADER_CONTRACTID_PREFIX "zip",
      nsXPTZipLoaderConstructor
    },
    { "Zip Reader", 
       NS_ZIPREADER_CID,
      "@mozilla.org/libjar/zip-reader;1", 
      nsJARConstructor
    },
    { "Zip Reader Cache", 
       NS_ZIPREADERCACHE_CID,
      "@mozilla.org/libjar/zip-reader-cache;1", 
      nsZipReaderCacheConstructor
    },
    { NS_JARPROTOCOLHANDLER_CLASSNAME,
      NS_JARPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar", 
      nsJARProtocolHandlerConstructor
    }
};

// Jar module shutdown hook
static void PR_CALLBACK nsJarShutdown(nsIModule *module)
{
    // Release cached buffers from zlib allocator
    delete gZlibAllocator;
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsJarModule, components, nsJarShutdown)
