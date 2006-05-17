/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Dispatcher for a variety of application-scope events.  The real event loop
 * is in nsAppShell.
 */

#ifndef nsMacMessagePump_h__
#define nsMacMessagePump_h__

#include "prtypes.h"

#include <Events.h>

class nsToolkit;
class nsMacTSMMessagePump;

class nsMacMessagePump
{
  public:
    nsMacMessagePump(nsToolkit *aToolKit);
    virtual ~nsMacMessagePump();

    PRBool ProcessEvents(PRBool aProcessEvents);
  
    // returns PR_TRUE if the event was handled
    PRBool DispatchEvent(EventRecord *anEvent);

  protected:
    // these all return PR_TRUE if the event was handled
    PRBool DoMouseDown(EventRecord &anEvent);
    PRBool DoMouseUp(EventRecord &anEvent);
    PRBool DoMouseMove(EventRecord &anEvent);
    PRBool DoUpdate(EventRecord &anEvent);
    PRBool DoKey(EventRecord &anEvent);
    PRBool DoActivate(EventRecord &anEvent);

    PRBool DispatchOSEventToRaptor(EventRecord &anEvent, WindowPtr aWindow);

    static pascal OSStatus MouseClickEventHandler(
                                           EventHandlerCallRef aHandlerCallRef,
                                           EventRef            aEvent,
                                           void*               aUserData);
    static pascal OSStatus WNETransitionEventHandler(
                                           EventHandlerCallRef aHandlerCallRef,
                                           EventRef            aEvent,
                                           void*               aUserData);

  protected:
    nsToolkit*           mToolkit;
    nsMacTSMMessagePump* mTSMMessagePump;
    EventHandlerRef      mMouseClickEventHandler;
    EventHandlerRef      mWNETransitionEventHandler;
};
#endif // nsMacMessagePump_h__
