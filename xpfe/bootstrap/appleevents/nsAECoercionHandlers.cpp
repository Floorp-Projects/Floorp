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



#include "nsAEUtils.h"

#include "nsAECoercionHandlers.h"



AECoercionHandlers*	AECoercionHandlers::sAECoercionHandlers = nil;


/*----------------------------------------------------------------------------
	AECoercionHandlers 
	
----------------------------------------------------------------------------*/

AECoercionHandlers::AECoercionHandlers()
:	mTextDescToPascalString(nil)
,	mPascalStringDescToText(nil)
{
	OSErr	err;
	
	mTextDescToPascalString = NewAECoerceDescProc(TextToPascalStringCoercion);
	ThrowIfNil(mTextDescToPascalString);

	err = ::AEInstallCoercionHandler(typeChar, typePascalString,
								mTextDescToPascalString,
								(long)this,
								true,			/* Pass a pointer not a descriptor */
								false );		/* Application table, not System */
	ThrowIfOSErr(err);

	mPascalStringDescToText = NewAECoerceDescProc(PascalStringToTextCoercion);
	ThrowIfNil(mPascalStringDescToText);

	err = ::AEInstallCoercionHandler(typePascalString, typeChar,
								mPascalStringDescToText,
								(long)this,
								true,			/* Pass a pointer not a descriptor */
								false );		/* Application table, not System */
	ThrowIfOSErr(err);
}



/*----------------------------------------------------------------------------
	~AECoercionHandlers 
	
----------------------------------------------------------------------------*/

AECoercionHandlers::~AECoercionHandlers()
{
	if (mTextDescToPascalString)
	{
		AERemoveCoercionHandler(typeChar, typePascalString, mTextDescToPascalString, false);
		DisposeRoutineDescriptor(mTextDescToPascalString);
	}

	if (mPascalStringDescToText)
	{
		AERemoveCoercionHandler(typePascalString, typeChar, mPascalStringDescToText, false);
		DisposeRoutineDescriptor(mPascalStringDescToText);
	}
}


#pragma mark -

/*----------------------------------------------------------------------------
	TextToPascalStringCoercion 
	
----------------------------------------------------------------------------*/
OSErr AECoercionHandlers::TextToPascalStringCoercion(const AEDesc *fromDesc, DescType toType, long handlerRefcon, AEDesc *toDesc)
{
	OSErr	err = noErr;
	
	if (toType != typePascalString)
		return errAECoercionFail;

	switch (fromDesc->descriptorType)
	{
		case typeChar:
			Str255		pString;
			DescToPString(fromDesc, pString, 255);
			err = AECreateDesc(typePascalString, pString, pString[0] + 1, toDesc);
			break;
			
		default:
			err = errAECoercionFail;
			break;
	}

	return err;
}

/*----------------------------------------------------------------------------
	PascalStringToTextCoercion 
	
----------------------------------------------------------------------------*/

OSErr AECoercionHandlers::PascalStringToTextCoercion(const AEDesc *fromDesc, DescType toType, long handlerRefcon, AEDesc *toDesc)
{
	OSErr	err = noErr;
	
	if (toType != typeChar)
		return errAECoercionFail;

	switch (fromDesc->descriptorType)
	{
		case typePascalString:
			{
				Handle	dataHandle = fromDesc->dataHandle;
				long		stringLen = *(unsigned char *)(*dataHandle);
				if (stringLen > 255)
				{
					err = errAECoercionFail;
					break;
				}
				
				StHandleLocker		locker(dataHandle);
				err = AECreateDesc(typeChar, *dataHandle + 1, stringLen, toDesc);
			}
			break;
			
		default:
			err = errAECoercionFail;
			break;
	}

	return err;
}



#pragma mark -

/*----------------------------------------------------------------------------
	CreateCoercionHandlers 
	
----------------------------------------------------------------------------*/

OSErr CreateCoercionHandlers()
{
	OSErr	err = noErr;
	
	if (AECoercionHandlers::sAECoercionHandlers)
		return noErr;
	
	try
	{
		AECoercionHandlers::sAECoercionHandlers = new AECoercionHandlers;
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
	ShutdownCoercionHandlers 
	
----------------------------------------------------------------------------*/

OSErr ShutdownCoercionHandlers()
{
	if (!AECoercionHandlers::sAECoercionHandlers)
		return noErr;
	
	try
	{
		delete AECoercionHandlers::sAECoercionHandlers;
	}
	catch(...)
	{
	}
	
	AECoercionHandlers::sAECoercionHandlers = nil;
	return noErr;
}

