/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIEventListener_h__
#define nsIEventListener_h__

#include "nsGUIEvent.h"

/*
 * Event listener interface.
 * Alternative to a callback for recieving events.
 */

class nsIEventListener {

public:

 /**
  * Processes all events. 
  * If a mouse listener is registered this method will not process mouse events. 
  * @param anEvent the event to process. @see nsGUIEvent.h for event types.
  */

  virtual nsEventStatus ProcessEvent(const nsGUIEvent & anEvent) = 0;

};

#endif // nsIEventListener_h__
