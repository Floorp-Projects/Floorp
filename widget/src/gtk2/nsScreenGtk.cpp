/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <caillon@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsScreenGtk.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xatom.h>

static GdkFilterReturn
root_window_event_filter(GdkXEvent *aGdkXEvent, GdkEvent *aGdkEvent,
                         gpointer aClosure)
{
  XEvent *xevent = static_cast<XEvent*>(aGdkXEvent);
  nsScreenGtk *ourScreen = static_cast<nsScreenGtk*>(aClosure);

  // See comments in nsScreenGtk::Init below.
  switch (xevent->type) {
    case ConfigureNotify:
      ourScreen->ReInit();
      break;
    case PropertyNotify:
      {
        XPropertyEvent *propertyEvent = &xevent->xproperty;
        if (propertyEvent->atom == ourScreen->NetWorkareaAtom()) {
          ourScreen->ReInit();
        }
      }
      break;
    default:
      break;
  }

  return GDK_FILTER_CONTINUE;
}

nsScreenGtk :: nsScreenGtk (  )
  : mRootWindow(nsnull),
    mScreenNum(0),
    mRect(0, 0, 0, 0),
    mAvailRect(0, 0, 0, 0)
{
}


nsScreenGtk :: ~nsScreenGtk()
{
  if (mRootWindow) {
    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nsnull;
  }
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenGtk, nsIScreen)


NS_IMETHODIMP
nsScreenGtk :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = mRect.x;
  *outTop = mRect.y;
  *outWidth = mRect.width;
  *outHeight = mRect.height;

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenGtk :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = mAvailRect.x;
  *outTop = mAvailRect.y;
  *outWidth = mAvailRect.width;
  *outHeight = mAvailRect.height;

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenGtk :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  GdkVisual * rgb_visual = gdk_rgb_get_visual();
  *aPixelDepth = rgb_visual->depth;

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenGtk :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


void
nsScreenGtk :: Init (PRBool aReInit)
{
  // We listen for configure events on the root window to pick up
  // changes to this rect.  We could listen for "size_changed" signals
  // on the default screen to do this, except that doesn't work with
  // versions of GDK predating the GdkScreen object.  See bug 256646.
  mAvailRect = mRect = nsRect(0, 0, gdk_screen_width(), gdk_screen_height());

  // We need to account for the taskbar, etc in the available rect.
  // See http://freedesktop.org/Standards/wm-spec/index.html#id2767771

  // XXX do we care about _NET_WM_STRUT_PARTIAL?  That will
  // add much more complexity to the code here (our screen
  // could have a non-rectangular shape), but should
  // lead to greater accuracy.

  if (!aReInit) {
#if GTK_CHECK_VERSION(2,2,0)
    mRootWindow = gdk_get_default_root_window();
#else
    mRootWindow = GDK_ROOT_PARENT();
#endif // GTK_CHECK_VERSION(2,2,0)
    g_object_ref(mRootWindow);

    // GDK_STRUCTURE_MASK ==> StructureNotifyMask, for ConfigureNotify
    // GDK_PROPERTY_CHANGE_MASK ==> PropertyChangeMask, for PropertyNotify
    gdk_window_set_events(mRootWindow,
                          GdkEventMask(gdk_window_get_events(mRootWindow) |
                                       GDK_STRUCTURE_MASK |
                                       GDK_PROPERTY_CHANGE_MASK));
    gdk_window_add_filter(mRootWindow, root_window_event_filter, this);
    mNetWorkareaAtom =
      XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow), "_NET_WORKAREA", False);
  }

  long *workareas;
  GdkAtom type_returned;
  int format_returned;
  int length_returned;

#if GTK_CHECK_VERSION(2,0,0)
  GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);
#else
  GdkAtom cardinal_atom = (GdkAtom) XA_CARDINAL;
#endif

  gdk_error_trap_push();

  // gdk_property_get uses (length + 3) / 4, hence G_MAXLONG - 3 here.
  if (!gdk_property_get(mRootWindow,
                        gdk_atom_intern ("_NET_WORKAREA", FALSE),
                        cardinal_atom,
                        0, G_MAXLONG - 3, FALSE,
                        &type_returned,
                        &format_returned,
                        &length_returned,
                        (guchar **) &workareas)) {
    // This window manager doesn't support the freedesktop standard.
    // Nothing we can do about it, so assume full screen size.
    return;
  }

  // Flush the X queue to catch errors now.
  gdk_flush();

  if (!gdk_error_trap_pop() &&
      type_returned == cardinal_atom &&
      length_returned && (length_returned % 4) == 0 &&
      format_returned == 32) {
    int num_items = length_returned / sizeof(long);

    for (int i = 0; i < num_items; i += 4) {
      nsRect workarea(workareas[i],     workareas[i + 1],
                      workareas[i + 2], workareas[i + 3]);
      if (!mRect.Contains(workarea)) {
        // Note that we hit this when processing screen size changes,
        // since we'll get the configure event before the toolbars have
        // been moved.  We'll end up cleaning this up when we get the
        // change notification to the _NET_WORKAREA property.  However,
        // we still want to listen to both, so we'll handle changes
        // properly for desktop environments that don't set the
        // _NET_WORKAREA property.
        NS_WARNING("Invalid bounds");
        continue;
      }

      mAvailRect.IntersectRect(mAvailRect, workarea);
    }
  }
  g_free (workareas);
}

void
nsScreenGtk :: Init (XineramaScreenInfo *aScreenInfo)
{
  nsRect xineRect(aScreenInfo->x_org, aScreenInfo->y_org,
                  aScreenInfo->width, aScreenInfo->height);

  mScreenNum = aScreenInfo->screen_number;

  mAvailRect = mRect = xineRect;
}
