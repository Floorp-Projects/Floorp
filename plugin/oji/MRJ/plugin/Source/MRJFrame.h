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
	MRJFrame.h
	
	Encapsulates a JManager frame.
	
	by Patrick C. Beard.
 */

#pragma once

#ifndef CALL_NOT_IN_CARBON
	#define CALL_NOT_IN_CARBON 1
#endif

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
