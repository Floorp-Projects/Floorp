/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	MRJContext.h

	Manages Java content using the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#pragma once

#include "jni.h"
#include "JManager.h"
#include "nsIPluginTagInfo2.h"

//
// Instance state information about the plugin.
//
// *Developers*: Use this struct to hold per-instance
//				 information that you'll need in the
//				 various functions in this file.
//

class MRJSession;
class MRJPluginInstance;
class nsIPluginInstancePeer;
struct nsPluginWindow;
class MRJFrame;
class MRJPage;
struct MRJPageAttributes;

struct nsPluginPoint {
	PRInt32             x;
    PRInt32             y;
};

class MRJContext {
public:
	MRJContext(MRJSession* session, MRJPluginInstance* instance);
	~MRJContext();

	void processAppletTag();
	Boolean createContext();
	JMAWTContextRef getContextRef();
    JMAppletViewerRef getViewerRef();
	
	void setProxyInfoForURL(char * url, JMProxyType proxyType);
	Boolean appletLoaded();
	Boolean loadApplet();
	Boolean isActive();
	
	void suspendApplet();
	void resumeApplet();
	
	jobject getApplet();
	
	void idle(short modifiers);
	void drawApplet();
	void printApplet(nsPluginWindow* printingWindow);
	
	void activate(Boolean active);
	void resume(Boolean inFront);

	void click(const EventRecord* event, MRJFrame* frame);
	void keyPress(long message, short modifiers);
	void keyRelease(long message, short modifiers);
	
	void setWindow(nsPluginWindow* pluginWindow);
	Boolean inspectWindow();
	void setClipping();
	
	MRJFrame* findFrame(WindowRef window);
	GrafPtr getPort();
	
	void showFrames();
	void hideFrames();
	void releaseFrames();
	
	void setDocumentBase(const char* documentBase);
	const char* getDocumentBase();
	
	void setAppletHTML(const char* appletHTML, nsPluginTagType tagType);
	const char* getAppletHTML();

private:
	void localToFrame(Point* pt);
	void ensureValidPort();
	void setVisibility();

	static OSStatus requestFrame(JMAWTContextRef context, JMFrameRef newFrame, JMFrameKind kind,
								const Rect *initialBounds, Boolean resizeable, JMFrameCallbacks *callbacks);
	static OSStatus releaseFrame(JMAWTContextRef context, JMFrameRef oldFrame);
	static SInt16 getUniqueMenuID(JMAWTContextRef context, Boolean isSubmenu);
	static void exceptionOccurred(JMAWTContextRef context, JMTextRef exceptionName, JMTextRef exceptionMsg, JMTextRef stackTrace);

	static void showDocument(JMAppletViewerRef viewer, JMTextRef urlString, JMTextRef windowName);
	static void setStatusMessage(JMAppletViewerRef viewer, JMTextRef statusMsg);
	
	void showURL(const char* url, const char* target);
	void showStatus(const char* message);
	SInt16 allocateMenuID(Boolean isSubmenu);

	OSStatus createFrame(JMFrameRef frameRef, JMFrameKind kind, const Rect* initialBounds, Boolean resizeable);

	// Finds a suitable MRJPage object for this document URL, or creates one.
	MRJPage* findPage(const MRJPageAttributes& attributes);

	static CGrafPtr getEmptyPort();

private:
	MRJPluginInstance*		mPluginInstance;
	MRJSession*				mSession;
	JMSessionRef 			mSessionRef;
	nsIPluginInstancePeer* 	mPeer;
	JMAppletLocatorRef		mLocator;
	JMAWTContextRef			mContext;
	JMAppletViewerRef		mViewer;
	JMFrameRef				mViewerFrame;
	Boolean					mIsActive;
	nsPluginPoint           mCachedOrigin;
	nsPluginRect            mCachedClipRect;
	RgnHandle               mCachedClipping;
	RgnHandle				mPluginClipping;
	nsPluginWindow*			mPluginWindow;
	CGrafPtr				mPluginPort;
	char*					mDocumentBase;
	char*					mAppletHTML;
	MRJPage*				mPage;
};
