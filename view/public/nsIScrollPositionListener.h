/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Patrick Beard
 */

#ifndef nsIScrollPositionListener_h___
#define nsIScrollPositionListener_h___

#include "nsISupports.h"
#include "nsCoord.h"

// forward declarations
class nsIScrollableView;

// IID for the nsIScrollPositionListener interface
// {f8dfc500-6ad1-11d3-8360-a3f373ff79fc}
#define NS_ISCROLLPOSITIONLISTENER_IID \
{ 0xf8dfc500, 0x6ad1, 0x11d3, { 0x83, 0x60, 0xa3, 0xf3, 0x73, 0xff, 0x79, 0xfc } }

/**
 * Provides a way for a client of an nsIScrollableView to learn about scroll position
 * changes.
 */
class nsIScrollPositionListener : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCROLLPOSITIONLISTENER_IID)

	NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY) = 0;
	NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY) = 0;
};

#endif /* nsIScrollPositionListener_h___ */

