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
	MRJFrame.h
	
	Encapsulates a JManager frame.
	
	by Patrick C. Beard.
 */

#pragma once

#include "jni.h"
#include "JManager.h"

struct EventRecord;
struct nsPluginPrint;

class MRJFrame {
public:
	MRJFrame(JMFrameRef frameRef);
	virtual ~MRJFrame();
	
	/** Methods used to implement the JMFrame callback protocol. */
	virtual void setSize(const Rect* newSize);
	virtual void invalRect(const Rect* invalidRect);
	virtual void showHide(Boolean visible);
	virtual void setTitle(const StringPtr title);
	virtual void checkUpdate();
	virtual void reorder(ReorderRequest request);
	virtual void setResizeable(Boolean resizeable);
	
	/** Methods to handle various events. */
	virtual Boolean handleEvent(const EventRecord* event);
	
	virtual void idle(SInt16 modifiers);
	virtual void update();
	virtual void activate(Boolean active);
	virtual void resume(Boolean inFront);
	virtual void click(const EventRecord* event);
	virtual void click(const EventRecord* event, Point localWhere);
	virtual void keyPress(UInt32 message, SInt16 modifiers);
	virtual void keyRelease(UInt32 message, SInt16 modifiers);

	virtual void focusEvent(Boolean gotFocus);
	virtual void menuSelected(UInt32 message, SInt16 modifiers);

	virtual void print(GrafPtr printingPort, Point frameOrigin);

protected:
	virtual GrafPtr getPort() = 0;

protected:
	JMFrameRef mFrameRef;
	Boolean mActive;
	Boolean mFocused;
};
