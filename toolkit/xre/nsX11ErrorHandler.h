/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_X11
#  include <X11/Xlib.h>
#  include "X11UndefineNone.h"  // Unset some macros defined by X.h included by Xlib.h

/**
 * InstallX11ErrorHandler is not suitable for processes running with GTK3 as
 * GDK3 will replace the handler.  This is still used for the plugin process,
 * which runs with GTK2.
 **/
void InstallX11ErrorHandler();

void CleanupX11ErrorHandler();

extern "C" int X11Error(Display* display, XErrorEvent* event);
#endif
