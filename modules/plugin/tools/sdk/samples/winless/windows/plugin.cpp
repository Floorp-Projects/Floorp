/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#include <windows.h>
#include <windowsx.h>

#include "plugin.h"

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

  // now is the time to tell Mozilla that we are windowless
  NPN_SetValue(aCreateDataStruct->instance, NPPVpluginWindowBool, NULL);

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
  mInitialized(FALSE)
{
}

nsPluginInstance::~nsPluginInstance()
{
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{
  // no GUI to init in windowless case
  mInitialized = TRUE;
  return TRUE;
}

void nsPluginInstance::shut()
{
  // no GUI to shut in windowless case
  mInitialized = FALSE;
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}

NPError nsPluginInstance::SetWindow(NPWindow* aWindow)
{
  // keep window parameters
  mWindow = aWindow;
  return NPERR_NO_ERROR;
}

uint16 nsPluginInstance::HandleEvent(void* aEvent)
{
  NPEvent * event = (NPEvent *)aEvent;
  switch (event->event) {
    case WM_PAINT: 
    {
      if(!mWindow)
        break;

      // get the dirty rectangle to update or repaint the whole window
      RECT * drc = (RECT *)event->lParam;
      if(drc)
        FillRect((HDC)event->wParam, drc, (HBRUSH)(COLOR_ACTIVECAPTION+1));
      else {
        RECT rc;
        rc.bottom = mWindow->y + mWindow->height;
        rc.left   = mWindow->x;
        rc.right  = mWindow->x + mWindow->width;
        rc.top    = mWindow->y;
        FillRect((HDC)event->wParam, &rc, (HBRUSH)(COLOR_ACTIVECAPTION+1));
      }
      break;
    }
    case WM_KEYDOWN:
    {
      Beep(1000,100);
      break;
    }
    default:
      return 0;
  }
  return 1;
}