/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */
 
 
#ifndef _NS_ASEAPP_H_
	#include "nsASEApp.h"
#endif

#include <AppleEvents.h>
#include <Balloons.h>
#include <MacTypes.h>

#include "nsEventHandler.h"
#include "nsAppleSingleEncoder.h"
#include "nsAppleSingleDecoder.h"
#include "MoreFilesExtras.h"

Boolean gDone;

nsASEApp::nsASEApp()
{
	InitManagers();
	InitAEHandlers();
	mWindow = NULL;
	SetCompletionStatus(false);
	
	OSErr err = NavLoad();
	if (err!= noErr)
		FatalError(navLoadErr);
		
	MakeMenus();
}

nsASEApp::~nsASEApp()
{
	NavUnload();
}

void
nsASEApp::InitManagers(void)
{
	MaxApplZone();
	MoreMasters(); MoreMasters(); MoreMasters();
	
	InitGraf(&qd.thePort);
	InitFonts();			
	InitWindows();
	InitMenus();
	TEInit();		
	InitDialogs(NULL);
	
	InitCursor();
	FlushEvents(everyEvent, 0);	
}

#pragma mark -
#pragma mark *** Apple Event Handlers ***
#pragma mark -

pascal OSErr
EncodeEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon)
{
	OSErr	err = noErr;
	FSSpec	param;
	Boolean	result = false, isDir = false;
	AEDesc	fileDesc;
	long	dummy;
	
	// extract FSSpec from params
	err = AEGetParamDesc(appEvent, keyDirectObject, typeFSS, &fileDesc);
	if (err != noErr)
		goto reply;
	BlockMoveData(*fileDesc.dataHandle, &param, sizeof(FSSpec));
	
	// param check
	err = nsASEApp::GotRequiredParams(appEvent);
	if (err != noErr)
		goto reply;
		
	FSpGetDirectoryID(&param, &dummy, &isDir);
	
	// if folder recursively traverse and encode contents
	if (isDir)
	{
		nsAppleSingleEncoder encoder;
		err = encoder.EncodeFolder(&param);
	}
	else
	{
		// it's a file not a folder so proceed as usual
		
		// check if given file has res fork (takes care of existence check)
		if (nsAppleSingleEncoder::HasResourceFork(&param))
		{
			// encode given file
			nsAppleSingleEncoder encoder;
			err = encoder.Encode(&param);
		}
	}
	
	// if noErr thus far 
	if (err == noErr)
	{
		// then set result to true
		result = true;
	}
	
reply:
	// package reply
	AEPutParamPtr(reply, keyDirectObject, typeBoolean, &result, sizeof(result));
	
	// boolean takes care of failures
	return noErr;
}

pascal OSErr
DecodeEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon)
{
	OSErr	err = noErr;
	FSSpec	param, outFile;
	Boolean	result = false, isDir = false;
	AEDesc	fileDesc;
	long	dummy;
	
	// extract FSSpec from params
	err = AEGetParamDesc(appEvent, keyDirectObject, typeFSS, &fileDesc);
	if (err != noErr)
		goto reply;
	BlockMoveData(*fileDesc.dataHandle, &param, sizeof(FSSpec));
	
	// param check
	err = nsASEApp::GotRequiredParams(appEvent);
	if (err != noErr)
		goto reply;
			
	FSpGetDirectoryID(&param, &dummy, &isDir);
	
	// if folder recursively traverse and encode contents
	if (isDir)
	{
		nsAppleSingleDecoder decoder;
		err = decoder.DecodeFolder(&param);
	}
	else
	{	
		// it's a file not a folder so proceed as usual
		
		// check if given file is in AS format (takes care of existence check)
		if (nsAppleSingleDecoder::IsAppleSingleFile(&param))
		{
			// decode given file
			nsAppleSingleDecoder decoder;
			err = decoder.Decode(&param, &outFile);
		}
	}
	
	// if noErr thus far 
	if (err == noErr)
	{
		// then set result to true
		result = true;
	}
	
reply:
	// package reply
	AEPutParamPtr(reply, keyDirectObject, typeBoolean, &result, sizeof(result));
	
	// boolean takes care of failures
	return noErr;
}

pascal OSErr
QuitEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon)
{
	OSErr	err = noErr;
	
	nsASEApp::SetCompletionStatus(true);
	
	return err;
}

#pragma mark -

void
nsASEApp::InitAEHandlers()
{
	OSErr 					err = noErr;
	
	mEncodeUPP = NewAEEventHandlerProc((ProcPtr) EncodeEvent);
	err = AEInstallEventHandler(kASEncoderEventClass, kAEEncode,
								mEncodeUPP, 0L, false);
	if (err != noErr)
		::CautionAlert(aeInitErr, nil);
	
	mDecodeUPP = NewAEEventHandlerProc((ProcPtr) DecodeEvent);
	err = AEInstallEventHandler(kASEncoderEventClass, kAEDecode,
								mDecodeUPP, 0L, false);
	if (err != noErr)
		::CautionAlert(aeInitErr, nil);
		
	mQuitUPP = NewAEEventHandlerProc((ProcPtr) QuitEvent);
	err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
								mQuitUPP, 0L, false);
	if (err != noErr)
		::CautionAlert(aeInitErr, nil);
}
OSErr
nsASEApp::GotRequiredParams(AppleEvent *appEvent)
{
	OSErr		err = noErr;
	DescType	returnedType;
	Size		actualSize;

	err = AEGetAttributePtr(appEvent, keyMissedKeywordAttr, typeWildCard,
							&returnedType, NULL, 0, &actualSize);

	if (err == errAEDescNotFound)
		err = noErr;
	else if (err == noErr)
		err = errAEParamMissed;
		
	return err;
}

void
nsASEApp::MakeMenus()
{
    Handle 		mbarHdl;
	MenuHandle	menuHdl;

	mbarHdl = ::GetNewMBar(rMenuBar);
	::SetMenuBar(mbarHdl);
	
	if ((menuHdl = ::GetMenuHandle(rMenuApple))!=nil) 
	{
		::AppendResMenu(menuHdl, 'DRVR');
	}
		
	if ((menuHdl = GetMenuHandle(rMenuEdit))!=nil)
		::DisableItem(menuHdl, 0);
	
	::HMGetHelpMenuHandle(&menuHdl);
	::DisableItem(menuHdl, 0);

	::DrawMenuBar();
}

void
nsASEApp::SetCompletionStatus(Boolean aVal)
{
	gDone = aVal;
}

Boolean 
nsASEApp::GetCompletionStatus()
{
	return gDone;
}



void
nsASEApp::FatalError(short aErrID)
{
	::StopAlert(aErrID, nil);
	SetCompletionStatus(true);
}

OSErr
nsASEApp::Run()
{
	OSErr		err = noErr;
	EventRecord evt;
	nsEventHandler handler;
	
	while (!gDone)
	{
		if (::WaitNextEvent(everyEvent, &evt, 180, NULL))
		{
			handler.HandleNextEvent(&evt);
		}
	}
		
	return err;
}

int
main(void)
{
	nsASEApp app;
	
	app.Run();
	
	return 0;
}

