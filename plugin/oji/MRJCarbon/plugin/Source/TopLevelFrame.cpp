/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
	TopLevelFrame.cpp
	
	An MRJFrame sub-class that manages the behavior of a top-level window
	running inside the Communicator.
	
	by Patrick C. Beard.
 */

#include <Controls.h>
#include <Events.h>

#include "TopLevelFrame.h"
#include "LocalPort.h"

#include "nsIPluginManager2.h"
#include "nsIEventHandler.h"

#if !defined(MRJPLUGIN_4X)
#define USE_ALT_WINDOW_HANDLING
#endif

#ifdef USE_ALT_WINDOW_HANDLING
#include "AltWindowHandling.h"
#endif

#include "nsIEventHandler.h"
#include "AltWindowHandling.h"

extern nsIPluginManager2* thePluginManager2;

static void UnsetPort(GrafPtr port);
static short getModifiers();

TopLevelFrame::TopLevelFrame(nsIEventHandler* handler, JMFrameRef frameRef, JMFrameKind kind,
							const Rect* initialBounds, Boolean resizeable)
	:	MRJFrame(frameRef),
		mHandler(handler), mWindow(NULL), mBounds(*initialBounds)
{
	Boolean hasGoAway = true;
	SInt16 windowProc = documentProc;
	SInt16 resizeHeight = resizeable ? 15 : 0;
	
	switch (kind) {
	case eBorderlessModelessWindowFrame:
		hasGoAway = false;
		windowProc = plainDBox;
		// mBounds.bottom += resizeHeight;
		resizeable = false;
		break;
	case eModelessWindowFrame:
	case eModelessDialogFrame:
		hasGoAway = true;
		windowProc = resizeable ? zoomDocProc : documentProc;
		// mBounds.bottom += resizeHeight;
		break;
	case eModalWindowFrame:
		hasGoAway = true;
		// We have to allow resizeable modal windows.
		windowProc = resizeable ? documentProc : movableDBoxProc;
		break;
	}
	
	mWindow = ::NewCWindow(NULL, &mBounds, "\p", false, windowProc, WindowPtr(-1), hasGoAway, long(this));
	if (mWindow != NULL) {
		Point zeroPt = { 0, 0 };
		::JMSetFrameVisibility(frameRef, mWindow, zeroPt, mWindow->clipRgn);
	}
}

TopLevelFrame::~TopLevelFrame()
{
	// make sure the window is hidden (and unregistered with the browser).
	showHide(false);

	// make sure this port isn't ever current again.
	::UnsetPort(mWindow);

	if (mWindow != NULL)
		::DisposeWindow(mWindow);
}

void TopLevelFrame::setSize(const Rect* newSize)
{
	mBounds = *newSize;

	if (mWindow != NULL) {
		SInt16 width = newSize->right - newSize->left;
		SInt16 height = newSize->bottom - newSize->top;
		::SizeWindow(mWindow, width, height, true);
		::MoveWindow(mWindow, newSize->left, newSize->top, false);
	}
}

void TopLevelFrame::invalRect(const Rect* invalidRect)
{
	if (mWindow != NULL) {
		::InvalRect(invalidRect);
	}
}

void TopLevelFrame::showHide(Boolean visible)
{
	if (mWindow != NULL && visible != IsWindowVisible(mWindow)) {
		if (visible) {
#if !defined(USE_ALT_WINDOW_HANDLING)		
			// Have to notify the browser that this window exists, so that it will receive events.
			thePluginManager2->RegisterWindow(mHandler, mWindow);
			// the plugin manager takes care of showing the window.
			// ::ShowWindow(mWindow);
			// ::SelectWindow(mWindow);
#else
            AltRegisterWindow(mHandler, mWindow);
#endif
		} else {
#if defined(USE_ALT_WINDOW_HANDLING)		
            AltUnregisterWindow(mHandler, mWindow);
#else
 			// the plugin manager takes care of hiding the window.
 			// ::HideWindow(mWindow);
 			// Let the browser know it doesn't have to send events anymore.
 			thePluginManager2->UnregisterWindow(mHandler, mWindow);
#endif
			activate(false);
		}
		
		// ::ShowHide(mWindow, visible);
	}
}

void TopLevelFrame::setTitle(const StringPtr title)
{
	if (mWindow != NULL) {
		::SetWTitle(mWindow, title);
	}
}

void TopLevelFrame::checkUpdate()
{
}

void TopLevelFrame::reorder(ReorderRequest request)
{
	switch (request) {
	case eBringToFront:		/* bring the window to front */
		::BringToFront(mWindow);
		break;
	case eSendToBack:		/* send the window to back */
		::SendBehind(mWindow, NULL);
		break;
	case eSendBehindFront:	/* send the window behind the front window */
		WindowPtr frontWindow = ::FrontWindow();
		if (mWindow == frontWindow) {
			::SendBehind(mWindow, GetNextWindow(mWindow));
		} else {
			::SendBehind(mWindow, frontWindow);
		}
		break;
	}
}

void TopLevelFrame::setResizeable(Boolean resizeable)
{
	// this might have to recreate the window, no?
}

static void computeBounds(WindowRef window, Rect* bounds)
{
	LocalPort port(window);
	port.Enter();
	
		Point position = { 0, 0 };
		::LocalToGlobal(&position);
		
		*bounds = window->portRect;
	
	port.Exit();
	
	::OffsetRect(bounds, position.h, position.v);
}

void TopLevelFrame::activate(Boolean active)
{
	focusEvent(active);
	MRJFrame::activate(active);
}

void TopLevelFrame::click(const EventRecord* event)
{
	Point where = event->where;
	SInt16 modifiers = event->modifiers;
	WindowRef hitWindow;
	short partCode = ::FindWindow(where, &hitWindow);
	switch (partCode) {
	case inContent:
		::SelectWindow(mWindow);
		MRJFrame::click(event);
		break;
	case inDrag:
		{
			Rect bounds = (**GetGrayRgn()).rgnBBox;
			DragWindow(mWindow, where, &bounds);
			computeBounds(mWindow, &bounds);
			::JMSetFrameSize(mFrameRef, &bounds);

			Point zeroPt = { 0, 0 };
			::JMSetFrameVisibility(mFrameRef, mWindow, zeroPt, mWindow->clipRgn);
		}
		break;
	case inGrow:
		Rect limits = { 30, 30, 5000, 5000 };
		long result = GrowWindow(mWindow, where, &limits);
		if (result != 0) {
			short width = (result & 0xFFFF);
			short height = (result >> 16) & 0xFFFF;
			Rect newBounds;
			topLeft(newBounds) = topLeft(mBounds);
			newBounds.right = newBounds.left + width;
			newBounds.bottom = newBounds.top + height;
			::JMSetFrameSize(mFrameRef, &newBounds);

			Point zeroPt = { 0, 0 };
			::JMSetFrameVisibility(mFrameRef, mWindow, zeroPt, mWindow->clipRgn);
		}
		break;
	case inGoAway:
		if (::TrackGoAway(mWindow, where))
			::JMFrameGoAway(mFrameRef);
		break;
	case inZoomIn:
	case inZoomOut:
		if (::TrackBox(mWindow, where, partCode)) {
			ZoomWindow(mWindow, partCode, true);
			computeBounds(mWindow, &mBounds);
			::JMSetFrameSize(mFrameRef, &mBounds);
		}
		break;
	case inCollapseBox:
		break;
	}
}

WindowRef TopLevelFrame::getWindow()
{
	return mWindow;
}

GrafPtr TopLevelFrame::getPort()
{
	return mWindow;
}

static void UnsetPort(GrafPtr port)
{
	GrafPtr curPort;
	::GetPort(&curPort);
	if (curPort == port) {
		::GetWMgrPort(&port);
		::SetPort(port);
	}
}

static short getModifiers()
{
	EventRecord event;
	::OSEventAvail(0, &event);
	return event.modifiers;
}
