/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created Christopher Blizzard are Copyright (C) Christopher
 * Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsPointerService.h"
#include "nsWindow.h"

NS_IMPL_ADDREF_INHERITED(nsPointerService, nsBasePointerService)
NS_IMPL_RELEASE_INHERITED(nsPointerService, nsBasePointerService)
NS_IMPL_QUERY_INTERFACE_INHERITED0(nsPointerService, nsBasePointerService)


NS_IMETHODIMP
nsPointerService::WidgetUnderPointer(nsIWidget **_retval,
				     PRUint32 *aXOffset, PRUint32 *aYOffset)
{
  *_retval = nsnull;
  *aXOffset = 0;
  *aYOffset = 0;

  Bool retval;
  Window root_return;
  Window child_return = None;
  int root_x_return, root_y_return;
  int win_x_return, win_y_return;
  unsigned int mask_return;

  // Flush the queue to get any pending destroys out the door.  If we
  // don't then we can end up with a sitution where the XQueryPointer
  // flushes the queue and then the XTranslateCoordinates will cause
  // an X error since the window doesn't exist anymore.  God,
  // sometimes I just hate X.
  XSync(GDK_DISPLAY(), False);

  // Query the pointer
  retval = XQueryPointer(GDK_DISPLAY(),
			 GDK_WINDOW_XWINDOW(GDK_ROOT_PARENT()),
			 &root_return, &child_return,
			 &root_x_return, &root_y_return,
			 &win_x_return, &win_y_return,
			 &mask_return);

  // the pointer is on a different window
  if (!retval || child_return == None)
    return NS_OK;

  int done = 0;
  Window dest_w = child_return;
  int    xlate_x_return;
  int    xlate_y_return;

  // loop to find the inner most window
  while (!done) {
    Window xlate_return = None;
    retval = XTranslateCoordinates(GDK_DISPLAY(),
				   GDK_WINDOW_XWINDOW(GDK_ROOT_PARENT()),
				   dest_w,
				   win_x_return, win_y_return,
				   &xlate_x_return, &xlate_y_return,
				   &xlate_return);
    
    // the pointer is on a different screen
    if (!retval)
      return NS_OK;

    // if xlate_return was None then we've reached the inner most window
    if (xlate_return == None)
      done = 1;
    // otherwise set our new destination window to the return from the
    // translation
    else
      dest_w = xlate_return;
  }

  GdkWindow *window;
  nsWindow  *widget;

  // get the gdk window under the pointer
  window = gdk_window_lookup(dest_w);
  // it's not a gdk window
  if (!window)
    return NS_OK;
  
  // is that an nsWindow window?
  gpointer data = NULL;
  gdk_window_get_user_data(window, &data);
  // nope
  if (!data)
    return NS_OK;

  // downcast
  widget = (nsWindow *)gtk_object_get_data(GTK_OBJECT(data), "nsWindow");
  if (!widget)
    return NS_OK;

  *_retval = NS_STATIC_CAST(nsIWidget *, widget);
  *aXOffset = xlate_x_return;
  *aYOffset = xlate_y_return;

  NS_ADDREF(*_retval);

  return NS_OK;
}
