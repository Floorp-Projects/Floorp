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


#include "nsAEUtils.h"
#include "nsAETokens.h"
#include "nsAECoreClass.h"

#include "nsAEGenericClass.h"


/*----------------------------------------------------------------------------
	AEGenericClass 
	
----------------------------------------------------------------------------*/
AEGenericClass::AEGenericClass(DescType classType, DescType containerClass)
:	mClass(classType)
,	mContainerClass(containerClass)
,	mItemFromContainerAccessor(nil)
{

	// Window from null accessor used by the entire window hierarchy
	mItemFromContainerAccessor = NewOSLAccessorProc(AEGenericClass::ItemFromContainerAccessor);
	ThrowIfNil(mItemFromContainerAccessor);
	
	OSErr	err;
	err = AEInstallObjectAccessor(mClass,	 	containerClass, 
										mItemFromContainerAccessor, 
										(long)this, 
										false);

	// although items of a given class can't contain other items of the same class, 
	// we need this accessor to support formRelativePostion,
	// which sends us one item as a "container" and asks us to find
	// either the item before or after that item
	err = AEInstallObjectAccessor(mClass, 		mClass, 
										mItemFromContainerAccessor, 
										(long)this, 
										false);
	ThrowIfOSErr(err);

}

/*----------------------------------------------------------------------------
	~AEGenericClass 
	
----------------------------------------------------------------------------*/
AEGenericClass::~AEGenericClass()
{
	if (mItemFromContainerAccessor)
		DisposeOSLAccessorUPP(mItemFromContainerAccessor);
}

#pragma mark -


/*----------------------------------------------------------------------------
	DispatchEvent 
	
	Handles all OSL messages that this object should handle
----------------------------------------------------------------------------*/
void AEGenericClass::DispatchEvent(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
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
			case kAEClone:
				HandleDuplicate(token, appleEvent, reply);
				break;
				
			case kAEClose:
				HandleClose(token, appleEvent, reply);
				break;
				
			case kAECountElements:
				HandleCount(token, appleEvent, reply);
				break;
				
			case kAECreateElement:
				HandleMake(token, appleEvent, reply);
				break;
				
			case kAEDelete:
				HandleDelete(token, appleEvent, reply);
				break;
				
			case kAEDoObjectsExist:
				HandleExists(token, appleEvent, reply);
				break;
				
			case kAEGetData:
				HandleGetData(token, appleEvent, reply);
				break;
				
			case kAEGetDataSize:
				HandleDataSize(token, appleEvent, reply);
				break;
				
			case kAEMove:
				HandleMove(token, appleEvent, reply);
				break;
				
			case kAEOpen:		// == kAEOpenDocuments
				HandleOpen(token, appleEvent, reply);
				break;
				
			case kAEPrint:
				HandlePrint(token, appleEvent, reply);
				break;
			
			case kAEOpenApplication:
				HandleRun(token, appleEvent, reply);
				break;
							
			case kAEQuitApplication:
				HandleQuit(token, appleEvent, reply);
				break;
				
			case kAESave:
				HandleSave(token, appleEvent, reply);
				break;
				
			case kAESetData:
				HandleSetData(token, appleEvent, reply);
				break;

			// MT-NW suite
			case kAEExtract:
				HandleExtract(token, appleEvent, reply);
				break;
				
			case kAESendMessage:
				HandleSendMessage(token, appleEvent, reply);
				break;
				
			default:
				err = errAEEventNotHandled;
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
	GetProperty 
	
----------------------------------------------------------------------------*/
void	AEGenericClass::GetProperty(				DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass,
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	
	// call the base class utility method, which calls back up to the object
	GetPropertyFromListOrObject(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
}

#pragma mark -

/*----------------------------------------------------------------------------
	ItemFromContainerAccessor
	
	Callback for getting an item from its container
----------------------------------------------------------------------------*/
pascal OSErr AEGenericClass::ItemFromContainerAccessor(	DescType			desiredClass,		// cWindow
												const AEDesc*		containerToken,	// null container
												DescType			containerClass,  	 // cApplication
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken,		// specified window is returned in result
												long 				refCon)
{
	AEGenericClass*	itemClass = reinterpret_cast<AEGenericClass *>(refCon);
	if (!itemClass) return paramErr;
	
	OSErr		err = noErr;
	
	try
	{
		itemClass->GetItemFromContainer(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
	}
	catch(OSErr catchErr)
	{
		err = catchErr;
	}
	catch(...)
	{
		err = paramErr;
	}
	
	return err;
}



#pragma mark -

/*----------------------------------------------------------------------------
	HandleClose 
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleClose(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleCount 
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleCount(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ConstAETokenDesc	tokenDesc(token);
	long 			numberOfObjects = 0;
	DescType		objectClass;
	OSErr		err = noErr;	

	if (!reply->dataHandle)
		return;
	
	// Get the class of object that we will count
	err = GetObjectClassFromAppleEvent(appleEvent, &objectClass);
	ThrowIfOSErr(err);
	
	err = CheckForUnusedParameters(appleEvent);
	ThrowIfOSErr(err);

	if (AEListUtils::TokenContainsTokenList(token))
	{
		err = AECountItems(token, &numberOfObjects);
		ThrowIfOSErr(err);
		
	}
	else
	{
		CountObjects(objectClass, tokenDesc.GetDispatchClass(), token, &numberOfObjects);
	}

	err = AEPutParamPtr(reply, keyAEResult, 
								 typeLongInteger, 
								 (Ptr)&numberOfObjects, 
								 sizeof(long));
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	HandleGetData 
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleGetData(AEDesc *tokenOrTokenList, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr 		err 			= noErr;	
	StAEDesc		data;
	StAEDesc		desiredTypes;
	
	(void)AEGetParamDesc(appleEvent, keyAERequestedType, typeAEList, &desiredTypes);
	
	GetDataFromListOrObject(tokenOrTokenList, &desiredTypes, &data);

	if (reply->descriptorType != typeNull)
	{
		err = AEPutKeyDesc(reply, keyDirectObject, &data);
		ThrowIfOSErr(err);
	}
}

/*----------------------------------------------------------------------------
	HandleSetData 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleSetData(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	SetDataForListOrObject(token, appleEvent, reply);
}

/*----------------------------------------------------------------------------
	HandleDataSize 
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleDataSize(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleDelete 

	All attempts to delete an empty list are handled here
	Application contains documents and windows, and they can't be deleted
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleDelete(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleDuplicate 
	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleDuplicate(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleExists 

	If <anObject> exists...
	The AEResolve() function in AERCoreSuite.c will already have filtered
	out all cases where the object did not exist, so this function should
	always return TRUE.

----------------------------------------------------------------------------*/
void AEGenericClass::HandleExists(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	OSErr	err;
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
void AEGenericClass::HandleMake(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	DescType		insertionPos	= typeNull;
	OSErr		err 			= noErr;

	StAEDesc		insertionLoc;
	StAEDesc		objectSpec;

	// For Create Element, the object specifier is contained in  
	// a typeInsertionLoc record instead of in a direct parameter. 
	// We coerce the insertionLoc record into an AERecord so we can extract the fields.	 
	// Notice that this is a REQUIRED parameter, but we give it a default behavior
	// by creating a new element at beginning of the first document
	
	err = ::AEGetParamDesc(appleEvent, 				// Extract as a AERecord
								  keyAEInsertHere, 
								  typeAERecord, 
								  &insertionLoc);
	if (err == errAEDescNotFound)
	{
		insertionPos	= kAEBeginning;
		err			= noErr;
	}
	else if (err == noErr)
	{
		// Get the enumerated insertion location (at end, in front, before, after, replace.)
		
		OSType		typeCode;
		Size			actualSize;
		err = ::AEGetKeyPtr(&insertionLoc, 
								  	keyAEPosition, 			// insertion location
									typeEnumeration, 
									&typeCode, 
									(Ptr)&insertionPos,
						   			sizeof(insertionPos), 
						   			&actualSize);

		// Extract and resolve the object specifier from the insertion location record.
		// In a case like "make new rectangle before rectangle 1 of document 1",
		// the ospec will resolve to "rectangle 1 of document 1"
				 
		err = ::AEGetKeyDesc(&insertionLoc, 
									keyAEObject, 
									typeWildCard, 
									&objectSpec);
	}

	// if there was a object specifier in the insertion location (eg, "before rectangle 1 of document 1"),
	//   then we call AEResolve() to get a token for it,
	// Otherwise, is was something like "at end" or "at beginning", which is also OK, 
	//   then deal with it correctly later.
	if (objectSpec.descriptorType == typeNull) 
	{
		::AEDisposeDesc(token);			// destroy it's old representation, token will now be null descriptor
	}
	else
	{
		err = ::AEResolve(&objectSpec, 
							 kAEIDoMinimum, 
							 token);			// token will contain info about the object to insert before, after, etc.
		ThrowIfOSErr(err);
	}
		
	// Extract the optional parameters from the AppleEvent
	
	// ----- [with data ....] -----
	
	StAEDesc		withData;
	const AEDesc*	withDataPtr = nil;
	
	err = ::AEGetParamDesc(appleEvent, 
								  keyAEData,
								  typeWildCard,
								  &withData);
	if (err == errAEDescNotFound)
		err = noErr;
	else
	{
		ThrowIfOSErr(err);
		withDataPtr = &withData;
	}

	// ----- [with properties {property: value, ...}] -----
	StAEDesc		withProperties;
	const AEDesc*	withPropertiesPtr = nil;
	err = AEGetParamDesc(appleEvent, 
								  keyAEPropData, 
								  typeWildCard, 
								  &withProperties);
								  
	if (err == errAEDescNotFound)
		err = noErr;
	else
	{
		ThrowIfOSErr(err);
		withPropertiesPtr = &withProperties;
	}
	
	// Finally, use the token and other parameters to create & initialize the object
	MakeNewObject(insertionPos, token, withDataPtr, withPropertiesPtr, reply);
}

/*----------------------------------------------------------------------------
	HandleMove 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleMove(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleOpen 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleOpen(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleRun 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleRun(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandlePrint 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandlePrint(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleQuit 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleQuit(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	HandleSave 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleSave(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowOSErr(errAEEventNotHandled);
}


/*----------------------------------------------------------------------------
	HandleExtract 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleExtract(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowOSErr(errAEEventNotHandled);
}


/*----------------------------------------------------------------------------
	HandleSendMessage 

	
----------------------------------------------------------------------------*/
void AEGenericClass::HandleSendMessage(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply)
{
	ThrowOSErr(errAEEventNotHandled);
}


#pragma mark -

/*----------------------------------------------------------------------------
	CompareObjects 
	
----------------------------------------------------------------------------*/
void AEGenericClass::CompareObjects(				DescType			comparisonOperator,
											const AEDesc *		object,
											const AEDesc *		descriptorOrObject,
											Boolean *			result)
{
	ThrowOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	CountObjects 
	
----------------------------------------------------------------------------*/
void AEGenericClass::CountObjects(					DescType 		 	desiredType,
											DescType 		 	containerClass,
											const AEDesc *		container,
							   				long *			result)
{
	ThrowOSErr(errAEEventNotHandled);
}

#pragma mark -

/*----------------------------------------------------------------------------
	GetDataFromListOrObject 

	
----------------------------------------------------------------------------*/

void AEGenericClass::GetDataFromListOrObject(const AEDesc *tokenOrTokenList, AEDesc *desiredTypes, AEDesc *data)
{
	if (AEListUtils::TokenContainsTokenList(tokenOrTokenList) == false)
	{
		GetDataFromObject(tokenOrTokenList, desiredTypes, data);
	}
	else
	{
		ThrowIfOSErr(AECreateList(nil, 0, false, data));
		GetDataFromList(tokenOrTokenList, desiredTypes, data);
	}
}

/*----------------------------------------------------------------------------
	GetDataFromListOrObject 

	
----------------------------------------------------------------------------*/

void AEGenericClass::SetDataForListOrObject(const AEDesc *tokenOrTokenList, const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc			data;
	
	switch (tokenOrTokenList->descriptorType)
	{
		case typeAEList:
			{
				AECoreClass::GetAECoreHandler()->GetEventKeyDataParameter(appleEvent, typeWildCard, &data);
				SetDataForList(tokenOrTokenList, &data);
			}
			break;
				
		case cProperty:
			{
				ConstAETokenDesc	tokenDesc(tokenOrTokenList);
				DescType		propertyCode = tokenDesc.GetPropertyCode();
				//DescType		objectClass    = tokenDesc.GetObjectClass();
				
				if (CanSetProperty(propertyCode))
				{
					AECoreClass::GetAECoreHandler()->GetEventKeyDataParameter(appleEvent, GetKeyEventDataAs(propertyCode), &data);
					SetDataForObject(tokenOrTokenList, &data);
				}
				else
				{
					ThrowIfOSErr(errAENotModifiable);
				}
			}
			break;
			
		default:
			ThrowIfOSErr(errAENotModifiable);
	}
}

/*----------------------------------------------------------------------------
	GetDataFromList 

	
----------------------------------------------------------------------------*/
void AEGenericClass::GetDataFromList(const AEDesc *srcList, AEDesc *desiredTypes, AEDesc *dstList)
{
	OSErr		err;
	long			itemNum;
	long			numItems;
	DescType		keyword;
	StAEDesc		srcItem;
	StAEDesc		dstItem;
		
	err = AECountItems((AEDescList*)srcList, &numItems);
	ThrowIfOSErr(err);
		
	for (itemNum = 1; itemNum <= numItems; itemNum++)
	{
		err = AEGetNthDesc(srcList, itemNum, typeWildCard, &keyword, &srcItem);
		ThrowIfOSErr(err);
		
		if (AEListUtils::TokenContainsTokenList(&srcItem) == false)
		{
			GetDataFromObject(&srcItem, desiredTypes, &dstItem);  // Get data from single item
		}
		else
		{
			ThrowIfOSErr(AECreateList(nil, 0, false, &dstItem));
			GetDataFromList(&srcItem, desiredTypes, &dstItem);
		}
		err = AEPutDesc(dstList, itemNum, &dstItem);
		ThrowIfOSErr(err);
	}
}


/*----------------------------------------------------------------------------
	GetDataFromObject 

	
----------------------------------------------------------------------------*/
void AEGenericClass::GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data)
{
	ConstAETokenDesc		tokenDesc(token);
	DescType 			propertyCode 		= tokenDesc.GetPropertyCode();
	OSErr			err				= noErr;

	switch (propertyCode)
	{
		case pContents:
		case typeNull:
			// must mean contents. Make a self specifier.
			CreateSpecifier(token, data);
			break;
			
		default:
			err = errAECantSupplyType;
			break;
	}
	
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	SetDataForList 

	Given a token that contains a list of cWindow tokens,
	walk the list recursively to set the data for each token in the list
	
----------------------------------------------------------------------------*/
void AEGenericClass::SetDataForList(const AEDesc *token, AEDesc *data)
{
	OSErr 			err;
		
	if (AEListUtils::TokenContainsTokenList(token) == false)
	{
		SetDataForObject(token, data);
	}
	else
	{
		long			numItems;
		long			itemNum;
		err = AECountItems((AEDescList*)token, &numItems);
		ThrowIfOSErr(err);
		
		for (itemNum = 1; itemNum <= numItems; itemNum++)
		{
			StAEDesc	  	tempToken;
			AEKeyword	keyword;
			
		 	err = AEGetNthDesc((AEDescList*)token, itemNum, typeWildCard, &keyword, &tempToken);
			ThrowIfOSErr(err);
			
			if (AEListUtils::TokenContainsTokenList(&tempToken) == false)
			{
				SetDataForObject(&tempToken, data);  		// Set data from single item
			}
			else
			{
				SetDataForList(&tempToken, data); 	// Recurse sublist
			}
		}
	}
}


/*----------------------------------------------------------------------------
	CanGetProperty 

----------------------------------------------------------------------------*/
DescType AEGenericClass::GetKeyEventDataAs(DescType propertyCode)
{
	return typeWildCard;
}

#pragma mark -

/*----------------------------------------------------------------------------
	CanGetProperty 

----------------------------------------------------------------------------*/
Boolean AEGenericClass::CanGetProperty(DescType propertyCode)
{
	Boolean	canGet = false;
	
	switch (propertyCode)
	{
		case pContents:
			canGet = true;
			break;
	}
	return canGet;
}

/*----------------------------------------------------------------------------
	CanSetProperty 

----------------------------------------------------------------------------*/
Boolean AEGenericClass::CanSetProperty(DescType propertyCode)
{
	return false;
}


#pragma mark -

/*----------------------------------------------------------------------------
	CreateSpecifier 

	Subclasses should not need to override this. It 
----------------------------------------------------------------------------*/
void AEGenericClass::CreateSpecifier(const AEDesc *token, AEDesc *outSpecifier)
{
	CreateSelfSpecifier(token, outSpecifier);
}

/*----------------------------------------------------------------------------
	GetContainerSpecifier 

----------------------------------------------------------------------------*/
void AEGenericClass::GetContainerSpecifier(const AEDesc *token, AEDesc *outContainerSpecifier)
{
	outContainerSpecifier->descriptorType = typeNull;
	outContainerSpecifier->dataHandle = nil;
	
	AEDispatchHandler*	handler = AECoreClass::GetDispatchHandlerForClass(mContainerClass);
	if (handler)
	{
		handler->CreateSelfSpecifier(token, outContainerSpecifier);
	}
}

#pragma mark -

/*----------------------------------------------------------------------------
	GetPropertyFromListOrObject 

----------------------------------------------------------------------------*/
void AEGenericClass::GetPropertyFromListOrObject(		DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass,
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken)
{
	if (AEListUtils::TokenContainsTokenList((AEDescList*)containerToken) == false)
	{
		GetPropertyFromObject(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
	}
	else
	{
		OSErr	err = AECreateList(nil, 0, false, resultToken);
		ThrowIfOSErr(err);
		
		GetPropertyFromList(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
	}
}


/*----------------------------------------------------------------------------
	GetPropertyFromList 

----------------------------------------------------------------------------*/
void AEGenericClass::GetPropertyFromList(				DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass,
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken)
{
	OSErr		err		= noErr;
	long			itemNum;
	long			numItems;
	DescType		keyword;
	
	err = AECountItems((AEDescList*)containerToken, &numItems);
	ThrowIfOSErr(err);
		
	for (itemNum = 1; itemNum <= numItems; itemNum++)
	{
		StAEDesc		srcItem;
		StAEDesc		dstItem;
		
		err = AEGetNthDesc(containerToken, itemNum, typeWildCard, &keyword, &srcItem);
		ThrowIfOSErr(err);
		
		if (AEListUtils::TokenContainsTokenList(&srcItem) == false)
		{
			GetPropertyFromObject(desiredClass, &srcItem, containerClass, keyForm, keyData, &dstItem);
		}
		else
		{
			err = AECreateList(nil, 0, false, &dstItem);
			ThrowIfOSErr(err);
			
			GetPropertyFromList(desiredClass, &srcItem, containerClass, keyForm, keyData, &dstItem);
		}

		err = AEPutDesc(resultToken, itemNum, &dstItem);
		ThrowIfOSErr(err);
	}
	
}


/*----------------------------------------------------------------------------
	GetPropertyFromObject 

----------------------------------------------------------------------------*/
void AEGenericClass::GetPropertyFromObject(			DescType			desiredType,
						  					const AEDesc*		containerToken,
								  			DescType			containerClass,
								  			DescType			keyForm,
								 			const AEDesc*		keyData,
								  			AEDesc*			resultToken)
{
	OSErr			err;	
	DescType			requestedProperty;

	err = AEDuplicateDesc(containerToken, resultToken);
	ThrowIfOSErr(err);
		
	requestedProperty = **(DescType**)(keyData->dataHandle);
	
	if (requestedProperty == kAEAll || requestedProperty == keyAEProperties)
		requestedProperty = pProperties;

	if (CanGetProperty(requestedProperty) || CanSetProperty(requestedProperty))
	{
		AETokenDesc		resultTokenDesc(resultToken);
		resultToken->descriptorType = desiredType;
		resultTokenDesc.SetPropertyCode(requestedProperty);
	}
	else
	{
		ThrowIfOSErr(errAEEventNotHandled);
	}
}


#pragma mark -


/*----------------------------------------------------------------------------
	MakeNewObject 

----------------------------------------------------------------------------*/
void AEGenericClass::MakeNewObject(				const DescType		insertionPosition,
											const AEDesc*		token,
											const AEDesc*		ptrToWithData, 
											const AEDesc*		ptrToWithProperties,
											AppleEvent*		reply)
{
	ThrowOSErr(errAEEventNotHandled);
}

