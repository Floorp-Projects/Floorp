/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIConnectionGroup_h___
#define nsIConnectionGroup_h___

#include "nsICancelable.h"
#include "nsICollection.h"

class nsIProtocolConnection;

// XXX regenerate:
#define NS_ICONNECTIONGROUP_IID                      \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x6f, 0xc8, 0x11, 0x9d, 0x7a} \
}

class nsIConnectionGroup : public nsICancelable, public nsICollection
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONNECTIONGROUP_IID);

    /**
     * Add a child to an nsIConnectionGroup.
     *
     * @param aGroup    The nsIConnectionGroup to be added as a child.  When the
     *                  child is added it will receive the nsILoadAttribs
     *                  of its parent...
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD AddChildGroup(nsIConnectionGroup* aGroup) = 0;

    /**
     * Remove a child from an nsIConnectionGroup.
     *
     * @param aGroup    The nsIConnectionGroup to be removed.
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD RemoveChildGroup(nsIConnectionGroup* aGroup) = 0;

};

#endif /* nsIConnectionGroup_h___ */
