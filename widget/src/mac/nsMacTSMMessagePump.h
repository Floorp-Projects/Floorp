/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

// 
// nsMacTSMMessageHandler
//
// This file contains the default implementation for the mac TSM handlers loop. 
//
// Clients may either use this implementation or write their own. Embedding applications
// will almost certainly write their own because they will want control of the event
// loop to do other processing.

#ifndef NSMACTSMMESSAGEPUMP_h__
#define NSMACTSMMESSAGEPUMP_h__

#include <AppleEvents.h>
#include <TextServices.h>

class nsMacTSMMessagePump {

public:
	nsMacTSMMessagePump();
	~nsMacTSMMessagePump();

	static nsMacTSMMessagePump* GetSingleton();
	void Shutdown();
	
private:
	static pascal OSErr PositionToOffsetHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon);
	static pascal OSErr OffsetToPositionHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon);
	static pascal OSErr UpdateHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon);
	static AEEventHandlerUPP mPos2OffsetUPP;
	static AEEventHandlerUPP mOffset2PosUPP;
	static AEEventHandlerUPP mUpdateUPP;
	
	static nsMacTSMMessagePump* gSingleton;

};


#endif // NSMACTSMMESSAGEPUMP_h__