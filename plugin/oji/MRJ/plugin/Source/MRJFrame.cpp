/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
	MRJFrame.cpp
	
	Encapsulates a JManager frame.
	
	by Patrick C. Beard.
 */

#include "MRJFrame.h"
#include "LocalPort.h"
#include "nsplugindefs.h"

MRJFrame::MRJFrame(JMFrameRef frameRef)
	:	mFrameRef(frameRef), mActive(false), mFocused(false)
{
}

MRJFrame::~MRJFrame()
{
}

/* Stub implementations. */
void MRJFrame::setSize(const Rect* newSize) {}
void MRJFrame::invalRect(const Rect* invalidRect) {}
void MRJFrame::showHide(Boolean visible) {}
void MRJFrame::setTitle(const StringPtr title) {}
void MRJFrame::checkUpdate() {}
void MRJFrame::reorder(ReorderRequest request) {}
void MRJFrame::setResizeable(Boolean resizeable) {}

Boolean MRJFrame::handleEvent(const EventRecord* event)
{
	Boolean eventHandled = true;

	switch (event->what) {
	case nsPluginEventType_AdjustCursorEvent:
		idle(event->modifiers);
		break;
	
	case ::mouseDown:
		click(event);
		break;
	
	case keyDown:
	case autoKey:
		keyPress(event->message, event->modifiers);
		break;
	
	case keyUp:
		keyRelease(event->message, event->modifiers);
		break;

	case updateEvt:
		update();
		break;
	
	case activateEvt:
		activate((event->modifiers & activeFlag) != 0);
		break;

#if 0
	case osEvt:
		resume((event->message & resumeFlag) != 0);
		eventHandled = false;
		break;
#endif
	
	default:
		eventHandled = false;
		break;
	}
		
	return eventHandled;
}

void MRJFrame::idle(SInt16 modifiers)
{
	LocalPort port(getPort());
	port.Enter();
	
	Point pt;
	::GetMouse(&pt);
	::JMFrameMouseOver(mFrameRef, pt, modifiers);
	
	port.Exit();
}

void MRJFrame::update()
{
	GrafPtr framePort = getPort();
	if (framePort != NULL)
		::JMFrameUpdate(mFrameRef, framePort->clipRgn);
}

void MRJFrame::activate(Boolean active)
{
	if (mActive != active) {
		mActive = active;
		::JMFrameActivate(mFrameRef, active);
	}
}

void MRJFrame::focusEvent(Boolean gotFocus)
{
	if (&::JMFrameFocus != NULL) {
		if (gotFocus != mFocused) {
			if (gotFocus) {
				// HACK, until focus really works.
				if (mActive != gotFocus) {
					mActive = gotFocus;
					::JMFrameActivate(mFrameRef, gotFocus);
				}
			}
			mFocused = gotFocus;
			::JMFrameFocus(mFrameRef, gotFocus);
		}
	} else {
		if (mActive != gotFocus) {
			mActive = gotFocus;
			::JMFrameActivate(mFrameRef, gotFocus);
		}
	}
}

void MRJFrame::resume(Boolean inFront)
{
	::JMFrameResume(mFrameRef, inFront);
}

void MRJFrame::click(const EventRecord* event)
{
	// make the frame's port current, and move its origin to (0, 0).
	// this is needed to transform the mouse click location to frame coordinates.
	LocalPort port(getPort());
	port.Enter();

	Point localWhere = event->where;
	::GlobalToLocal(&localWhere);
	click(event, localWhere);
	
	// restore the plugin port's origin, and restore the current port.
	port.Exit();
}

void MRJFrame::click(const EventRecord* event, Point localWhere)
{
	if (&::JMFrameClickWithEventRecord != NULL)
		::JMFrameClickWithEventRecord(mFrameRef, localWhere, event);
	else
		::JMFrameClick(mFrameRef, localWhere, event->modifiers);
}

void MRJFrame::keyPress(UInt32 message, SInt16 modifiers)
{
	::JMFrameKey(mFrameRef, message & charCodeMask, (message & keyCodeMask) >> 8, modifiers);
}

void MRJFrame::keyRelease(UInt32 message, SInt16 modifiers)
{
	::JMFrameKeyRelease(mFrameRef, message & charCodeMask, (message & keyCodeMask) >> 8, modifiers);
}

void MRJFrame::menuSelected(UInt32 message, SInt16 modifiers)
{
	MenuHandle menu = ::GetMenuHandle(short(message >> 16));
	if (menu != NULL) {
		short item = short(message);
		if (&::JMMenuSelectedWithModifiers != NULL)
			::JMMenuSelectedWithModifiers(::JMGetFrameContext(mFrameRef), menu, item, modifiers);
		else
			::JMMenuSelected(::JMGetFrameContext(mFrameRef), menu, item);
	}
}

void MRJFrame::print(GrafPtr printingPort, Point frameOrigin)
{
	OSStatus status = JMDrawFrameInPort(mFrameRef, printingPort, frameOrigin, printingPort->clipRgn, false);
	if (status != noErr) {
		::MoveTo(10, 12);
		::TextFont(0);
		::TextSize(12);
		::DrawString("\pMRJPlugin:  printing failed.");
	}
}
