/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GDKVISUAL_WRAPPER_H
#define GDKVISUAL_WRAPPER_H

#define gdk_visual_get_depth gdk_visual_get_depth_
#include_next <gdk/gdkvisual.h>
#undef gdk_visual_get_depth

static inline gint
gdk_visual_get_depth(GdkVisual *visual)
{
  return visual->depth;
}
#endif /* GDKVISUAL_WRAPPER_H */
