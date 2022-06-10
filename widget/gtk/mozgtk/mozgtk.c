/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

#include <gdk/gdk.h>

// Dummy call to gtk3 library to prevent the linker from removing
// the gtk3 dependency with --as-needed.
// see toolkit/library/moz.build for details.
MOZ_EXPORT void mozgtk_linker_holder() { gdk_display_get_default(); }

#ifdef MOZ_X11
#  include <X11/Xlib.h>
// Bug 1271100
// We need to trick system Cairo into not using the XShm extension due to
// a race condition in it that results in frequent BadAccess errors. Cairo
// relies upon XShmQueryExtension to initially detect if XShm is available.
// So we define our own stub that always indicates XShm not being present.
// mozgtk loads before libXext/libcairo and so this stub will take priority.
// Our tree usage goes through xcb and remains unaffected by this.
//
// This is also used to force libxul to depend on the mozgtk library. If we
// ever can remove this workaround for system Cairo, we'll need something
// to replace it for that purpose.
MOZ_EXPORT Bool XShmQueryExtension(Display* aDisplay) { return False; }
#endif
