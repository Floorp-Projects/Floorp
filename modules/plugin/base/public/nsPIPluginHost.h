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

#ifndef nsPIPluginHost_h___
#define nsPIPluginHost_h___

#include "nsIPluginInstance.h"

#define NS_PIPLUGINHOST_IID                        \
{/* 8e3d71e6-2319-11d5-9cf8-0060b0fbd8ac */        \
  0x8e3d71e6, 0x2319, 0x11d5,                      \
  {0x9c, 0xf8, 0x00, 0x60, 0xb0, 0xfb, 0xd8, 0xac} \
}

class nsPIPluginHost : public nsISupports
{
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_PIPLUGINHOST_IID)

 /*
  * To notify the plugin manager that the plugin created a script object 
  */
  NS_IMETHOD
  SetIsScriptableInstance(nsCOMPtr<nsIPluginInstance> aPluginInstance, PRBool aScriptable) = 0;
};

#endif /* nsPIPluginHost_h___ */
