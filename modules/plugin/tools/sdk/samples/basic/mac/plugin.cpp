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

#include "plugin.h"

#include <string.h>

#if !TARGET_API_MAC_CARBON
extern QDGlobals*	gQDPtr;
#endif


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
  mSaveClip = NewRgn();
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

/////////////////////////////////////////////////////////////
//
// DrawString
//
void
nsPluginInstance::DrawString(const unsigned char* text, 
                             short width, 
                             short height, 
                             short centerX, 
                             Rect drawRect)
{
	short length, textHeight, textWidth;
 
	if(text == NULL)
		return;
	
	length = strlen((char*)text);
	TextFont(1);
	TextFace(bold);
	TextMode(srcCopy);
	TextSize(12);
	
	FontInfo fontInfo;
	GetFontInfo(&fontInfo);

	textHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;
	textWidth = TextWidth(text, 0, length);
		
	if (width > textWidth && height > textHeight)
	{
		MoveTo(centerX - (textWidth >> 1), height >> 1);
		DrawText(text, 0, length);
	}		
}

/////////////////////////////////////////////////////////////
//
// DoDraw - paint
//
void 
nsPluginInstance::DoDraw(void)
{
	Rect drawRect;
  RGBColor	black = { 0x0000, 0x0000, 0x0000 };
  RGBColor	white = { 0xFFFF, 0xFFFF, 0xFFFF };
	SInt32		height = mWindow->height;
	SInt32		width = mWindow->width;
	SInt32		centerX = (width) >> 1;
	SInt32		centerY = (height) >> 1;

	const char * ua = getVersion();
	char* pascalString = (char*) NPN_MemAlloc(strlen(ua) + 1);
	strcpy(pascalString, ua);
	UInt8		*pTheText = (unsigned char*) ua;

	drawRect.top = 0;
	drawRect.left = 0;
	drawRect.bottom = drawRect.top + height;
	drawRect.right = drawRect.left + width;

  PenNormal();
  RGBForeColor(&black);
  RGBBackColor(&white);

#if !TARGET_API_MAC_CARBON
  FillRect(&drawRect, &(gQDPtr->white));
#else
  Pattern qdWhite;
  FillRect(&drawRect, GetQDGlobalsWhite(&qdWhite));
#endif

	FrameRect(&drawRect);
  DrawString(pTheText, width, height, centerX, drawRect);
}

/////////////////////////////////////////////////////////////
//
// SetWindow
//
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

/////////////////////////////////////////////////////////////
//
// HandleEvent
//
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

/////////////////////////////////////////////////////////////
//
// StartDraw - setup port state
//
NPBool
nsPluginInstance::StartDraw(NPWindow* window)
{
	if (mWindow == NULL)
		return false;
		
	NP_Port* npport = (NP_Port*) mWindow->window;
	CGrafPtr ourPort = npport->port;
	
	if (mWindow->clipRect.left < mWindow->clipRect.right)
	{
		GetPort(&mSavePort);
		SetPort((GrafPtr) ourPort);
    Rect portRect;
#if !TARGET_API_MAC_CARBON
    portRect = ourPort->portRect;
#else
    GetPortBounds(ourPort, &portRect);
#endif
		mSavePortTop = portRect.top;
		mSavePortLeft = portRect.left;
		GetClip(mSaveClip);
		
		mRevealedRect.top = mWindow->clipRect.top + npport->porty;
		mRevealedRect.left = mWindow->clipRect.left + npport->portx;
		mRevealedRect.bottom = mWindow->clipRect.bottom + npport->porty;
		mRevealedRect.right = mWindow->clipRect.right + npport->portx;
		SetOrigin(npport->portx, npport->porty);
		ClipRect(&mRevealedRect);

		return true;
	}
	else
		return false;
}

/////////////////////////////////////////////////////////////
//
// EndDraw - restore port state
//
void
nsPluginInstance::EndDraw(NPWindow* window)
{
	SetOrigin(mSavePortLeft, mSavePortTop);
	SetClip(mSaveClip);
	SetPort(mSavePort);
}



