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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <X11/Xatom.h> // for XA_STRING
#include <stdlib.h>
#include <nsIWidget.h>
#include <nsIXRemoteService.h>
#include <nsCOMPtr.h>
#include <nsIServiceManager.h>
#include "nsGtkMozRemoteHelper.h"


#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"

Atom nsGtkMozRemoteHelper::sMozVersionAtom  = 0;
Atom nsGtkMozRemoteHelper::sMozLockAtom     = 0;
Atom nsGtkMozRemoteHelper::sMozCommandAtom  = 0;
Atom nsGtkMozRemoteHelper::sMozResponseAtom = 0;

// XXX get this dynamically
static char *sRemoteVersion          = "5.0";

void
nsGtkMozRemoteHelper::SetupVersion(GdkWindow *aWindow)
{
  Window window;
  unsigned char *data = (unsigned char *)sRemoteVersion;
  EnsureAtoms();
  window = GDK_WINDOW_XWINDOW(aWindow);

  XChangeProperty(GDK_DISPLAY(), window, sMozVersionAtom, XA_STRING,
		  8, PropModeReplace, data, strlen(sRemoteVersion));

}

gboolean
nsGtkMozRemoteHelper::HandlePropertyChange(GtkWidget *aWidget,
                                           GdkEventProperty *aEvent,
                                           nsIWidget *ansIWidget)
{

  EnsureAtoms();

  // see if this is the command atom and it's new
  if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
      aEvent->window == aWidget->window &&
      aEvent->atom == sMozCommandAtom)
  {
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    char *data = 0;

    result = XGetWindowProperty (GDK_DISPLAY(),
				 GDK_WINDOW_XWINDOW(aWidget->window),
				 sMozCommandAtom,
				 0,                        /* long_offset */
				 (65536 / sizeof (long)),  /* long_length */
				 True,                     /* atomic delete after */
				 XA_STRING,                /* req_type */
				 &actual_type,             /* actual_type return */
				 &actual_format,           /* actual_format_return */
				 &nitems,                  /* nitems_return */
				 &bytes_after,             /* bytes_after_return */
				 (unsigned char **)&data); /* prop_return
							      ( we only care about 
							      the first ) */
    if (result != Success)
    {
      // failed to get property off the window
      return FALSE;
    }
    else if (!data || !*data)
    {
      // failed to get the data off the window or it was the wrong
      // type
      return FALSE;
    }
    // cool, we got the property data.
    char *response = NULL;
    // The reason that we are using a conditional free here is that if
    // ParseCommand() fails below it's probably because of failed
    // memory allocations.  If that's the case we want to make sure
    // that we try to get something back to the client instead of just
    // giving up since a lot of clients will just hang forever.  Hence
    // using a static string.
    PRBool freeResponse = PR_TRUE;

    // parse the command
    nsCOMPtr<nsIXRemoteService> remoteService;
    remoteService = do_GetService(NS_IXREMOTESERVICE_CONTRACTID);

    if (remoteService)
      remoteService->ParseCommand(ansIWidget, data, &response);

    if (!response)
    {
      response = "500 error parsing command";
      freeResponse = PR_FALSE;
    }
    // put the property onto the window as the response
    XChangeProperty (GDK_DISPLAY(), GDK_WINDOW_XWINDOW(aWidget->window),
		     sMozResponseAtom, XA_STRING,
		     8, PropModeReplace, (const unsigned char *)response, strlen (response));
    if (freeResponse)
      nsCRT::free(response);
    XFree(data);
    return TRUE;
  }
  else if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
	   aEvent->window == aWidget->window &&
	   aEvent->atom == sMozResponseAtom)
  {
    // client accepted the response.  party on wayne.
    return TRUE;
  }
  else if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
	   aEvent->window == aWidget->window &&
	   aEvent->atom == sMozLockAtom)
  {
    // someone locked the window
    return TRUE;
  }
  return FALSE;
}

void
nsGtkMozRemoteHelper::EnsureAtoms(void)
{
 // init our atoms if we need to
  if (!sMozVersionAtom)
    sMozVersionAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_VERSION_PROP, False);
  if (!sMozLockAtom)
    sMozLockAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_LOCK_PROP, False);
  if (!sMozCommandAtom)
    sMozCommandAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_COMMAND_PROP, False);
  if (!sMozResponseAtom)
    sMozResponseAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_RESPONSE_PROP, False);

}


nsGtkMozRemoteHelper::~nsGtkMozRemoteHelper()
{
}

nsGtkXRemoteWidgetHelper::nsGtkXRemoteWidgetHelper()
{
  NS_INIT_ISUPPORTS();
}

nsGtkXRemoteWidgetHelper::~nsGtkXRemoteWidgetHelper()
{
}

NS_IMPL_ISUPPORTS1(nsGtkXRemoteWidgetHelper, nsIXRemoteWidgetHelper)

NS_IMETHODIMP
nsGtkXRemoteWidgetHelper::EnableXRemoteCommands(nsIWidget *aWidget)
{
  // find the native gdk window
  GdkWindow *window = NS_STATIC_CAST(GdkWindow *,
				     aWidget->GetNativeData(NS_NATIVE_WINDOW));
  if (!window)
    return NS_ERROR_FAILURE;

  // find the toplevel gdk window
  GdkWindow *temp = window;

  while (temp) {
    temp = gdk_window_get_parent(window);
    if (!temp || temp == GDK_ROOT_PARENT())
      break;
    window = temp;
  }

  // ok, found the toplevel window - set up the version information
  nsGtkMozRemoteHelper::SetupVersion(window);
  
  return NS_OK;
}

