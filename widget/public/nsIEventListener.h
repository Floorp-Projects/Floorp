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
 */

#ifndef nsIEventListener_h__
#define nsIEventListener_h__

#include "nsISupports.h"

struct nsGUIEvent;

/**
 * Event listener interface.
 * Alternative to a callback for recieving events.
 */

// {c83f6b80-d7ce-11d2-8360-c4c894c4917c}
#define NS_IEVENTLISTENER_IID \
{ 0xc83f6b80, 0xd7ce, 0x11d2, { 0x83, 0x60, 0xc4, 0xc8, 0x94, 0xc4, 0x91, 0x7c } }

class nsIEventListener : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTLISTENER_IID)
  
 /**
  * Processes all events. 
  * If a mouse listener is registered this method will not process mouse events. 
  * @param anEvent the event to process. See nsGUIEvent.h for event types.
  */

  virtual nsEventStatus ProcessEvent(const nsGUIEvent & anEvent) = 0;

};

#endif // nsIEventListener_h__
