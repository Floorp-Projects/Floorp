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


#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>

#include "plugin.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h" // some usefule macros are defined here

#define MIME_TYPES_HANDLED  "application/scriptable-plugin"
#define PLUGIN_NAME         "Scriptable Example Plugin for Mozilla"
#define PLUGIN_DESCRIPTION  MIME_TYPES_HANDLED":scr:"PLUGIN_NAME

char* NPP_GetMIMEDescription(void)
{
    return(PLUGIN_DESCRIPTION);
}

//////////////////////////////////////
//
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
nsPluginInstanceBase * NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct)
{
  if(!aCreateDataStruct)
    return NULL;

  nsPluginInstance * plugin = new nsPluginInstance(aCreateDataStruct->instance);
  return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
  if(aPlugin)
    delete (nsPluginInstance *)aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance) : nsPluginInstanceBase(),
  mInstance(aInstance),
  mInitialized(FALSE),
  mWindow(0),
  mXtwidget(0),
  mFontInfo(0),
  mScriptablePeer(NULL)
{
  mString[0] = '\0';
}

nsPluginInstance::~nsPluginInstance()
{
  // mScriptablePeer may be also held by the browser 
  // so releasing it here does not guarantee that it is over
  // we should take precaution in case it will be called later
  // and zero its mPlugin member
  mScriptablePeer->SetInstance(NULL);
  NS_IF_RELEASE(mScriptablePeer);
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
  XClearArea(mDisplay, mWindow, 0, 0, 0, 0, False);
  if (x >= 0 && y >= 0) {
    GC gc = XCreateGC(mDisplay, mWindow, 0, NULL);
    if (!gc)
      return;
    XDrawRectangle(mDisplay, mWindow, gc, x, y, w, h);
    if (mString[0]) {
  int l = strlen(mString);
  int fmba = mFontInfo->max_bounds.ascent;
  int fmbd = mFontInfo->max_bounds.descent;
  int fh = fmba + fmbd;
  y += fh;
  x += 32;
      XDrawString(mDisplay, mWindow, gc, x, y, mString, l);
    }
    XFreeGC(mDisplay, gc);
  }
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{
  if(aWindow == NULL)
    return FALSE;

  if (SetWindow(aWindow))
    mInitialized = TRUE;
         
  return mInitialized;
}

NPError nsPluginInstance::SetWindow(NPWindow* aWindow)
{
  if(aWindow == NULL)
    return FALSE;

  mX = aWindow->x;
  mY = aWindow->y;
  mWidth = aWindow->width;
  mHeight = aWindow->height;
  if (mWindow != (Window) aWindow->window) {
    mWindow = (Window) aWindow->window;
    NPSetWindowCallbackStruct *ws_info = (NPSetWindowCallbackStruct *)aWindow->ws_info;
  mDisplay = ws_info->display;
  mVisual = ws_info->visual;
  mDepth = ws_info->depth;
  mColormap = ws_info->colormap;

  if (!mFontInfo) {
      if (!(mFontInfo = XLoadQueryFont(mDisplay, "9x15")))
    printf("Cannot open 9X15 font\n");
  }
  // add xt event handler
  Widget xtwidget = XtWindowToWidget(mDisplay, mWindow);
    if (xtwidget && mXtwidget != xtwidget) {
      mXtwidget = xtwidget;
    long event_mask = ExposureMask;
    XSelectInput(mDisplay, mWindow, event_mask);
    XtAddEventHandler(xtwidget, event_mask, False, (XtEventHandler)xt_event_handler, this);
  }
  }
  draw();
  return TRUE;
}


void nsPluginInstance::shut()
{
  mInitialized = FALSE;
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}

// this will force to draw a version string in the plugin window
void nsPluginInstance::showVersion()
{
  const char *ua = NPN_UserAgent(mInstance);
  strcpy(mString, ua);
  draw();
}

// this will clean the plugin window
void nsPluginInstance::clear()
{
  strcpy(mString, "");
  draw();
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// here the plugin is asked by Mozilla to tell if it is scriptable
// we should return a valid interface id and a pointer to 
// nsScriptablePeer interface which we should have implemented
// and which should be defined in the corressponding *.xpt file
// in the bin/components folder
NPError	nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
  NPError rv = NPERR_NO_ERROR;

  if (aVariable == NPPVpluginScriptableInstance) {
      // addref happens in getter, so we don't addref here
    nsIScriptablePluginSample * scriptablePeer = getScriptablePeer();
      if (scriptablePeer) {
        *(nsISupports **)aValue = scriptablePeer;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
    }
  else if (aVariable == NPPVpluginScriptableIID) {
    static nsIID scriptableIID =  NS_ISCRIPTABLEPLUGINSAMPLE_IID;
      nsIID* ptr = (nsIID *)NPN_MemAlloc(sizeof(nsIID));
      if (ptr) {
          *ptr = scriptableIID;
          *(nsIID **)aValue = ptr;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
  }

  return rv;
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// this method will return the scriptable object (and create it if necessary)
nsScriptablePeer* nsPluginInstance::getScriptablePeer()
{
  if (!mScriptablePeer) {
    mScriptablePeer = new nsScriptablePeer(this);
    if(!mScriptablePeer)
      return NULL;

    NS_ADDREF(mScriptablePeer);
  }

  // add reference for the caller requesting the object
  NS_ADDREF(mScriptablePeer);
  return mScriptablePeer;
}
