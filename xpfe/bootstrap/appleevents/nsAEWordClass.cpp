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


#include "def.h"
#include "wasteutil.h"

#include "AEUtils.h"
#include "AETextClass.h"

#include "AEWordClass.h"


/*----------------------------------------------------------------------------
	GetNumItems 
	
----------------------------------------------------------------------------*/
UInt32 AEWordIterator::GetNumItems(const AEDesc* containerToken)
{
	AETokenDesc	container(containerToken);
	WEReference	theWE = container.GetWEReference();

	return theWE ? GetWENumWords(theWE) : 0;
}

/*----------------------------------------------------------------------------
	GetIndexedItemReference 
	
----------------------------------------------------------------------------*/
AEClassIterator::ItemRef AEWordIterator::GetIndexedItemReference(const AEDesc* containerToken, TAEListIndex itemIndex)
{
	return (ItemRef)itemIndex;
}

/*----------------------------------------------------------------------------
	GetIndexFromItemID 
	
----------------------------------------------------------------------------*/
TAEListIndex AEWordIterator::GetIndexFromItemID(const AEDesc* containerToken, ItemID itemID)
{
	return (TAEListIndex)itemID;
}

/*----------------------------------------------------------------------------
	GetIndexedItemID 
	
----------------------------------------------------------------------------*/
AEClassIterator::ItemID AEWordIterator::GetIndexedItemID(const AEDesc* containerToken, TAEListIndex itemIndex)
{
	return (ItemID)GetIndexedItemReference(containerToken, itemIndex);
}

/*----------------------------------------------------------------------------
	GetIDFromReference 
	
----------------------------------------------------------------------------*/
AEClassIterator::ItemID AEWordIterator::GetIDFromReference(const AEDesc* containerToken, ItemRef itemRef)
{
	return (ItemID)itemRef;
}

/*----------------------------------------------------------------------------
	GetReferenceFromID 
	
----------------------------------------------------------------------------*/
AEClassIterator::ItemRef AEWordIterator::GetReferenceFromID(const AEDesc* containerToken, ItemID itemID)
{
	return (ItemRef)itemID;
}

/*----------------------------------------------------------------------------
	GetItemIDFromToken 
	
----------------------------------------------------------------------------*/
AEClassIterator::ItemID AEWordIterator::GetItemIDFromToken(const AEDesc* token)
{
	AETokenDesc	container(token);
	return (ItemID)container.GetElementNumber();
}

/*----------------------------------------------------------------------------
	SetItemIDInCoreToken 
	
----------------------------------------------------------------------------*/
void AEWordIterator::SetItemIDInCoreToken(const AEDesc* containerToken, CoreTokenRecord* tokenRecord, ItemID itemID)
{
	AETokenDesc		tokenDesc(containerToken);
	WindowPtr		wind = tokenDesc.GetWindowPtr();
	WEReference		theWE = tokenDesc.GetWEReference();

	tokenRecord->window = wind;
	tokenRecord->theWE = theWE;
	
	tokenRecord->elementNumber = (TAEListIndex)itemID;
}

#pragma mark -

/*----------------------------------------------------------------------------
	AEWordClass 
	
----------------------------------------------------------------------------*/

AEWordClass::AEWordClass()
:	AEGenericClass(cWord, AETextClass::cTEText)
{
}

/*----------------------------------------------------------------------------
	~AEWordClass 
	
----------------------------------------------------------------------------*/
AEWordClass::~AEWordClass()
{

}

#pragma mark -


/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
----------------------------------------------------------------------------*/
void AEWordClass::GetItemFromContainer(		DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass, 
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	AEWordIterator		wordIterator;
	wordIterator.GetItemFromContainer(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
}


/*----------------------------------------------------------------------------
	CountObjects 
	
----------------------------------------------------------------------------*/
void AEWordClass::CountObjects(				DescType 		 	desiredType,
										DescType 		 	containerClass,
										const AEDesc *		container,
						   				long *			result)
{
	AEWordIterator		wordIterator;
	long		numItems = wordIterator.GetNumItems(container);
	*result = numItems;
}

#pragma mark -


/*----------------------------------------------------------------------------
	GetDataFromObject 
	
----------------------------------------------------------------------------*/
void AEWordClass::GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data)
{
	AETokenDesc		tokenDesc(token);

	DescType			aDescType		= cWord;
	DescType 			propertyCode 		= tokenDesc.GetPropertyCode();
	WindowPtr		window        	 	= tokenDesc.GetWindowPtr();
	WEReference		theWE			= tokenDesc.GetWEReference();
	TAEListIndex		wordIndex			= tokenDesc.GetElementNumber();
	OSErr			err				= noErr;
	long				wordStart, wordEnd;
	
	ThrowErrIfNil(theWE, errAENoSuchObject);
	
	switch (propertyCode)
	{
		case pProperties:
			err = AECreateList(nil, 0, true, data);
			ThrowIfOSErr(err);

			err = AEPutKeyPtr(data, pObjectType, typeType, &aDescType, sizeof(DescType));
			break;
			
		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
			err = AECreateDesc(typeType, &aDescType, sizeof(DescType), data);
			ThrowIfOSErr(err);
			break;
	
		case pContents:
		case typeNull:
			{
				err = GetWEIndexedWordOffsets(theWE, wordIndex, &wordStart, &wordEnd);
				ThrowIfOSErr(err);
			
				Handle			weHand = WEGetText(theWE);
				StHandleLocker		lockHand(weHand);
				
				err = AECreateDesc(typeChar, *weHand + wordStart, wordEnd - wordStart,data);
				ThrowIfOSErr(err);
			}
			break;
			
		default:
			Inherited::GetDataFromObject(token, desiredTypes, data);
			break;
	}
	
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	GetDataFromObject 
	
----------------------------------------------------------------------------*/
void AEWordClass::SetDataForObject(const AEDesc *token, AEDesc *data)
{
	AETokenDesc		tokenDesc(token);

	DescType			aDescType		= cWord;
	DescType 			propertyCode 		= tokenDesc.GetPropertyCode();
	WindowPtr		window        	 	= tokenDesc.GetWindowPtr();
	WEReference		theWE			= tokenDesc.GetWEReference();
	TAEListIndex		wordIndex			= tokenDesc.GetElementNumber();
	OSErr			err				= noErr;
	long				wordStart, wordEnd;
	
	ThrowErrIfNil(theWE, errAENoSuchObject);
	
	switch (propertyCode)
	{
	
		case pContents:
		case typeNull:
			{
				err = GetWEIndexedWordOffsets(theWE, wordIndex, &wordStart, &wordEnd);
				ThrowIfOSErr(err);
			
			}
			break;
	}
	
	ThrowIfOSErr(err);
}

#pragma mark -


/*----------------------------------------------------------------------------
	CanSetProperty 
		
----------------------------------------------------------------------------*/
Boolean AEWordClass::CanSetProperty(DescType propertyCode)
{
	Boolean result = false;
	
	switch (propertyCode)
	{
		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
			result = false;
			break;
		
		case typeNull:
			result = true;		// contents
			break;
			
		case pProperties:
		default:
			result = Inherited::CanSetProperty(propertyCode);
			break;
	}
	
	return result;
}


/*----------------------------------------------------------------------------
	CanGetProperty 
	
----------------------------------------------------------------------------*/
Boolean AEWordClass::CanGetProperty(DescType propertyCode)
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
			result = true;
			break;
			
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
void AEWordClass::CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier)
{
	AETokenDesc			tokenDesc(token);

	WEReference			theWE = tokenDesc.GetWEReference();
	TAEListIndex			wordIndex = tokenDesc.GetElementNumber();
	
	AEDesc				selfDesc = { typeNull, nil };
	AEDesc				containerSpecifier = { typeNull, nil };
	OSErr				err = noErr;
	
	GetContainerSpecifier(token, &containerSpecifier);
	
	err = ::AECreateDesc(typeLongInteger, &wordIndex, sizeof(TAEListIndex), &selfDesc);
	ThrowIfOSErr(err);
	
	err = ::CreateObjSpecifier(mClass, &containerSpecifier, formAbsolutePosition, &selfDesc, true, outSpecifier);
	ThrowIfOSErr(err);
}
