/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */

#include "nsMemory.h"

#include "nsAESpyglassSuiteHandler.h"
#include "nsCommandLineServiceMac.h"
#include "nsDocLoadObserver.h"

/*----------------------------------------------------------------------------
	AESpyglassSuiteHandler 
	
----------------------------------------------------------------------------*/
AESpyglassSuiteHandler::AESpyglassSuiteHandler()
:   mDocObserver(nsnull)
{
}

/*----------------------------------------------------------------------------
	~AESpyglassSuiteHandler 
	
----------------------------------------------------------------------------*/
AESpyglassSuiteHandler::~AESpyglassSuiteHandler()
{
    NS_IF_RELEASE(mDocObserver);
}


/*----------------------------------------------------------------------------
	HandleSpyglassSuiteEvent 
	
----------------------------------------------------------------------------*/
void AESpyglassSuiteHandler::HandleSpyglassSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr		err = noErr;
	
	AEEventID		eventID;
	OSType		typeCode;
	Size			actualSize 	= 0L;
	
	// Get the event ID
	err = AEGetAttributePtr(appleEvent, 	keyEventIDAttr, 
									typeType, 
									&typeCode, 
									(Ptr)&eventID, 
									sizeof(eventID), 
									&actualSize);
	ThrowIfOSErr(err);
	
	try
	{
		switch (eventID)
		{
			case kOpenURLEvent:
				HandleOpenURLEvent(appleEvent, reply);
				break;
			
			case kRegisterURLEchoEvent:
				HandleRegisterURLEchoEvent(appleEvent, reply);
				break;
				
			case kUnregisterURLEchoEvent:
				HandleUnregisterURLEchoEvent(appleEvent, reply);
				break;
			
			default:
				ThrowOSErr(errAEEventNotHandled);
				break;
		}
	}
	catch (OSErr catchErr)
	{
		PutReplyErrorNumber(reply, catchErr);
		throw;
	}
	catch ( ... )
	{
		PutReplyErrorNumber(reply, paramErr);
		throw;
	}
}


/*----------------------------------------------------------------------------
	HandleOpenURLEvent 
	
----------------------------------------------------------------------------*/
void AESpyglassSuiteHandler::HandleOpenURLEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc		directParameter;
	FSSpec		saveToFile;
	Boolean		gotSaveToFile = false;
	SInt32		targetWindowID = -1;
	SInt32		openFlags = 0;
	OSErr		err;
	
	// extract the direct parameter (an object specifier)
	err = ::AEGetKeyDesc(appleEvent, keyDirectObject, typeWildCard, &directParameter);
	ThrowIfOSErr(err);

	// look for the save to file param
	StAEDesc		targetFileDesc;
	err = ::AEGetKeyDesc(appleEvent, kParamSaveToFileDest, typeFSS, &targetFileDesc);
	if (err != errAEDescNotFound)
	{
		targetFileDesc.GetFileSpec(saveToFile);
		gotSaveToFile = true;
	}
	
#if 0
	// look for the target window param
	StAEDesc		openInWindowDesc;
	err = ::AEGetKeyDesc(appleEvent, kParamOpenInWindow, typeLongInteger, &openInWindowDesc);
	if (err != errAEDescNotFound)
		targetWindowID = openInWindowDesc.GetLong();

	// look for the open flags
	StAEDesc		openFlagsDesc;
	err = ::AEGetKeyDesc(appleEvent, kParamOpenFlags, typeLongInteger, &openFlagsDesc);
	if (err != errAEDescNotFound)
		openFlags = openFlagsDesc.GetLong();

        // do something with targetWindowID and openFlags...
#endif
	
	long		dataSize = directParameter.GetDataSize();
	char*	urlString = (char *)nsMemory::Alloc(dataSize + 1);
	ThrowIfNil(urlString);
	
	directParameter.GetCString(urlString, dataSize + 1);
	
	nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
	cmdLine.DispatchURLToNewBrowser(urlString);
	
	nsMemory::Free(urlString);	
}



/*----------------------------------------------------------------------------
	HandleRegisterURLEchoEvent 
	
----------------------------------------------------------------------------*/
void AESpyglassSuiteHandler::HandleRegisterURLEchoEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	// extract the direct parameter (the requester's signature)
	StAEDesc directParameter;
	OSErr err = ::AEGetKeyDesc(appleEvent, keyDirectObject, typeType, &directParameter);
	ThrowIfOSErr(err);

	if (typeType == directParameter.descriptorType)
	{
	    if (mDocObserver == nsnull) {
    		mDocObserver = new nsDocLoadObserver;
    		ThrowIfNil(mDocObserver);
    		NS_ADDREF(mDocObserver);        // our owning ref
    	}
    	OSType requester;
    	if (AEGetDescData(&directParameter, &requester, sizeof(requester)) == noErr)
    		mDocObserver->AddEchoRequester(requester);
	}
}

/*----------------------------------------------------------------------------
	HandleUnregisterURLEchoEvent 
	
----------------------------------------------------------------------------*/
void AESpyglassSuiteHandler::HandleUnregisterURLEchoEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	// extract the direct parameter (the requester's signature)
	StAEDesc directParameter;
	OSErr err = ::AEGetKeyDesc(appleEvent, keyDirectObject, typeType, &directParameter);
	ThrowIfOSErr(err);

	if (typeType == directParameter.descriptorType)
	{
	    if (mDocObserver)
		mDocObserver->RemoveEchoRequester(**(OSType**)directParameter.dataHandle);
	}
}
