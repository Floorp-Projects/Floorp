/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Initial Developer of this code under the MPL is Brian Stell
 * <bstell@netscape.com>.
 * Portions created by the Initial Developers are Copyright (C) 2001
 * Netscape.  All Rights Reserved. 
 */

#ifndef _NSKEYBOARDUTILS_H_
#define _NSKEYBOARDUTILS_H_

extern "C" {
#include <X11/Xlib.h>
#include <gdk/gdk.h>
}
#include "prtypes.h"

class nsXKBModeSwitch {
  public:
    static void ControlWorkaround(gboolean grab_during_popup,
                                  gboolean ungrab_during_mode_switch);
    static gint GrabKeyboard(GdkWindow *win, gint owner_events, guint32 time);
    static void UnGrabKeyboard(guint32 time);
    static void HandleKeyPress(XKeyEvent *xke);
    static void HandleKeyRelease(XKeyEvent *xke);
    static void HandleMappingNotify();

  private:
    static void Init();

    static PRUint32 gModeSwitchKeycode1;
    static PRUint32 gModeSwitchKeycode2;
    static PRBool   gGrabDuringPopup;
    static PRBool   gUnGrabDuringModeSwitch;
    static PRBool   gModeSwitchDown;
    static gint     gOwnerEvents;
    static guint32  gGrabTime;

};

#endif /* _NSKEYBOARDUTILS_H_ */
