/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
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


#include <Sound.h>
#include <Scrap.h>

#include "nsAEUtils.h"
#include "nsAETokens.h"
#include "nsAECoreClass.h"
#include "nsAEDocumentClass.h"
#include "nsAEWindowClass.h"

#include "nsAEApplicationClass.h"

#include "nsCommandLineServiceMac.h"

/*----------------------------------------------------------------------------
	AEApplicationClass 
	
----------------------------------------------------------------------------*/
AEApplicationClass::AEApplicationClass()
:	AEGenericClass(cApplication, typeNull)
{
}

/*----------------------------------------------------------------------------
	~AEApplicationClass 
	
----------------------------------------------------------------------------*/
AEApplicationClass::~AEApplicationClass()
{
}

#pragma mark -



/*----------------------------------------------------------------------------
	GetPropertyFromApp 
	
	Override default to customize behaviour.
----------------------------------------------------------------------------*/
void	AEApplicationClass::GetProperty(			DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass,
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	OSErr			err;
	CoreTokenRecord 	token;
	DescType			requestedProperty = **(DescType**)keyData->dataHandle;
	
	token.dispatchClass	= GetClass();
	token.objectClass    	= GetClass();
	token.propertyCode 	= requestedProperty;

	if (CanGetProperty(requestedProperty) || CanSetProperty(requestedProperty))
	{
		err = AECreateDesc(cProperty, (Ptr)&token, sizeof(CoreTokenRecord), resultToken);
		ThrowIfOSErr(err);
	}
	else
	{
		ThrowIfOSErr(errAEEventNotHandled);
	}
}



/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
	Not appropriate for the application
----------------------------------------------------------------------------*/
void AEApplicationClass::GetItemFromContainer(	DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass, 
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	ThrowIfOSErr(errAEEventNotHandled);
}


#pragma mark -

/*----------------------------------------------------------------------------
	HandleClose 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleClose(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr 	err 	= noErr;

	StAEDesc	saving;
	StAEDesc	savingIn;
		
	// Extract the [saving yes/no/ask] optional parameter, if present
	err = AEGetParamDesc(appleEvent, 
								  keyAESaveOptions, 
								  typeWildCard, 
								  &saving);
								  
	if (err != errAEDescNotFound)
		ThrowIfOSErr(err);
		
	// Extract the [saving in <alias>] optional parameter, if present
	err = AEGetParamDesc(appleEvent, 
								  keyAEFile, 
								  typeWildCard, 
								  &savingIn);
								  
	if (err != errAEDescNotFound)
		ThrowIfOSErr(err);
			
	// Check for any required parameters we may have missed
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);
		
	// Now, do the application-related work
	SysBeep(2);
		

}

/*----------------------------------------------------------------------------
	HandleCount 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleCount(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err 				= noErr;
	long 		numberOfObjects 	= 0L;

	DescType	objectClass;

	if (!reply->dataHandle)
		return;
	
	// Get the class of object that we will count
	err = GetObjectClassFromAppleEvent(appleEvent, &objectClass);
	ThrowIfOSErr(err);

	// Make sure we got & handled all of the required paramters
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);
	
	// Send back the results
	numberOfObjects = CountApplicationObjects(token, objectClass);
	err = AEPutParamPtr(reply, keyAEResult, typeLongInteger, (Ptr)&numberOfObjects, sizeof(long));
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	HandleDataSize 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleDataSize(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr 	err = noErr;
	StAEDesc	data;
	long		size  = 0L;
	
	// First, get the data
	HandleGetData(token, appleEvent, reply);
	
	// now, extract it from the reply
	err = AEGetKeyDesc(reply, keyDirectObject, typeWildCard, &data);
	ThrowIfOSErr(err);
	
	size = data.GetDataSize();
	
	// do we leak all the data here?
	err = AEPutParamPtr(reply, 
								 keyAEResult, 
								 typeLongInteger, 
								 (Ptr)&size, 
								 sizeof(long));
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	HandleDelete 

	All attempts to delete an empty list are handled here
	Application contains documents and windows, and they can't be deleted
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleDelete(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err = noErr;
		
	if (AEListUtils::TokenContainsTokenList(token))
	{
		long 			numItems;
		
		AECountItems(token, &numItems);

		if (numItems > 0)
			err = errAEEventNotHandled;
	}

	ThrowIfOSErr(err);
}

/*----------------------------------------------------------------------------
	HandleDuplicate 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleDuplicate(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleExists 

	If <referenceToObject> exists...
	The AEResolve() function in AERCoreSuite.c will already have filtered
	out all cases where the object did not exist, so this function should
	always return TRUE.
	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleExists(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err 		= noErr;
	Boolean	foundIt	= true;

	err = AEPutParamPtr(reply, 
					 keyAEResult, 
					 typeBoolean, 
					 (Ptr)&foundIt, 
					 sizeof(Boolean));
		
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	HandleMake 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleMake(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleMove 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleMove(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleRun 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleRun(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err = noErr;

        // do stuff on startup that we want to do
        
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);
}

/*----------------------------------------------------------------------------
	HandleOpen 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleOpen(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err;
	
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);

	long 			numItems, i;
	Boolean		openedGroups = false;

	err = ::AECountItems(token, &numItems);
	ThrowIfOSErr(err);
	
	for (i = 1; i <= numItems; i++)
	{
		FSSpec 		fSpec;
		FInfo 		fndrInfo;
		AEKeyword 	keywd;
		DescType 		returnedType;
		Size 			actualSize;

		err = ::AEGetNthPtr(token, i, typeFSS, &keywd, &returnedType, (Ptr)&fSpec, sizeof(fSpec), &actualSize);
		ThrowIfOSErr(err);
		
		err = ::FSpGetFInfo(&fSpec, &fndrInfo);
		ThrowIfOSErr(err);

		nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
		cmdLine.HandleOpenOneDoc(fSpec, fndrInfo.fdType);
	}
}


/*----------------------------------------------------------------------------
	HandlePrint 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandlePrint(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err;
	
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);

	long 			numItems, i;
	Boolean		openedGroups = false;

	err = ::AECountItems(token, &numItems);
	ThrowIfOSErr(err);
	
	for (i = 1; i <= numItems; i++)
	{
		FSSpec 		fSpec;
		FInfo 		fndrInfo;
		AEKeyword 	keywd;
		DescType 		returnedType;
		Size 			actualSize;

		err = ::AEGetNthPtr(token, i, typeFSS, &keywd, &returnedType, (Ptr)&fSpec, sizeof(fSpec), &actualSize);
		ThrowIfOSErr(err);
		
		err = ::FSpGetFInfo(&fSpec, &fndrInfo);
		ThrowIfOSErr(err);

		nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
		cmdLine.HandlePrintOneDoc(fSpec, fndrInfo.fdType);
	}
}

/*----------------------------------------------------------------------------
	HandleQuit 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleQuit(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	// get optional saving param
	StAEDesc		savingParam;
	TAskSave		askSave = eSaveUnspecified;
	
	OSErr	err = ::AEGetKeyDesc(appleEvent, keyAESaveOptions, typeEnumeration, &savingParam);
	if (err != errAEDescNotFound)
	{
		DescType		enumValue = savingParam.GetEnumType();
		
		switch (enumValue)
		{
			case 'yes ':		askSave = eSaveYes;		break;
			case 'no  ':		askSave = eSaveNo;			break;
			case 'ask ':		askSave = eSaveAsk;		break;
		}
	}
	
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);

	nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
	err = cmdLine.Quit(askSave);
	ThrowIfOSErr(err);
}

/*----------------------------------------------------------------------------
	HandleSave 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleSave(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

#if 0
/*----------------------------------------------------------------------------
	HandleSetData 

	
----------------------------------------------------------------------------*/
void AEApplicationClass::HandleSetData(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr 			err 		= noErr;	
	StAEDesc			tokenData;
	AETokenDesc		tokenDesc(token);
	DescType		 	propertyCode;
			
	if (token->descriptorType == cProperty)
	{
		propertyCode = tokenDesc.GetPropertyCode();

		if (CanSetProperty(propertyCode))
		{
			// only the clipboard property is writeable
			// the clipboard data should be in a list, so we extract that list
			switch (propertyCode)
			{
				case pClipboard:
					err = AEGetKeyDesc(appleEvent, keyAEData, typeAEList, &tokenData);
					ThrowIfOSErr(err);
					
					SetDataForObject(token, &tokenData);		// may throw

					err = AEPutKeyDesc(reply, keyDirectObject, &tokenData);	// return the requested data
					ThrowIfOSErr(err);
					break;
					
				default:
					ThrowIfOSErr(errAENotModifiable);  // "Can't set xxx to nnn"
					break;
			}
		}
		else
		{
			ThrowIfOSErr(errAENotModifiable);
		}
	}
	else
	{
		ThrowIfOSErr(errAEEventNotHandled);
	}
	
}
#endif

#pragma mark -

/*----------------------------------------------------------------------------
	CountObjects 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::CountObjects(				DescType 		 	desiredType,
											DescType 		 	containerClass,
											const AEDesc *		container,
							   				long *			result)
{
	long		numberOfObjects = CountApplicationObjects(container, desiredType);
	*result = numberOfObjects;
}

#pragma mark -

/*----------------------------------------------------------------------------
	GetDataFromObject 
	
----------------------------------------------------------------------------*/
void AEApplicationClass::GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data)
{
	OSErr					err	= noErr;
	
	Str255					applicationName = "\p";
	Str255					versionString;
	
	ConstAETokenDesc				tokenDesc(token);
	
	ProcessSerialNumber		applicationProcessNumber;
	ProcessInfoRec				applicationInfo;
	FSSpec					appFSSpec;

	Boolean           				isFrontProcess 		= true;	// еее !gInBackground;
	
	DescType					aDescType		= cApplication;
	
	long						documentNumber	= 0L;
	unsigned long				elementNumber		= 0L;
		
	Boolean					usePropertyCode	= tokenDesc.UsePropertyCode();
	DescType					propertyCode;
	
	long 						free;
	long 						contiguous;
	unsigned long 				ticks;

	err = GetCurrentProcess(&applicationProcessNumber);	
	
	if (err == noErr)
	{
		applicationInfo.processInfoLength 	= sizeof(ProcessInfoRec);
		applicationInfo.processName 		= applicationName;
		applicationInfo.processAppSpec 	= &appFSSpec;
		err = GetProcessInformation(&applicationProcessNumber, &applicationInfo);
	}
				
	GetShortVersionString(2, versionString);
	PurgeSpace(&free, &contiguous);
				
	ticks = TickCount();

	propertyCode = tokenDesc.GetPropertyCode();
	
	switch (propertyCode)
	{
		case pProperties:
			err = AECreateList(nil, 0, true, data);
			ThrowIfOSErr(err);

			err = AEPutKeyPtr(data, pObjectType, 		typeType, 		&aDescType, sizeof(DescType));
			err = AEPutKeyPtr(data, pName,       		typeChar, 		&applicationName[1], applicationName[0]);
			err = AEPutKeyPtr(data, pVersion,    		typeChar, 		&versionString[1], versionString[0]);
			err = AEPutKeyPtr(data, pIsFrontProcess,	typeBoolean, 		&isFrontProcess, sizeof(Boolean));
			err = AEPutKeyPtr(data, pFreeMemory,		typeLongInteger, 	&free, sizeof(long));
			err = AEPutKeyPtr(data, pLargestFreeBlock,	typeLongInteger, 	&contiguous, sizeof(long));
			err = AEPutKeyPtr(data, pTicks,			typeLongInteger, 	&ticks, sizeof(long));
			break;
			
		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
			err = AECreateDesc(typeType, &aDescType, sizeof(DescType), data);
			break;
			
		case pName:	
			err = AECreateDesc(typeChar, &applicationName[1], applicationName[0], data);
			break;

		case pVersion:
			err = AECreateDesc(typeChar, &versionString[1], versionString[0], data);
			break;

		case pIsFrontProcess:
			err = AECreateDesc(typeBoolean, &isFrontProcess, sizeof(Boolean), data);
			break;

		case pFreeMemory:
			err = AECreateDesc(typeLongInteger, &free, sizeof(long), data);
			break;
			
		case pLargestFreeBlock:
			err = AECreateDesc(typeLongInteger, &contiguous, sizeof(long), data);
			break;
			
		case pTicks:
			err = AECreateDesc(typeLongInteger, &ticks, sizeof(long), data);
			break;
			
		case pClipboard:
#if !TARGET_CARBON		
			{
				//	Return all of the items currently on the clipboard.
				//	The returned information is an AEList, and each data type
				//	on the scrap gets its own entry in the list
				//	Since the OS doesn't supply the tools for getting all
				//	of the types in the scrap, we have to scan the scrap ourselves

				char			*scrapPtr;
				char			*scrapEnd;
				PScrapStuff	 scrapInfo;
				OSType		 itemType;
				long			 itemLength;
				long			 index;
				
				err = AECreateList(NULL, 0, false, data);
				ThrowIfOSErr(err);
				
				err = LoadScrap();										//	Make sure the scrap is in memory, not on disk.
				ThrowIfOSErr(err);
				
				scrapInfo = InfoScrap();								//	Get the base address of the scrap in RAM
				MoveHHi(scrapInfo->scrapHandle);
				HLock  (scrapInfo->scrapHandle);						// ...and lock it
				
				scrapPtr = (char *)*scrapInfo->scrapHandle;
				scrapEnd = scrapPtr + scrapInfo->scrapSize;
				
				// scan the scrap in memory and extract each scrap type
				
				index = 1;
				while (scrapPtr < scrapEnd) 
				{
					itemType = *(OSType *)scrapPtr;
					scrapPtr += sizeof(itemType);
					itemLength = *(long *)scrapPtr;
					scrapPtr += sizeof(itemLength);
					
					// Move this information into the next entry on the list
					err = AEPutPtr(data, index, itemType, scrapPtr, itemLength);
					ThrowIfOSErr(err);
						
					index++;
					
					// Adjust the pointer to the start of the next item
					
					if (itemLength & 1) 
						itemLength++; 										// If it's odd, make it even
						
					scrapPtr += itemLength;
				}
				HUnlock  (scrapInfo->scrapHandle);
			}
#endif
			break;

		default:
			Inherited::GetDataFromObject(token, desiredTypes, data);
			break;
	}

	ThrowIfOSErr(err);
}

/*----------------------------------------------------------------------------
	SetDataForObject 
	
	Assumption: HandleSetData() has already filtered out all attempts
	to write to a read-only property.
	
----------------------------------------------------------------------------*/
void AEApplicationClass::SetDataForObject(const AEDesc *token, AEDesc *data)
{
	OSErr				err			= noErr;

	long					numItems;
	long					index;
	AEKeyword 			theAEKeyword;
	
	ConstAETokenDesc			tokenDesc(token);
	Boolean				usePropertyCode     = tokenDesc.UsePropertyCode();
	DescType				propertyCode;
	
	if (usePropertyCode)
	{
		propertyCode = tokenDesc.GetPropertyCode();
		
		switch (propertyCode)
		{
			// the clipboard is the only writeable property for the application object
			case pClipboard:
				//	The data should be an AE list containing a series of things to be placed on the
				//	clipboard. The data type of each item is also the clipboard type for that data
#if !TARGET_CARBON				
				err = ZeroScrap();
				ThrowIfOSErr(err);
				
				AECountItems(data, &numItems);
				
				//  Copy each item onto the clipboard
				
				for (index = 1; index <= numItems; index++) 
				{
					StAEDesc		currentItemDesc;
					err = AEGetNthDesc(data, index, typeWildCard, &theAEKeyword, &currentItemDesc);
					ThrowIfOSErr(err);
						
					HLock(currentItemDesc.dataHandle);
					err = PutScrap(GetHandleSize(currentItemDesc.dataHandle), 
										  currentItemDesc.descriptorType,
								   	 	*currentItemDesc.dataHandle);
					ThrowIfOSErr(err);
				}
#endif
				break;
				
			default:
				ThrowIfOSErr(errAENotModifiable);
		}
	}
	
}


/*----------------------------------------------------------------------------
	GetKeyEventDataAs 

----------------------------------------------------------------------------*/
DescType AEApplicationClass::GetKeyEventDataAs(DescType propertyCode)
{
	DescType		returnType;

	switch (propertyCode)
	{
		case pClipboard:
			returnType = typeAEList;
			break;
			
		default:
			returnType = typeWildCard;
	
	}
	return returnType;
}


#pragma mark -

/*----------------------------------------------------------------------------
	CanSetProperty 
		
----------------------------------------------------------------------------*/
Boolean AEApplicationClass::CanSetProperty(DescType propertyCode)
{
	Boolean result = false;
	
	switch (propertyCode)
	{
		// Properties we can set:
		
		case pClipboard:
			result = true;
			break;
			
		// Properties we should be able to set, but they're not implemented yet:
		
					
		// Properties that are read-only

		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
		
		case pProperties:
		
		case pFreeMemory:
		case pLargestFreeBlock:
		case pTicks:

		case pIsFrontProcess:
		case pName:
		case pVersion:
		case pInsertionLoc:
		case pSelection:
		case pUserSelection:
			result = false;
			break;
		
		default:
			result = Inherited::CanSetProperty(propertyCode);
			break;
	}
	
	return result;
}


/*----------------------------------------------------------------------------
	CanGetProperty 
	
----------------------------------------------------------------------------*/
Boolean AEApplicationClass::CanGetProperty(DescType propertyCode)
{
	Boolean result = false;
	
	switch (propertyCode)
	{
		// Properties we can get:

		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
			
		case pProperties:
		
		case pFreeMemory:
		case pLargestFreeBlock:
		case pTicks:

		case pIsFrontProcess:
		case pName:
		case pVersion:
		case pInsertionLoc:
		case pSelection:
		case pUserSelection:
			result = true;
			break;
			
		// Properties we should be able to get, but they're not implemented yet:
					
		// Properties we should not be able to get:
		
		default:
			result = Inherited::CanGetProperty(propertyCode);
			break;
	}
	
	return result;
}


#pragma mark -

/*----------------------------------------------------------------------------
	CreateSelfSpecifier 

----------------------------------------------------------------------------*/
void AEApplicationClass::CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier)
{
	OSErr	err = ::AECreateDesc(typeNull, nil, 0, outSpecifier);
	ThrowIfOSErr(err);
}

#pragma mark -

/*----------------------------------------------------------------------------
	CountApplicationObjects 
	
----------------------------------------------------------------------------*/
long AEApplicationClass::CountApplicationObjects(const AEDesc *token, DescType desiredType)
{
	long		numberOfObjects = 0;
	OSErr	err = noErr;
	
	if (AEListUtils::TokenContainsTokenList(token))
	{
		err = AECountItems(token, &numberOfObjects);
	}
	else
	{
		AEDispatchHandler*	countHandler = AECoreClass::sAECoreHandler->GetDispatchHandler(desiredType);
		if (countHandler == nil)
			ThrowOSErr(errAEEventNotHandled);
		
		countHandler->CountObjects(desiredType, typeNull, token, &numberOfObjects);
		/*
		switch (desiredType)
		{
			case cDocument:
				numberOfObjects = AEDocumentClass::CountDocuments();
				break;

			case cWindow:
				numberOfObjects = AEWindowClass::CountWindows(kAnyWindowKind);
				break;
			
			// application specific classes
			case cGroupWindow:
				numberOfObjects = AEWindowClass::CountWindows(kUserGroupWindowKind);
				break;
			
			
			default:
				err = errAEEventNotHandled;
				break;
		}
		*/
	}

	ThrowIfOSErr(err);
	return numberOfObjects;
}

