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

#ifndef __INSTANCEBASE_H__
#define __INSTANCEBASE_H__

#include "nsplugin.h"

/*******************************************************************
*
* CPluginInstance class is the class for all our plugins our plugin
* instances. This is abstract class, platform specific method are
* to be implemented in derived class for a specific platform.
*
********************************************************************/

class CPluginInstance : public nsIPluginInstance 
{
public:

NS_DECL_ISUPPORTS

  // (Corresponds to NPP_HandleEvent.)
  // Note that for Unix and Mac the nsPluginEvent structure is different
  // from the old NPEvent structure -- it's no longer the native event
  // record, but is instead a struct. This was done for future extensibility,
  // and so that the Mac could receive the window argument too. For Windows
  // and OS2, it's always been a struct, so there's no change for them.
  NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);

  NS_IMETHOD Initialize(nsIPluginInstancePeer* peer);
  NS_IMETHOD GetPeer(nsIPluginInstancePeer* *result);

  // This method is called when the plugin instance is to be started. 
  // This happens in two circumstances: (1) after the plugin instance
  // is first initialized, and (2) after a plugin instance is returned to
  // (e.g. by going back in the window history) after previously being stopped
  // by the Stop method. 
  NS_IMETHOD Start();

  // This method is called when the plugin instance is to be stopped (e.g. by 
  // displaying another plugin manager window, causing the page containing 
  // the plugin to become removed from the display).
  NS_IMETHOD Stop();

  // This is called once, before the plugin instance peer is to be 
  // destroyed. This method is used to destroy the plugin instance. 
  NS_IMETHOD Destroy();

  // (Corresponds to NPP_SetWindow.)
  NS_IMETHOD SetWindow(nsPluginWindow* window);

  NS_IMETHOD NewStream(nsIPluginStreamListener** result);
  NS_IMETHOD Print(nsPluginPrint* platformPrint);
  NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);

  CPluginInstance();
  virtual ~CPluginInstance();

  // Platrorm specific methods, which need to be implemented in the derived class
  virtual nsresult PlatformNew() = 0;
  virtual nsresult PlatformDestroy() = 0;
  virtual nsresult PlatformSetWindow(nsPluginWindow* window) = 0;
  virtual PRInt16 PlatformHandleEvent(nsPluginEvent* event) = 0;

protected:
  nsIPluginInstancePeer* fPeer;
  nsPluginWindow* fWindow;
  nsPluginMode fMode;
};

CPluginInstance * Platform_CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, const char* aPluginMIMEType);

#endif
