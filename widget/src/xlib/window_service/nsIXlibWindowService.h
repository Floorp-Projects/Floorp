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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are
 * Copyright (C) 1999 Christopher Blizzard. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIXlibWindowService_h__
#define nsIXlibWindowService_h__

#include "nsISupports.h"

#include <sys/time.h>
#include <X11/Xlib.h>

// Interface id for the XlibWindow service
// { bd39ccb0-3f08-11d3-b419-00805f6d4c2a }
#define NS_XLIB_WINDOW_SERVICE_IID        \
              { 0xbd39ccb0,               \
              0x3f08,                     \
              0x11d3,                     \
              { 0xb4, 0x19, 0x00, 0x80, 0x5f, 0x6d, 0x4c, 0x2a } }

// Class ID for our implementation
// { 285fb550-3af4-11d3-aeb9-0000f8e25c06 }
#define NS_XLIB_WINDOW_SERVICE_CID \
  { 0x285fb550,                    \
    0x3af4,                        \
    0x11d3,                        \
    { 0xae, 0xb9, 0x00, 0x00, 0xf8, 0xe2, 0x5c, 0x06 } }

#define NS_XLIB_WINDOW_SERVICE_CONTRACTID "@mozilla.org/widget/xlib/window_service;1"

typedef void (*nsXlibWindowCallback)(PRUint32 aWindowID);

typedef void * nsXlibNativeEvent;

typedef void (*nsXlibEventDispatcher)(nsXlibNativeEvent aNativeEvent);

typedef int (*nsXlibTimeToNextTimeoutFunc)(struct timeval *aTimer);

typedef void (*nsXlibProcessTimeoutsProc)(Display *aDisplay);

class nsIXlibWindowService : public nsISupports
{
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_XLIB_WINDOW_SERVICE_IID);

  NS_IMETHOD SetWindowCreateCallback(nsXlibWindowCallback aCallback) = 0;
  NS_IMETHOD SetWindowDestroyCallback(nsXlibWindowCallback aCallback) = 0;
  NS_IMETHOD GetWindowCreateCallback(nsXlibWindowCallback * aCallbackOut) = 0;
  NS_IMETHOD GetWindowDestroyCallback(nsXlibWindowCallback * aCallbackOut) = 0;

  NS_IMETHOD SetEventDispatcher(nsXlibEventDispatcher aDispatcher) = 0;
  NS_IMETHOD GetEventDispatcher(nsXlibEventDispatcher * aDispatcherOut) = 0;

  NS_IMETHOD SetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc aFunc) = 0;
  NS_IMETHOD GetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc * aFuncOut) = 0;

  NS_IMETHOD SetProcessTimeoutsProc(nsXlibProcessTimeoutsProc aProc) = 0;
  NS_IMETHOD GetProcessTimeoutsProc(nsXlibProcessTimeoutsProc * aProcOut) = 0;
  
};

#endif /* nsIXlibWindowService_h__ */
