/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

// Only define the following workaround when using GTK3, which we detect
// by checking if GTK3 stubs are not provided.
#include <X11/Xlib.h>
// Bug 1271100
// We need to trick system Cairo into not using the XShm extension due to
// a race condition in it that results in frequent BadAccess errors. Cairo
// relies upon XShmQueryExtension to initially detect if XShm is available.
// So we define our own stub that always indicates XShm not being present.
// mozgtk loads before libXext/libcairo and so this stub will take priority.
// Our tree usage goes through xcb and remains unaffected by this.
MFBT_API Bool XShmQueryExtension(Display* aDisplay) { return False; }
