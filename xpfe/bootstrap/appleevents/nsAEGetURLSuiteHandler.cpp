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

#include "nsAEGetURLSuiteHandler.h"
#include "nsCommandLineServiceMac.h"


/*----------------------------------------------------------------------------
	AEGetURLSuiteHandler 
	
----------------------------------------------------------------------------*/
AEGetURLSuiteHandler::AEGetURLSuiteHandler()
{
}

/*----------------------------------------------------------------------------
	~AEGetURLSuiteHandler 
	
----------------------------------------------------------------------------*/
AEGetURLSuiteHandler::~AEGetURLSuiteHandler()
{
}


/*----------------------------------------------------------------------------
	HandleGetURLSuiteEvent 
	
----------------------------------------------------------------------------*/
void AEGetURLSuiteHandler::HandleGetURLSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
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
			case kGetURLEvent:
				HandleGetURLEvent(appleEvent, reply);
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
	HandleGetURLEvent 
	
----------------------------------------------------------------------------*/
void AEGetURLSuiteHandler::HandleGetURLEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc		directParameter;
	OSErr		err;
	
	// extract the direct parameter (an object specifier)
	err = ::AEGetKeyDesc(appleEvent, keyDirectObject, typeWildCard, &directParameter);
	ThrowIfOSErr(err);

	// we need to look for other parameters, to do with destination etc.

	long		dataSize = directParameter.GetDataSize();
	char*	urlString = (char *)nsMemory::Alloc(dataSize + 1);
	ThrowIfNil(urlString);
	
	directParameter.GetCString(urlString, dataSize + 1);
	
	nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
	cmdLine.DispatchURLToNewBrowser(urlString);
	
	nsMemory::Free(urlString);	
}

