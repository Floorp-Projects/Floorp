/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

private:
	static OSErr PositionToOffsetHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, UInt32 handlerRefcon);
	static OSErr OffsetToPositionHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, UInt32 handlerRefcon);
	static OSErr UpdateHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, UInt32 handlerRefcon);
	static AEEventHandlerUPP mPos2OffsetUPP;
	static AEEventHandlerUPP mOffset2PosUPP;
	static AEEventHandlerUPP mUpdateUPP;

};


#endif // NSMACTSMMESSAGEPUMP_h__