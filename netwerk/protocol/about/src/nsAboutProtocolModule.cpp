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
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsAboutProtocolHandler.h"
#include "nsAboutBlank.h"
#include "nsAboutBloat.h"
#include "nsAboutCredits.h"
#include "mzAboutMozilla.h"

static nsModuleComponentInfo components[] = 
{
   { "About Protocol Handler", 
     NS_ABOUTPROTOCOLHANDLER_CID,
     NS_NETWORK_PROTOCOL_PROGID_PREFIX "about", 
     nsAboutProtocolHandler::Create
   },

   { "about:blank", 
     NS_ABOUT_BLANK_MODULE_CID,
     NS_ABOUT_MODULE_PROGID_PREFIX "blank", 
     nsAboutBlank::Create
   },
   
   { "about:bloat", 
     NS_ABOUT_BLOAT_MODULE_CID,
     NS_ABOUT_MODULE_PROGID_PREFIX "bloat", 
     nsAboutBloat::Create
   },

   { "about:credits",
     NS_ABOUT_CREDITS_MODULE_CID,
     NS_ABOUT_MODULE_PROGID_PREFIX "credits",
     nsAboutCredits::Create
   },

   { "about:mozilla",
     MZ_ABOUT_MOZILLA_MODULE_CID,
     NS_ABOUT_MODULE_PROGID_PREFIX "mozilla",
     mzAboutMozilla::Create
   },	     

   { "about:cache", 
     NS_ABOUT_CACHE_MODULE_CID,
     NS_ABOUT_MODULE_PROGID_PREFIX "cache", 
     nsAboutCache::Create
   },
};

NS_IMPL_NSGETMODULE("nsAboutProtocolModule", components);

