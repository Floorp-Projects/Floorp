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

#include <X11/Xlibint.h>
#ifdef HAVE_X11_XKBLIB_H
#include <X11/XKBlib.h>
#endif
#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include "nsKeyboardUtils.h"
#include "nspr.h"

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
//
// This workaround simulates the Mode_switch bit during a
// keyboard grab. The Mode_switch bit really belongs to
// the X Server issue so only do this workaround when 
// necessary.
//
// Note:
// Mode_switch MappingNotify should be called when the
// keyboard mapping changes (a MappingNotify event) but
// this event has a null window and gdk only allows an
// application to filter events with a non-null window
//

#undef DEBUG_X_KEYBOARD_MODE_SWITCH

// Xlib should define this!
#define MODIFIERMAP_ROW_SIZE 8

PRBool   nsXKBModeSwitch::gInitedGlobals = PR_FALSE;
PRUint32 nsXKBModeSwitch::gModeSwitchKeycode1 = 0;
PRUint32 nsXKBModeSwitch::gModeSwitchKeycode2 = 0;
PRUint32 nsXKBModeSwitch::gModeSwitchBit = 0;
PRBool   nsXKBModeSwitch::gWorkaroundEnabled = PR_FALSE;
PRBool   nsXKBModeSwitch::gAreGrabbingKeyboard = PR_FALSE;
PRBool   nsXKBModeSwitch::gModeSwitchDown = PR_FALSE;
PRBool   nsXKBModeSwitch::gEnableXkbKeysymToModifiers = PR_FALSE;
PRUint32 nsXKBModeSwitch::gUserDefinedModeSwitchBit = 0;


void
nsXKBModeSwitch::EnableWorkaround(gboolean enable)
{
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
    printf("nsXKBModeSwitch::EnableWorkaround: enable = %d, %s %d\n", 
                               enable, __FILE__, __LINE__);
#endif
  gWorkaroundEnabled = enable;

  //
  // This really should be called whenever a MappingNotify
  // event happens but gdk does not support that.
  //
  HandleMappingNotify();
}

void
nsXKBModeSwitch::AreGrabbingKeyboard(gboolean grabbing)
{
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
    printf("nsXKBModeSwitch::AreGrabbingKeyboard: grabbing = %d, %s %d\n", 
                               grabbing, __FILE__, __LINE__);
#endif
  gAreGrabbingKeyboard = grabbing;
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
        if (gEnableXkbKeysymToModifiers) {
#ifdef HAVE_X11_XKBLIB_H
          gModeSwitchBit |= XkbKeysymToModifiers(GDK_DISPLAY(), keysym);
#endif
        }
        else {
          gModeSwitchBit |= gUserDefinedModeSwitchBit;
        }
        if (!gModeSwitchKeycode1)
          gModeSwitchKeycode1 = keycode;
        else if (!gModeSwitchKeycode2)
          gModeSwitchKeycode2 = keycode;
      }
    }
  }
  XFreeModifiermap(xmodmap);

  if (!gModeSwitchKeycode1) {
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
  printf("\n\n");
  printf("nsXKBModeSwitch::HandleMappingNotify: no Mode_switch, %s %d\n", 
                              __FILE__, __LINE__);
  printf("\n\n");
#endif
    return;
  }

#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
  printf("\n\n");
  printf("nsXKBModeSwitch::HandleMappingNotify:, %s %d\n", __FILE__, __LINE__);
  printf("    gModeSwitchKeycode1 = %d\n", gModeSwitchKeycode1);
  printf("    gModeSwitchKeycode2 = %d\n", gModeSwitchKeycode2);
#endif

#ifdef HAVE_X11_XKBLIB_H
  {
    gint xkb_major = XkbMajorVersion;
    gint xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion (&xkb_major, &xkb_minor)) {
      xkb_major = XkbMajorVersion;
      xkb_minor = XkbMinorVersion;
      if (XkbQueryExtension (gdk_display, NULL, NULL, NULL,
                               &xkb_major, &xkb_minor)) {
        gint group = 1;
        gModeSwitchBit = XkbBuildCoreState(0, group);
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
        printf("    gModeSwitchBit(XKB) = 0x%x\n", gModeSwitchBit);
        printf("\n\n");
#endif
      }
    }
  }
#else
  printf("         gModeSwitchBit = 0x%x\n", gModeSwitchBit);
  printf("\n\n");
#endif
}

void
nsXKBModeSwitch::Init()
{
    gModeSwitchKeycode1 = 0;
    gModeSwitchKeycode2 = 0;
    gModeSwitchBit = 0;
    gAreGrabbingKeyboard = FALSE;
    gModeSwitchDown = FALSE;

  if (!gInitedGlobals) {
    gInitedGlobals = PR_TRUE;

    // allow the user to specify the Mode_Switch bit
    char *env_str;
    env_str = getenv("MOZ_XKB_MODE_SWITCH_BIT");
    if (env_str) 
      PR_sscanf(env_str, "%lX", &gUserDefinedModeSwitchBit);

    // Risk management: the value from XkbKeysymToModifier
    // has not been tested. For those systems without
    // the X KeyBoard Extension: if XkbKeysymToModifier
    // works allow users to enable its use
    // Please resolve if this works soon.
    env_str = getenv("MOZ_XKB_KEYSYM_TO_MODIFIERS");
    if (env_str) 
      gEnableXkbKeysymToModifiers = PR_TRUE;
  }
}

void
nsXKBModeSwitch::HandleKeyPress(XKeyEvent *xke)
{
  // if we do not have a Mode_switch keycode mapping
  // there is nothing to do
  if (!gModeSwitchKeycode1) {
    gModeSwitchDown = FALSE;
    return;
  }

  // check for a Mode_switch keypress
  if ((xke->keycode == gModeSwitchKeycode1) 
      || (xke->keycode == gModeSwitchKeycode2)) {
    gModeSwitchDown = TRUE;
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
    printf("nsXKBModeSwitch::HandleKeyPress: Mode_switch is down, %s %d\n", 
                               __FILE__, __LINE__);
#endif
    return;
  }

  // if all the conditions are met we simulate the
  // Mode_switch bit
  if (gModeSwitchDown 
      && gAreGrabbingKeyboard
      && gWorkaroundEnabled
      ) {
    xke->state |= gModeSwitchBit;
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
    printf("nsXKBModeSwitch::HandleKeyPress: set Mode_switch bit (0x%x), %s %d\n", 
                               gModeSwitchBit, __FILE__, __LINE__);
#endif
  }
}

void
nsXKBModeSwitch::HandleKeyRelease(XKeyEvent *xke)
{
  if (!gModeSwitchKeycode1
      || (xke->keycode == gModeSwitchKeycode1)
      || (xke->keycode == gModeSwitchKeycode2)
      ) {
    gModeSwitchDown = FALSE;
#ifdef DEBUG_X_KEYBOARD_MODE_SWITCH
    printf("nsXKBModeSwitch::HandleKeyRelease: Mode_switch is up, %s %d\n", 
                               __FILE__, __LINE__);
#endif
  }
}

