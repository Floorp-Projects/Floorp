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


#include "nsAEEventHandling.h"

#include "nsAECoreClass.h"

/*----------------------------------------------------------------------------
	CreateAEHandlerClasses 
	
	This call creates everything.
----------------------------------------------------------------------------*/

OSErr CreateAEHandlerClasses(Boolean suspendFirstEvent)
{
	OSErr	err = noErr;
	
	// if we have one, assume we have all
	if (AECoreClass::sAECoreHandler)
		return noErr;
	
	try
	{
		AECoreClass::sAECoreHandler = new AECoreClass(suspendFirstEvent);
	}
	catch(OSErr catchErr)
	{
		err = catchErr;
	}
	catch( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	CreateAEHandlerClasses 
	
	This call creates everything.
----------------------------------------------------------------------------*/

OSErr ResumeAEHandling(AppleEvent *appleEvent, AppleEvent *reply, Boolean dispatchEvent)
{
	OSErr	err = noErr;
	
	if (!AECoreClass::sAECoreHandler)
		return paramErr;
	
	try
	{
		AECoreClass::sAECoreHandler->ResumeEventHandling(appleEvent, reply, dispatchEvent);
	}
	catch(OSErr catchErr)
	{
		err = catchErr;
	}
	catch( ... )
	{
		err = paramErr;
	}

	return err;
}


/*----------------------------------------------------------------------------
	GetSuspendedEvent 
	
	Return the suspended event, if any
----------------------------------------------------------------------------*/
OSErr GetSuspendedEvent(AppleEvent *theEvent, AppleEvent *reply)
{
	OSErr	err = noErr;
	
	theEvent->descriptorType = typeNull;
	theEvent->dataHandle = nil;
	
	reply->descriptorType = typeNull;
	reply->dataHandle = nil;
	
	if (!AECoreClass::sAECoreHandler)
		return paramErr;
	
	try
	{
		AECoreClass::sAECoreHandler->GetSuspendedEvent(theEvent, reply);
	}
	catch(OSErr catchErr)
	{
		err = catchErr;
	}
	catch( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	ShutdownAEHandlerClasses 
	
	and this destroys the whole lot.
----------------------------------------------------------------------------*/
OSErr ShutdownAEHandlerClasses(void)
{
	if (!AECoreClass::sAECoreHandler)
		return noErr;
	
	try
	{
		delete AECoreClass::sAECoreHandler;
	}
	catch(...)
	{
	}
	
	AECoreClass::sAECoreHandler = nil;
	return noErr;
}

