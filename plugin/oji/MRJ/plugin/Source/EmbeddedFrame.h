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
	EmbeddedFrame.h
	
	An MRJFrame sub-class that manages the behavior of a top-level window
	running inside the Communicator.
	
	by Patrick C. Beard.
 */

#pragma once

#include "MRJFrame.h"

#ifndef __MACWINDOWS__
#include <MacWindows.h>
#endif

class MRJPluginInstance;
class EmbeddedFramePluginInstance;
class JSEvaluator;

class EmbeddedFrame : public MRJFrame {
public:
	EmbeddedFrame(MRJPluginInstance* pluginInstance, JMFrameRef frameRef, JMFrameKind kind, const Rect* initialBounds, Boolean resizeable);
	virtual ~EmbeddedFrame();

	virtual void setSize(const Rect* newSize);
	virtual void invalRect(const Rect* invalidRect);
	virtual void showHide(Boolean visible);
	virtual void setTitle(const StringPtr title);
	virtual void checkUpdate();
	virtual void reorder(ReorderRequest request);
	virtual void setResizeable(Boolean resizeable);
	
	virtual void activate(Boolean active);
	virtual void click(const EventRecord* event);
	
	void setPluginInstance(EmbeddedFramePluginInstance* embeddedInstance);
	
	void setWindow(WindowRef window);
	WindowRef getWindow();

protected:
	virtual GrafPtr getPort();

private:
	EmbeddedFramePluginInstance* mPluginInstance;
	JSEvaluator* mEvaluator;
	WindowRef mWindow;
	Rect mBounds;
};
