/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Bill Law       law@netscape.com
 */
#ifndef nsISplashScreen_h__
#define nsISplashScreen_h__

#include "nsISupports.h"

// {B5030250-D530-11d3-8070-00600811A9C3}
#define NS_ISPLASHSCREEN_IID \
 { 0xb5030250, 0xd530, 0x11d3, { 0x80, 0x70, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

/*
 * This class specifies the interface to be used by xp code to manage the
   platform-specific splash screen.  Each platform (that has a splash screen)
   should implement this interface and return a instance of that class from
   the function NS_CreateSplashScreen.

   Note that the nature of the splash screen object is such that it is
   impossible to obtain an instance of this interface via standard XPCOM
   means (componenter manager/service manager).

   For the same reason, implementors should take care to implement this
   interface without introducing any dependencies on other components.
   This includes XPCOM itself as it will not have been initialized when
   an object that implements this interface will be created.  Implementors
   should implement the nsISupports member functions "manually" (rather
   than via the standard NS_*IMPL macros) to avoid any dependencies
   lurking in those macros.
 */
class nsISplashScreen : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_ISPLASHSCREEN_IID )
    NS_IMETHOD Show() = 0;
    NS_IMETHOD Hide() = 0;
}; // class nsISplashScreen

#endif // nsISplashScreen_h__
