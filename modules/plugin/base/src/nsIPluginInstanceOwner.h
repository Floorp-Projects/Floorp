/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  GetURL(const char *aURL, const char *aTarget, 
         void *aPostData, PRUint32 aPostDataLen, 
         void *aHeadersData, PRUint32 aHeadersDataLen) = 0;

  /**
   * Show a status message in the host environment.
   * 
   */
  NS_IMETHOD
  ShowStatus(const char *aStatusMsg) = 0;

  NS_IMETHOD
  ShowStatus(const PRUnichar *aStatusMsg) = 0;

  /**
   * Get the associated document.
   */
  NS_IMETHOD
  GetDocument(nsIDocument* *aDocument) = 0;

  /**
   * Invalidate the rectangle
   */
  NS_IMETHOD
  InvalidateRect(nsPluginRect *invalidRect) = 0;

  /**
   * Invalidate the region
   */
  NS_IMETHOD
  InvalidateRegion(nsPluginRegion invalidRegion) = 0;

  /**
   * Force a redraw
   */
  NS_IMETHOD
  ForceRedraw() = 0;

  /**
   * Get the specified variable
   */
  NS_IMETHOD
  GetValue(nsPluginInstancePeerVariable variable, void *value) = 0;
};

#endif
