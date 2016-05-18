/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef WidgetStyleCache_h
#define WidgetStyleCache_h

#include <gtk/gtk.h>
#include "gtkdrawing.h"


typedef unsigned StyleFlags;
enum : StyleFlags {
  NO_STYLE_FLAGS,
  WHATEVER_MIGHT_BE_NEEDED = 1U << 0,
};

GtkWidget*
GetWidget(WidgetNodeType aNodeType);

// Callers must call ReleaseStyleContext() on the returned context.
GtkStyleContext*
ClaimStyleContext(WidgetNodeType aNodeType,
                  GtkTextDirection aDirection = GTK_TEXT_DIR_LTR,
                  GtkStateFlags aStateFlags = GTK_STATE_FLAG_NORMAL,
                  StyleFlags aFlags = NO_STYLE_FLAGS);
void
ReleaseStyleContext(GtkStyleContext* style);

void
ResetWidgetCache(void);

#endif // WidgetStyleCache_h
