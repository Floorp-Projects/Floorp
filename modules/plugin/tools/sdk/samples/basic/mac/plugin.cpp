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


#include <string.h>
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
  mInitialized(FALSE)
{
  mWindow = NULL;
}

nsPluginInstance::~nsPluginInstance()
{
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{
  if(aWindow == NULL)
    return FALSE;
  
  mWindow = aWindow;
  mInitialized = TRUE;
  return TRUE;
}

void nsPluginInstance::shut()
{
  mWindow = NULL;
  mInitialized = FALSE;
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}

const char * nsPluginInstance::getVersion()
{
  return NPN_UserAgent(mInstance);
}

CGrafPort gSavePort;
CGrafPtr  gOldPort;

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * DoDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
void 
nsPluginInstance::DoDraw(void)
{
	Rect drawRect;
	SInt32		height = mWindow->height;
	SInt32		width = mWindow->width;
	SInt32		centerX = (width) >> 1;
	SInt32		centerY = (height) >> 1;

	const char * ua = getVersion();
	char* pascalString = (char*) NPN_MemAlloc(strlen(ua) + 1);
	strcpy(pascalString, ua);
	
	drawRect.top = 0;
	drawRect.left = 0;
	drawRect.bottom = drawRect.top + height;
	drawRect.right = drawRect.left + width;
	
	EraseRect(&drawRect);
	FrameRect(&drawRect);
	MoveTo( centerX - 0.5 * StringWidth(c2pstr(pascalString)), centerY );
	DrawString(c2pstr(pascalString));
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformSetWindow
 *
 * Perform platform-specific window operations
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
NPError
nsPluginInstance::SetWindow(NPWindow* window)
{
	mWindow = window;
	if( StartDraw(window) ) {
		DoDraw();
		EndDraw(window);
	}
	return NPERR_NO_ERROR;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformHandleEvent
 *
 * Handle platform-specific events.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
uint16
nsPluginInstance::HandleEvent(void* event)
{
	int16 eventHandled = FALSE;
	
	EventRecord* ev = (EventRecord*) event;
	if (event != NULL)
	{
		switch (ev->what)
		{
			/*
			 * Draw ourselves on update events
			 */
			case updateEvt:
				if( StartDraw(mWindow) ) {
					DoDraw();
					EndDraw(mWindow);
				}
				eventHandled = true;
				break;

			default:
				break;
		}
	}
	return eventHandled;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * StartDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
NPBool
nsPluginInstance::StartDraw(NPWindow* window)
{
	NP_Port* port;
	Rect clipRect;
	RGBColor  col;
	
	if (window == NULL)
		return FALSE;
	port = (NP_Port*) window->window;
	if (window->clipRect.left < window->clipRect.right)
	{
	/* Preserve the old port */
		GetPort((GrafPtr*)&gOldPort);
		SetPort((GrafPtr)port->port);
	/* Preserve the old drawing environment */
		gSavePort.portRect = port->port->portRect;
		gSavePort.txFont = port->port->txFont;
		gSavePort.txFace = port->port->txFace;
		gSavePort.txMode = port->port->txMode;
		gSavePort.rgbFgColor = port->port->rgbFgColor;
		gSavePort.rgbBkColor = port->port->rgbBkColor;
		GetClip(gSavePort.clipRgn);
	/* Setup our drawing environment */
		clipRect.top = window->clipRect.top + port->porty;
		clipRect.left = window->clipRect.left + port->portx;
		clipRect.bottom = window->clipRect.bottom + port->porty;
		clipRect.right = window->clipRect.right + port->portx;
		SetOrigin(port->portx,port->porty);
		ClipRect(&clipRect);
		clipRect.top = clipRect.left = 0;
		TextSize(12);
		TextFont(20);
		TextMode(srcCopy);
		col.red = col.green = col.blue = 0;
		RGBForeColor(&col);
		col.red = col.green = col.blue = 65000;
		RGBBackColor(&col);
		return TRUE;
	}
	else
		return FALSE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * EndDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
void
nsPluginInstance::EndDraw(NPWindow* window)
{
	CGrafPtr myPort;
	NP_Port* port = (NP_Port*) window->window;
	SetOrigin(gSavePort.portRect.left, gSavePort.portRect.top);
	SetClip(gSavePort.clipRgn);
	GetPort((GrafPtr*)&myPort);
	myPort->txFont = gSavePort.txFont;
	myPort->txFace = gSavePort.txFace;
	myPort->txMode = gSavePort.txMode;
	RGBForeColor(&gSavePort.rgbFgColor);
	RGBBackColor(&gSavePort.rgbBkColor);
	SetPort((GrafPtr)gOldPort);
}



