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
	TopLevelFrame.h
	
	An MRJFrame sub-class that manages the behavior of a top-level window
	running inside the Communicator.
	
	by Patrick C. Beard.
 */

#pragma once

#include "MRJFrame.h"

#ifndef __MACWINDOWS__
#include <MacWindows.h>
#endif

class nsIEventHandler;

class TopLevelFrame : public MRJFrame {
public:
	TopLevelFrame(nsIEventHandler* handler, JMFrameRef frameRef, JMFrameKind kind, const Rect* initialBounds, Boolean resizeable);
	virtual ~TopLevelFrame();

	virtual void setSize(const Rect* newSize);
	virtual void invalRect(const Rect* invalidRect);
	virtual void showHide(Boolean visible);
	virtual void setTitle(const StringPtr title);
	virtual void checkUpdate();
	virtual void reorder(ReorderRequest request);
	virtual void setResizeable(Boolean resizeable);
	
	virtual void activate(Boolean active);
	virtual void click(const EventRecord* event);
	
	WindowRef getWindow();

protected:
	virtual GrafPtr getPort();

private:
	nsIEventHandler* mHandler;
	WindowRef mWindow;
	Rect mBounds;
};
