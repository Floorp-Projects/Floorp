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
	
	// Note:  opening a new window on a stream using the following code crashes 4.X browsers.
	// The problem is that the window has no URL assigned to it, and so some variables are
	// unitialized and the browser crashes in a call to strlen().

	class NewStreamMessage : public NativeMessage {
		nsIPluginInstancePeer* mPeer;
		const char* mType;
	public:
		NewStreamMessage(nsIPluginInstancePeer* peer, const char* type) : mPeer(peer), mType(type) {}
		
		virtual void execute() {
			nsIOutputStream* output = NULL;
			if (mPeer->NewStream(mType, "_new", &output) == NS_OK) {
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
	// var w = window.open('', '_new','resizable=no,status=no,width=200,height=200'); d = w.document; d.open(); d.write('<BODY MARGINHEIGHT=0 MARGINWIDTH=0>Hi</BODY>'); d.close();

	static UInt32 embeddedFrameCounter = 0;

	// Use JavaScript to create a window with an <EMBED TYPE="application/x-java-frame"> tag.
	const char* kEmbeddedFrameScript = "var w = window.open('','__MRJ_JAVA_FRAME_%d__','resizable=no,status=no,width=%d,height=%d,screenX=%d,screenY=%d');"
	                                   "var d = w.document; d.open();"
	                                   "d.writeln('<BODY BGCOLOR=#FFFFFF MARGINWIDTH=0 MARGINHEIGHT=0>&nbsp;<EMBED TYPE=application/x-java-frame WIDTH=%d HEIGHT=%d JAVAFRAME=%08X>');"
	                                   "d.close();";

	int width = mBounds.right - mBounds.left;
	int height = mBounds.bottom - mBounds.top;
	int screenX = mBounds.left;
	int screenY = mBounds.top;

	char* script = new char[::strlen(kEmbeddedFrameScript) + 100];
	::sprintf(script, kEmbeddedFrameScript, ++embeddedFrameCounter, width, height, screenX, screenY, width, height, this);

	JSEvaluator* evaluator = new JSEvaluator(pluginInstance);
	evaluator->AddRef();
	
	// create the window. It will have been created after returning from eval.
	const char* result = evaluator->eval(script);
	
	evaluator->Release();
	
	delete[] script;

#endif

	if (mWindow != NULL) {
		Point zeroPt = { 0, 0 };
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
