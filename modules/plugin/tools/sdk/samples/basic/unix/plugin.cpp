/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plugin.h"

#define MIME_TYPES_HANDLED  "application/basic-plugin"
#define PLUGIN_NAME         "Basic Example Plugin for Mozilla"
#define PLUGIN_DESCRIPTION  "Basic Example Plugin for Mozilla"

char* NPP_GetMIMEDescription(void)
{
    return(MIME_TYPES_HANDLED);
}

/////////////////////////////////////
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
  return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
}

/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase * NS_NewPluginInstance(NPP aInstance)
{
  nsPluginInstance * plugin = new nsPluginInstance(aInstance);
  return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
  if(aPlugin)
    delete aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance) : nsPluginInstanceBase(),
  mInstance(aInstance),
  mWindow(0),
  mInitialized(FALSE)
{
}

nsPluginInstance::~nsPluginInstance()
{
}

static void
xt_event_handler(Widget xtwidget, nsPluginInstance *plugin, XEvent *xevent, Boolean *b)
{
  switch (xevent->type) {
    case Expose:
      // get rid of all other exposure events
      if (plugin) {
        //while(XCheckTypedWindowEvent(plugin->Display(), plugin->Window(), Expose, xevent));
        plugin->draw();
      }
      default:
        break;
  }
}

void nsPluginInstance::draw()
{
  unsigned int h = mHeight/2;
  unsigned int w = 3 * mWidth/4;
  int x = (mWidth - w)/2; // center
  int y = h/2;
  XDrawRectangle(mDisplay, mWindow, mGC, x, y, w, h);
  const char *string = getVersion();
  if (string && *string) {
    int l = strlen(string);
    int fmba = mFontInfo->max_bounds.ascent;
    int fmbd = mFontInfo->max_bounds.descent;
    int fh = fmba + fmbd;
    y += fh;
    x += 32;
    XDrawString(mDisplay, mWindow, mGC, x, y, string, l); 
  }
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{

  if(aWindow == NULL)
    return FALSE;
  
  NPSetWindowCallbackStruct *ws_info = (NPSetWindowCallbackStruct *)aWindow->ws_info;
  mWindow = (Window) aWindow->window;
  mX = aWindow->x;
  mY = aWindow->y;
  mWidth = aWindow->width;
  mHeight = aWindow->height;
  mDisplay = ws_info->display;
  mVisual = ws_info->visual;
  mDepth = ws_info->depth;
  mColormap = ws_info->colormap;
  mFontInfo = XLoadQueryFont(mDisplay, "9x15");
  if (!mFontInfo) {
    printf("Cannot open 9X15 font\n");
    return FALSE;
  }
  mGC = XCreateGC(mDisplay, mWindow, 0, NULL);

  // add xt event handler
  Widget xtwidget = XtWindowToWidget(mDisplay, mWindow);
  if (xtwidget) {
    long event_mask = ExposureMask;
    XSelectInput(mDisplay, mWindow, event_mask);
    XtAddEventHandler(xtwidget, event_mask, False, (XtEventHandler)xt_event_handler, this);
  }
  mInitialized = TRUE;
  return TRUE;
}

void nsPluginInstance::shut()
{
  mInitialized = FALSE;
}

const char * nsPluginInstance::getVersion()
{
  return NPN_UserAgent(mInstance);
}

NPError nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
  NPError err = NPERR_NO_ERROR;
  switch (aVariable) {
    case NPPVpluginNameString:
      *((char **)aValue) = PLUGIN_NAME;
      break;
    case NPPVpluginDescriptionString:
      *((char **)aValue) = PLUGIN_DESCRIPTION;
      break;
    default:
      break;
  }
  return err;
}
