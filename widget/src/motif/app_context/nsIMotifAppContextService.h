/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIMotifAppContextService_h__
#define nsIMotifAppContextService_h__

#include "nsISupports.h"
#include <X11/Intrinsic.h>

// Interface id for the MotifAppContext service
// { 90F55E51-40EB-11d3-B219-000064657374 }
#define NS_MOTIF_APP_CONTEXT_SERVICE_IID                \
 { 0x90f55e51, 0x40eb, 0x11d3,                          \
  { 0xb2, 0x19, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

// Class ID for our implementation
// { 90F55E52-40EB-11d3-B219-000064657374 }
#define NS_MOTIF_APP_CONTEXT_SERVICE_CID                \
 { 0x90f55e52, 0x40eb, 0x11d3,                          \
  { 0xb2, 0x19, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define NS_MOTIF_APP_CONTEXT_SERVICE_CONTRACTID "@mozilla.org/widget/motif/app_context;1"

class nsIMotifAppContextService : public nsISupports
{
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_MOTIF_APP_CONTEXT_SERVICE_IID);

  /**
   * Register your function for window creation and destruction.
   * This function will get called whenever a new native X window
   * is created. or destroyed.
   *
   * @param  [IN] the function that you would like to register
   * @return NS_OK if ok, NS_ERROR_FAILURE if you pass in 0
   */

  NS_IMETHOD SetAppContext(XtAppContext aAppContext) = 0;

  /** 
   * This function will dispatch a native X event ( cast to a void *
   * here ) to the event handler on the inside of the widget
   * toolkit
   * @param [IN] a pointer to an XEvent, cast to a void *
   * @return NS_OK if ok, NS_ERROR_FAILURE if you pass in an
   * invalid window id
   */

  NS_IMETHOD GetAppContext(XtAppContext * aAppContextOut) = 0;
  
};

#endif /* nsIMotifAppContextService_h__ */
