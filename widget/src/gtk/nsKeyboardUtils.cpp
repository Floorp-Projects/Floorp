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

#include <X11/Xlib.h>
#ifdef HAVE_X11_XKBLIB_H
#   if defined (__digital__) && defined (__unix__)
#   define explicit Explicit
#   include <X11/XKBlib.h>
#   undef explicit
#else
#   include <X11/XKBlib.h>
#endif
#endif
#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <string.h>
#include "nsKeyboardUtils.h"
#include "nspr.h"
#include "nsWindow.h"

//
// xkbms: X KeyBoard Mode Switch
//
// Mode_switch during XGrabKeyboard workaround
//
// During a keyboard grab some (all?) X servers do not
// set the Mode_switch bit in the XKeyEvent state value
// even if the user is pressing the Mode_switch key.
// Thus some important keysym mappings are not available
// during a keyboard grab; eg: German keyboards use 
// Mode_switch (AltGr) and Q for '@' which is an 
// important key for entering email addresses.
// Finnish keyboards use dead_tilde for '~' which is an
// important key for entering user's home page URLs
//
// This workaround turns off the keyboard grab when
// the Mode_switch down.
//
// Note:
// Mode_switch MappingNotify should be called when the
// keyboard mapping changes (a MappingNotify event) but
// the XMappingNotify event has a null window and gdk only allows an
// application to filter events with a non-null window
//

#undef DEBUG_X_KEYBOARD_MODE_SWITCH
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
#define DEBUG_PRINTF(x) \
            PR_BEGIN_MACRO \
              printf x ; \
              printf(", %s %d\n", __FILE__, __LINE__); \
            PR_END_MACRO 
#else
#define DEBUG_PRINTF(x) \
            PR_BEGIN_MACRO \
            PR_END_MACRO 
#endif


// Xlib should define this!
#define MODIFIERMAP_ROW_SIZE 8

PRUint32 nsXKBModeSwitch::gModeSwitchKeycode1 = 0;
PRUint32 nsXKBModeSwitch::gModeSwitchKeycode2 = 0;
PRBool   nsXKBModeSwitch::gGrabDuringPopup = PR_TRUE;
PRBool   nsXKBModeSwitch::gUnGrabDuringModeSwitch = PR_TRUE;
PRBool   nsXKBModeSwitch::gModeSwitchDown = PR_FALSE;
gint     nsXKBModeSwitch::gOwnerEvents;
guint32  nsXKBModeSwitch::gGrabTime;


void 
nsXKBModeSwitch::ControlWorkaround(gboolean grab_during_popup,
                                   gboolean ungrab_during_mode_switch)
{
  DEBUG_PRINTF(("nsXKBModeSwitch::ControlWorkaround:"));
  DEBUG_PRINTF(("    grab_during_popup = %d",
                     grab_during_popup));
  DEBUG_PRINTF(("    ungrab_during_mode_switch = %d",
                     ungrab_during_mode_switch));
  gGrabDuringPopup = grab_during_popup;
  gUnGrabDuringModeSwitch = ungrab_during_mode_switch;

  //
  // This really should be called whenever a MappingNotify
  // event happens but gdk does not support that.
  //
  HandleMappingNotify();
}

gint
nsXKBModeSwitch::GrabKeyboard(GdkWindow *win, gint owner_events, guint32 time)
{
  // if grab is disabled pretend it succeeded
  if (!gGrabDuringPopup) {
    return GrabSuccess;
  }

  gint retval;
  retval = gdk_keyboard_grab(win, owner_events, time);
  if (retval == GrabSuccess) {
    gOwnerEvents = owner_events;
    gGrabTime = time;
  }
  else {
    gOwnerEvents = 0;
    gGrabTime = 0;
  }
 
  return retval;
}

void
nsXKBModeSwitch::HandleMappingNotify()
{
  XModifierKeymap *xmodmap;
  int i, j, max_keypermod;

  // since the mapping could change we (re)initialize variables
  Init();

  xmodmap = XGetModifierMapping(GDK_DISPLAY());
  if (!xmodmap)
    return;

  max_keypermod = MIN(xmodmap->max_keypermod,5);
  for (i=0; i<max_keypermod; i++) {
    for (j=0; j<MODIFIERMAP_ROW_SIZE; j++) {
      KeyCode keycode;
      KeySym keysym;
      char *keysymName;
      keycode = *((xmodmap->modifiermap)+(i*MODIFIERMAP_ROW_SIZE)+j);
      if (!keycode)
        continue;
      keysym = XKeycodeToKeysym(GDK_DISPLAY(), keycode, 0);
      if (!keysym)
        continue;
      keysymName = XKeysymToString(keysym);
      if (!keysymName)
        continue;
      if (!strcmp(keysymName,"Mode_switch")) {
        if (!gModeSwitchKeycode1)
          gModeSwitchKeycode1 = keycode;
        else if (!gModeSwitchKeycode2)
          gModeSwitchKeycode2 = keycode;
      }
    }
  }
  XFreeModifiermap(xmodmap);

  if (!gModeSwitchKeycode1) {
    DEBUG_PRINTF(("\n\nnsXKBModeSwitch::HandleMappingNotify: no Mode_switch\n\n"));
  }
  DEBUG_PRINTF(("\n\nnsXKBModeSwitch::HandleMappingNotify:"));
  DEBUG_PRINTF(("    gModeSwitchKeycode1 = %d", gModeSwitchKeycode1));
  DEBUG_PRINTF(("    gModeSwitchKeycode2 = %d", gModeSwitchKeycode2));

#if defined(HAVE_X11_XKBLIB_H) && \
  defined(XkbMajorVersion) && defined(XbMinorVersion)
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor)) {
      xkb_major = XkbMajorVersion;
      xkb_minor = XkbMinorVersion;
      if (XkbQueryExtension (gdk_display, NULL, NULL, NULL,
                               &xkb_major, &xkb_minor)) {
      }
    }
  }
#endif

}

void
nsXKBModeSwitch::Init()
{
    gModeSwitchKeycode1 = 0;
    gModeSwitchKeycode2 = 0;
    gModeSwitchDown = FALSE;
}

void
nsXKBModeSwitch::HandleKeyPress(XKeyEvent *xke)
{
  // if grab is completely disabled then there is nothing to do
  if (!gGrabDuringPopup) {
    return;
  }

  // check for a Mode_switch keypress
  if ((xke->keycode == gModeSwitchKeycode1) 
      || (xke->keycode == gModeSwitchKeycode2)) {
    gModeSwitchDown = TRUE;
    DEBUG_PRINTF(("nsXKBModeSwitch::HandleKeyPress: Mode_switch is down"));
    nsWindow *win = nsWindow::GetGrabWindow();
    if (!win)
      return;
    if (win->GrabInProgress()) {
      if (gUnGrabDuringModeSwitch) {
        gdk_keyboard_ungrab(GDK_CURRENT_TIME);
        DEBUG_PRINTF(("\n\n*** ungrab keyboard ***\n\n"));
      }
    }
  }
}

void
nsXKBModeSwitch::HandleKeyRelease(XKeyEvent *xke)
{
  // if grab is completely disabled then there is nothing to do
  if (!gGrabDuringPopup) {
    return;
  }

  // check for a Mode_switch keyrelease
  if ((xke->keycode == gModeSwitchKeycode1) 
          || (xke->keycode == gModeSwitchKeycode2)) {
    gModeSwitchDown = FALSE;
    DEBUG_PRINTF(("nsXKBModeSwitch::HandleKeyPress: Mode_switch is up"));
    nsWindow *win = nsWindow::GetGrabWindow();
    if (!win)
      return;
    if (win->GrabInProgress()) {
      if (gUnGrabDuringModeSwitch) {
        if (!win->GetGdkGrabWindow())
          return;
        gdk_keyboard_grab(win->GetGdkGrabWindow(), gOwnerEvents, gGrabTime);
        DEBUG_PRINTF(("\n\n*** re-grab keyboard ***\n\n"));
      }
    }
  }
}

void
nsXKBModeSwitch::UnGrabKeyboard(guint32 time)
{
  // if grab is completely disabled then there is nothing to do
  if (!gGrabDuringPopup) {
    return;
  }

  gdk_keyboard_ungrab(time);
  gOwnerEvents = 0;
  gGrabTime = 0;
}

