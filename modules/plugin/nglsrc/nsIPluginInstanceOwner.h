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

#ifndef nsIPluginInstanceOwner_h___
#define nsIPluginInstanceOwner_h___

#include "nsISupports.h"
#include "nsplugin.h"

class nsIDocument;

#define NS_IPLUGININSTANCEOWNER_IID \
{ 0x18270870, 0x32f1, 0x11d2, \
{ 0xa8, 0x30, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

struct nsIPluginInstanceOwner : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGININSTANCEOWNER_IID)

  /**
   * Let the owner know that an instance has been created
   *
   */
  NS_IMETHOD
  SetInstance(nsIPluginInstance *aInstance) = 0;

  /**
   * Get the instance associated with this owner.
   *
   */
  NS_IMETHOD
  GetInstance(nsIPluginInstance *&aInstance) = 0;

  /**
   * Get a handle to the window structure of the owner.
   * This pointer cannot be made persistant by the caller.
   *
   */
  NS_IMETHOD
  GetWindow(nsPluginWindow *&aWindow) = 0;

  /**
   * Get the display mode for the plugin instance.
   */
  NS_IMETHOD
  GetMode(nsPluginMode *aMode) = 0;

  /**
   * Create a place for the plugin to live in the owner's
   * environment. this may or may not create a window
   * depending on the windowless state of the plugin instance.
   * 
   */
  NS_IMETHOD
  CreateWidget(void) = 0;

  /**
   * Called when there is a valid target so that the proper
   * frame can be updated with new content. will not be called
   * with nsnull aTarget.
   * 
   */
  NS_IMETHOD
  GetURL(const char *aURL, const char *aTarget, void *aPostData) = 0;

  /**
   * Show a status message in the host environment.
   * 
   */
  NS_IMETHOD
  ShowStatus(const char *aStatusMsg) = 0;

  /**
   * Get the associated document.
   */
  NS_IMETHOD
  GetDocument(nsIDocument* *aDocument) = 0;
};

#endif
