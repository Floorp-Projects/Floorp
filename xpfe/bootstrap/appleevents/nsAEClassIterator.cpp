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

#include "nsAEClassIterator.h"


const AEClassIterator::ItemRef		AEClassIterator::kNoItemRef = -1;
const AEClassIterator::ItemID		AEClassIterator::kNoItemID = -1;

/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
----------------------------------------------------------------------------*/
void AEClassIterator::GetItemFromContainer(		DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass, 
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	OSErr				err	= noErr;
	
	CStr255				itemName;
	ItemRef				itemRef = kNoItemRef;
	ItemID				itemID = kNoItemID;
	DescType				keyDataType 	= keyData->descriptorType;

	Boolean				wantsAllItems	= false;

	StAEDesc 				startObject;	// These are used to resolve formRange
	StAEDesc 				stopObject;
		
	CoreTokenRecord		token;
	
	long					numItems = GetNumItems(containerToken);
	TAEListIndex			itemIndex;
	
	CheckKeyFormSupport(keyForm);		// throws on error
	
	switch (keyForm)
	{
		case formName:																// item by name
			if (DescToCString(keyData, itemName, 255) != noErr)
				ThrowIfOSErr(errAECoercionFail);

			itemRef = GetNamedItemReference(containerToken, itemName);
			if (itemRef == kNoItemRef)
				ThrowIfOSErr(errAENoSuchObject);
			break;
		
		case formAbsolutePosition:														// item by number
			itemIndex = NormalizeAbsoluteIndex(keyData, numItems, &wantsAllItems);
			if (wantsAllItems == false)
			{
				if (itemIndex == 0 || itemIndex > numItems)
					ThrowOSErr(errAEIllegalIndex);
			}
			itemRef = GetIndexedItemReference(containerToken, itemIndex);
			if (itemRef == kNoItemRef)
				ThrowOSErr(errAENoSuchObject);
			break;	
		
		case formRelativePosition:
			itemRef = ProcessFormRelativePostition(containerToken, keyData);
			break;	
		
		case formRange:
			switch (keyDataType)
			{		
				case typeRangeDescriptor:
					{
						ProcessFormRange((AEDesc *)keyData, &startObject, &stopObject);
						
						ConstAETokenDesc	startToken(&startObject);
						ConstAETokenDesc	stopToken(&stopObject);
						DescType 		startType = startToken.GetDispatchClass();
						DescType 		stopType  = stopToken.GetDispatchClass();
	 
						if (startType != mClass || stopType != mClass)
							ThrowOSErr(errAEWrongDataType);
					}
					break;

				default:
					ThrowOSErr(errAEWrongDataType);
					break;	
			}
			break;	
		
		default:
			ThrowIfOSErr(errAEEventNotHandled);
	}
	
	// if user asked for all items, and there aren't any,
	// we'll be kind and return an empty list.

	if (wantsAllItems && (err == errAENoSuchObject || err == errAEIllegalIndex))
	{
		err = AECreateList(nil, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
		return;
	}

	ThrowIfOSErr(err);

	// fill in the result token
	token.dispatchClass 	= GetClass();
	token.objectClass	= GetClass();
	token.propertyCode 	= typeNull;
	
	if (wantsAllItems)
	{
		err = AECreateList(NULL, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
		
		for (TAEListIndex index = 1; index <= numItems; index++)
		{
			ItemID		itemID = GetIndexedItemID(containerToken, index);
			if (itemID != kNoItemID)
			{
				SetItemIDInCoreToken(containerToken, &token, itemID);
				err = AEPutPtr(resultToken, 0, desiredClass, &token, sizeof(token));
				ThrowIfOSErr(err);
			}
		}
	}
	else if (keyForm == formRange)
	{
		ConstAETokenDesc		startToken(&startObject);
		ConstAETokenDesc		stopToken(&stopObject);
		
		ItemID			beginItemID 	= GetItemIDFromToken(&startObject);
		ItemID			endItemID	 	= GetItemIDFromToken(&stopObject);
		
		TAEListIndex		beginIndex		= GetIndexFromItemID(containerToken, beginItemID);
		TAEListIndex		endIndex		= GetIndexFromItemID(containerToken, endItemID);
										
		err = AECreateList(nil, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
			
		if (beginIndex > endIndex) // swap elements
		{
			TAEListIndex	temp = beginIndex;
			beginIndex		= endIndex;
			endIndex   	= temp;
		}
		
		for (TAEListIndex i = beginIndex; i <= endIndex; i ++)
		{
			ItemID		itemID = GetIndexedItemID(containerToken, i);
			if (itemID != kNoItemID)
			{
				SetItemIDInCoreToken(containerToken, &token, itemID);
				err = AEPutPtr(resultToken, 0, desiredClass, &token, sizeof(token));
				ThrowIfOSErr(err);
			}
		}
	}
	else
	{
		SetItemIDInCoreToken(containerToken, &token, GetIDFromReference(containerToken, itemRef));
		err = AECreateDesc(desiredClass, &token, sizeof(token), resultToken);
		ThrowIfOSErr(err);
	}
}


/*----------------------------------------------------------------------------
	ProcessFormRelativePostition 
	
----------------------------------------------------------------------------*/

AEClassIterator::ItemRef AEClassIterator::ProcessFormRelativePostition(const AEDesc* anchorToken, const AEDesc *keyData)
{
	OSErr		err = noErr;
	ItemID		anchorItemID = GetItemIDFromToken(anchorToken);
	TAEListIndex	anchorListIndex = GetIndexFromItemID(anchorToken, anchorItemID);
	TAEListIndex	wantedListIndex = 0;
	long			numItems = GetNumItems(anchorToken);
	ItemRef		returnRef = kNoItemRef;

	if (anchorListIndex != 0)
	{
		switch (keyData->descriptorType)
		{
		   case typeEnumerated:
			   	DescType		positionEnum;	
				if (DescToDescType((AEDesc*)keyData, &positionEnum) != noErr)
					ThrowIfOSErr(errAECoercionFail);

				switch (positionEnum)
				{
					case kAENext:						// get the window behind the anchor
						wantedListIndex = anchorListIndex + 1;
						if (wantedListIndex > numItems)
							err = errAENoSuchObject;
						break;
						
					case kAEPrevious:					// get the document in front of the anchor
						wantedListIndex = anchorListIndex - 1;
						if (wantedListIndex < 1)
							err = errAENoSuchObject;
						break;
						
					default:
						err = errAEEventNotHandled;
						break;
				}
				ThrowIfOSErr(err);
				break;
				
			default:
				err = errAECoercionFail;
				break;
		}
	}
	
	ThrowIfOSErr(err);
	return GetIndexedItemReference(anchorToken, wantedListIndex);
}


/*----------------------------------------------------------------------------
	NormalizeAbsoluteIndex 
	
	 Handles formAbsolutePosition resolution
	
---------------------------------------------------------------------------*/
TAEListIndex AEClassIterator::NormalizeAbsoluteIndex(const AEDesc *keyData, TAEListIndex maxIndex, Boolean *isAllItems)
{
	TAEListIndex	index;
	*isAllItems = false;								// set to true if we receive kAEAll constant
	
	// Extract the formAbsolutePosition data, either a integer or a literal constant
	
	switch (keyData->descriptorType)
	{
		case typeLongInteger:						// positve or negative index
			if (DescToLong(keyData, &index) != noErr)
				ThrowOSErr(errAECoercionFail);
				
			if (index < 0)							// convert a negative index from end of list to a positive index from beginning of list
				index = maxIndex + index + 1;
			break;
			
	   case typeAbsoluteOrdinal:										// 'abso'
	   		DescType		ordinalDesc;
			if (DescToDescType((AEDesc*)keyData, &ordinalDesc) != noErr)
				ThrowOSErr(errAECoercionFail);
			
			switch (ordinalDesc)
			{
			   	case kAEFirst:
			   		index = 1;
					break;
					
				case kAEMiddle:
					index = (maxIndex >> 1) + (maxIndex % 2);
					break;
						
			   	case kAELast:
			   		index = maxIndex;
					break;
						
			   	case kAEAny:
					index = (TickCount() % maxIndex) + 1;		// poor man's random
					break;
						
			   	case kAEAll:
			   		index = 1;
			   		*isAllItems = true;
					break;
			}
			break;

		default:
			ThrowOSErr(errAEWrongDataType);
	}

	// range-check the new index number
	if ((index < 1) || (index > maxIndex))
		ThrowOSErr(errAEIllegalIndex);
		
	return index;
}


/*----------------------------------------------------------------------------
	ProcessFormRange 
	
	Handles formRange resolution of boundary objects
----------------------------------------------------------------------------*/

void AEClassIterator::ProcessFormRange(AEDesc *keyData, AEDesc *start, AEDesc *stop)
{
	OSErr 		err = noErr;
	StAEDesc 		rangeRecord;
	StAEDesc		ospec;
	
	// coerce the range record data into an AERecord 
	
 	err = AECoerceDesc(keyData, typeAERecord, &rangeRecord);
 	ThrowIfOSErr(err);
	 
	// object specifier for first object in the range
	err = AEGetKeyDesc(&rangeRecord, keyAERangeStart, typeWildCard, &ospec);
 	if (err == noErr && ospec.descriptorType == typeObjectSpecifier)
 		err = AEResolve(&ospec, kAEIDoMinimum, start);
 		
 	ThrowIfOSErr(err);
 		
	ospec.Clear();
		
	// object specifier for last object in the range
	
	err = AEGetKeyDesc(&rangeRecord, keyAERangeStop, typeWildCard, &ospec);
  	if (err == noErr && ospec.descriptorType == typeObjectSpecifier)
 		err = AEResolve(&ospec, kAEIDoMinimum, stop);

	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	CheckKeyFormSupport
	
	throws if unsupported key form
----------------------------------------------------------------------------*/
void AEClassIterator::CheckKeyFormSupport(DescType keyForm)
{
	UInt16			testMask;
	switch (keyForm)
	{
		case formAbsolutePosition:		testMask = eHasFormAbsolutePosition;		break;
		case formRelativePosition:		testMask = eHasFormRelativePosition;		break;
		case formTest:					testMask = eHasFormTest;				break;
		case formRange:				testMask = eHasFormRange;				break;
		case formPropertyID:			testMask = eHasFormPropertyID;			break;
		case formName:				testMask = eHasFormName;				break;
		default:
			AE_ASSERT(false, "Unknown key form");
	}
	if ((mKeyFormSupport & testMask) == 0)
		ThrowOSErr(errAEBadKeyForm);
}

#pragma mark -


/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
----------------------------------------------------------------------------*/
void AENamedClassIterator::GetItemFromContainer(	DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass, 
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	OSErr				err	= noErr;
	
	CStr255				itemName;
	DescType				keyDataType 	= keyData->descriptorType;

	Boolean				wantsAllItems	= false;

	StAEDesc 				startObject;	// These are used to resolve formRange
	StAEDesc 				stopObject;
		
	CoreTokenRecord		token;
	
	long					numItems = GetNumItems(containerToken);
	
	CheckKeyFormSupport(keyForm);		// throws on error
	
	switch (keyForm)
	{
		case formName:																// item by name
			if (DescToCString(keyData, itemName, 255) != noErr)
				ThrowIfOSErr(errAECoercionFail);

			if (!NamedItemExists(containerToken, itemName))
				ThrowIfOSErr(errAENoSuchObject);
			break;
		
/*
		case formAbsolutePosition:														// item by number
	   		DescType		ordinalDesc;
			if (DescToDescType((AEDesc*)keyData, &ordinalDesc) != noErr)
				ThrowOSErr(errAECoercionFail);

			if (ordinalDesc != kAEAll)
				ThrowOSErr(errAEWrongDataType);

			wantsAllItems = true;
			break;	
*/		
		default:
			ThrowIfOSErr(errAEEventNotHandled);
	}
	
	ThrowIfOSErr(err);

	// fill in the result token
	token.dispatchClass 	= GetClass();
	token.objectClass	= GetClass();
	token.propertyCode 	= typeNull;
	
	if (wantsAllItems)
	{
		err = AECreateList(NULL, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
		
		for (TAEListIndex index = 1; index <= numItems; index++)
		{
			ItemID		itemID = GetIndexedItemID(containerToken, index);
			if (itemID != kNoItemID)
			{
				SetItemIDInCoreToken(containerToken, &token, itemID);
				err = AEPutPtr(resultToken, 0, desiredClass, &token, sizeof(token));
				ThrowIfOSErr(err);
			}
		}
	}
	else
	{
		SetNamedItemIDInCoreToken(containerToken, &token, itemName);
		err = AECreateDesc(desiredClass, &token, sizeof(token), resultToken);
		ThrowIfOSErr(err);
	}
}

