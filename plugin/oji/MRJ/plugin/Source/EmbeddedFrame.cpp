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
	TopLevelFrame.cpp
	
	An MRJFrame sub-class that manages the behavior of a top-level window
	running inside the Communicator.
	
	by Patrick C. Beard.
 */

#include <Controls.h>
#include <Events.h>

#include "EmbeddedFrame.h"
#include "EmbeddedFramePluginInstance.h"
#include "MRJPlugin.h"
#include "MRJSession.h"

#include "nsIPluginInstancePeer.h"
#include "nsIOutputStream.h"
#include "JSEvaluator.h"
#include "LocalPort.h"
#include "StringUtils.h"

static void UnsetPort(GrafPtr port);
static short getModifiers();

EmbeddedFrame::EmbeddedFrame(MRJPluginInstance* pluginInstance, JMFrameRef frameRef, JMFrameKind kind,
							const Rect* initialBounds, Boolean resizeable)
	:	MRJFrame(frameRef),
		mPluginInstance(NULL), mEvaluator(NULL), mWindow(NULL), mBounds(*initialBounds)
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

#if 0
	mWindow = ::NewCWindow(NULL, &mBounds, "\p", false, windowProc, WindowPtr(-1), hasGoAway, long(this));
	if (mWindow != NULL) {
		if (getModifiers() & controlKey) {
			// hack: Try creating a root control, to see if that messes up MRJ controls.
			ControlHandle rootControl = NULL;
			OSErr result = ::GetRootControl(mWindow, &rootControl);
			if (result != noErr || rootControl == NULL) {
				result = ::CreateRootControl(mWindow, &rootControl);
				if (result == noErr && rootControl != NULL) {
					FSSpec dumpFile = { -1, 2, "\pJava Console Controls" };
					result = DumpControlHierarchy(mWindow, &dumpFile);
				}
			}
		}
	}
#else

#if 0

	class NewStreamMessage : public NativeMessage {
		nsIPluginInstancePeer* mPeer;
		const char* mType;
	public:
		NewStreamMessage(nsIPluginInstancePeer* peer, const char* type) : mPeer(peer), mType(type) {}
		
		virtual void execute() {
			nsIOutputStream* output = NULL;
			if (mPeer->NewStream(mType, "_blank", &output) == NS_OK) {
				// write some data to the stream.	
				output->Close();
				NS_RELEASE(output);
			}
		}
	};

	// open a stream of type "application/x-java-frame", which should cause a full-screen plugin to get created for a Java frame's
	// behalf. communicate with the other instance via this stream.
	nsIPluginInstancePeer* peer = NULL;
	if (pluginInstance->GetPeer(&peer) == NS_OK) {
		NewStreamMessage msg(peer, "application/x-java-frame");
		pluginInstance->getSession()->sendMessage(&msg);
		NS_RELEASE(peer);
	}

#else

	// Use JavaScript to create a window with an <EMBED TYPE="application/x-java-frame"> tag.
	const char* kEmbeddedFrameScript = "var w = window.open('','_new','resizable=no,status=no,width=200,height=200');"
	                                   "var d = w.document; d.write('"
	                                   // "<BODY MARGINWIDTH=0 MARGINHEIGHT=0>"	// this doesn't work, don't know why
	                                   "<HTML><BODY>"
	                                   "<EMBED TYPE=\"application/x-java-frame\""
	                                   "WIDTH=200 HEIGHT=200 FRAME=XXXXXXXX></EMBED>"
	                                   "</BODY></HTML>'); d.close();";
	
	char* script = strdup(kEmbeddedFrameScript);
	char* address = strchr(script, 'X');
	sprintf(address, "%08X", this);
	address[8] = '>';

	JSEvaluator* evaluator = new JSEvaluator(pluginInstance);
	evaluator->AddRef();
	
	// create the window. It will be created after returning from eval.
	const char* result = evaluator->eval(script);
	
	delete[] script;

#endif

#endif

	if (mWindow != NULL) {
		Point zeroPt = { 5, 5 };
		::JMSetFrameVisibility(mFrameRef, mWindow, zeroPt, NULL);
	}
}

EmbeddedFrame::~EmbeddedFrame()
{
	if (mPluginInstance != NULL)
		mPluginInstance->setFrame(NULL);

	// make sure the window is hidden (and unregistered with the browser).
	showHide(false);

	// make sure this port isn't ever current again.
	::UnsetPort(mWindow);

	// if (mWindow != NULL)
	//	::DisposeWindow(mWindow);
}

void EmbeddedFrame::setSize(const Rect* newSize)
{
	mBounds = *newSize;

	if (mWindow != NULL) {
		SInt16 width = newSize->right - newSize->left;
		SInt16 height = newSize->bottom - newSize->top;
		::SizeWindow(mWindow, width, height, true);
		::MoveWindow(mWindow, newSize->left, newSize->top, false);
	}
}

void EmbeddedFrame::invalRect(const Rect* invalidRect)
{
	if (mWindow != NULL) {
		::InvalRect(invalidRect);
	}
}

void EmbeddedFrame::showHide(Boolean visible)
{
	if (mWindow != NULL && visible != IsWindowVisible(mWindow)) {
		if (visible) {
			// Have to notify the browser that this window exists, so that it will receive events.
			::ShowWindow(mWindow);
			::SelectWindow(mWindow);
		} else {
			::HideWindow(mWindow);
		}
		
		// ::ShowHide(mWindow, visible);
	}
}

void EmbeddedFrame::setTitle(const StringPtr title)
{
	if (mWindow != NULL) {
		::SetWTitle(mWindow, title);
	}
}

void EmbeddedFrame::checkUpdate()
{
}

void EmbeddedFrame::reorder(ReorderRequest request)
{
	switch (request) {
	case eBringToFront:		/* bring the window to front */
		break;
	case eSendToBack:		/* send the window to back */
		break;
	case eSendBehindFront:	/* send the window behind the front window */
		break;
	}
}

void EmbeddedFrame::setResizeable(Boolean resizeable)
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

void EmbeddedFrame::activate(Boolean active)
{
	focusEvent(active);
	MRJFrame::activate(active);
}

void EmbeddedFrame::click(const EventRecord* event)
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
		Rect bounds = (**GetGrayRgn()).rgnBBox;
		DragWindow(mWindow, where, &bounds);
		computeBounds(mWindow, &mBounds);
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

void EmbeddedFrame::setPluginInstance(EmbeddedFramePluginInstance* embeddedInstance)
{
	mPluginInstance = embeddedInstance;
}

void EmbeddedFrame::setWindow(WindowRef window)
{
	mWindow = window;
}

WindowRef EmbeddedFrame::getWindow()
{
	return mWindow;
}

GrafPtr EmbeddedFrame::getPort()
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
