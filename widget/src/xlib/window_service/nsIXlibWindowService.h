/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) 1999
 * Christopher Blizzard.  All Rights Reserved.
 */

#ifndef nsIXlibWindowService_h__
#define nsIXlibWindowService_h__

#include "nsISupports.h"

#include <sys/time.h>

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

#define NS_XLIB_WINDOW_SERVICE_PROGID "component://netscape/widget/xlib/window_service"

// /**
//  * This is an interface for getting access to the construction
//  * and destruction of native windows in xlib.
//  * @created 20/Jul/1999
//  * @author  Christopher Blizzard <blizzard@redhat.com>
//  */

// class nsXlibWindowCallback {
//  public:
//   virtual void WindowCreated  (PRUint32 aID) = 0;
//   virtual void WindowDestroyed(PRUint32 aID) = 0;
// };

typedef void (*nsXlibWindowCallback)(PRUint32 aWindowID);

typedef void * nsXlibNativeEvent;

typedef void (*nsXlibEventDispatcher)(nsXlibNativeEvent aNativeEvent);

typedef int (*nsXlibTimeToNextTimeoutFunc)(struct timeval *aTimer);

typedef void (*nsXlibProcessTimeoutsProc)(void);

class nsIXlibWindowService : public nsISupports
{
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_XLIB_WINDOW_SERVICE_IID);

//   /**
//    * Register your function for window creation and destruction.
//    * This function will get called whenever a new native X window
//    * is created. or destroyed.
//    *
//    * @param  [IN] the function that you would like to register
//    * @return NS_OK if ok, NS_ERROR_FAILURE if you pass in 0
//    */

//   NS_IMETHOD SetCreateCallback(nsXlibWindowCallback *aFunc) = 0;

//   /** 
//    * This function will dispatch a native X event ( cast to a void *
//    * here ) to the event handler on the inside of the widget
//    * toolkit
//    * @param [IN] a pointer to an XEvent, cast to a void *
//    * @return NS_OK if ok, NS_ERROR_FAILURE if you pass in an
//    * invalid window id
//    */

//   NS_IMETHOD DispatchNativeXlibEvent(void *aNativeEvent) = 0;

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
