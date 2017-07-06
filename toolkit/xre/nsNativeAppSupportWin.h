/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file has *public* stuff needed for the Win32 implementation of
 * the nsINativeAppSupport interface.  It has to be broken out into a
 * separate file in order to ensure that the generated .h file can be
 * used in a Win32 .rc file.  See /mozilla/xpfe/bootstrap/splash.rc.
 *
 * This file, and the generated .h, are only needed on Win32 platforms.
 */

// Constants identifying Win32 "native" resources.

#ifdef MOZ_PHOENIX

// Splash screen dialog ID.
#define IDD_SPLASH  100

// Splash screen bitmap ID.
#define IDB_SPLASH  101

// DDE application name
#define ID_DDE_APPLICATION_NAME 102

#define IDI_APPICON 1
#define IDI_DOCUMENT 2
#define IDI_NEWWINDOW 3
#define IDI_NEWTAB 4
#define IDI_PBMODE 5
#ifndef IDI_APPLICATION
#define IDI_APPLICATION 32512
#endif

#endif

// String that goes in the WinXP Start Menu.
#define IDS_STARTMENU_APPNAME 103
