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

#ifndef nsIPluginHost_h___
#define nsIPluginHost_h___

#include "xp_core.h"
#include "nsplugindefs.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIStreamListener.h"

class nsIPlugin;
class nsIURL;

#define NS_IPLUGINHOST_IID \
{ 0x264c0640, 0x1c31, 0x11d2, \
{ 0xa8, 0x2e, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

struct nsIPluginHost : public nsIFactory
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINHOST_IID)

  NS_IMETHOD
  Init(void) = 0;

  NS_IMETHOD
  Destroy(void) = 0;

  NS_IMETHOD
  LoadPlugins(void) = 0;
  
  NS_IMETHOD
  GetPluginFactory(const char *aMimeType, nsIPlugin** aPlugin) = 0;

  NS_IMETHOD
  InstantiateEmbededPlugin(const char *aMimeType, nsIURL* aURL, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec, nsIStreamListener *&aStreamListener, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  SetUpPluginInstance(const char *aMimeType, nsIURL *aURL, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  IsPluginAvailableForType(const char* aMimeType) = 0;

  NS_IMETHOD
  IsPluginAvailableForExtension(const char* aExtension, const char* &aMimeType) = 0;

};

#endif // nsIPluginHost_h___
