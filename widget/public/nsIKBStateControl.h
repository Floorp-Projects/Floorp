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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Frank Tang <ftang@netscape.com>
 */

#ifndef nsIKBStateControl_h__
#define nsIKBStateControl_h__

#include "nsISupports.h"

// {6A9AC731-988A-11d3-86CC-005004832142}
#define NS_IKBSTATECONTROL_IID \
{ 0x6a9ac731, 0x988a, 0x11d3, \
{ 0x86, 0xcc, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }


/**
 * interface to control keyboard input state
 */
class nsIKBStateControl : public nsISupports {

  public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IKBSTATECONTROL_IID)

    /*
     * Force Input Method Editor to commit the uncommited input
     */
    NS_IMETHOD ResetInputState()=0;

    /*
     * This method is called in the init stage of a password field
     */

};

#endif // nsIKBStateControl_h__
