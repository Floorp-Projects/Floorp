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


#ifndef __AECLASSITERATOR__
#define __AECLASSITERATOR__


#include "nsAETokens.h"

class AEClassIterator
{

protected:

	enum
	{
		eFormAbsolutePositionBit		= 0,
		eFormRelativePositionBit,
		eFormTestBit,
		eFormRangeBit,
		eFormPropertyIDBit,
		eFormNameBit
	};
	
	
public:

	enum
	{
		eHasFormAbsolutePosition		= (1 << eFormAbsolutePositionBit),
		eHasFormRelativePosition		= (1 << eFormRelativePositionBit),
		eHasFormTest				= (1 << eFormTestBit),
		eHasFormRange				= (1 << eFormPropertyIDBit),
		eHasFormPropertyID			= (1 << eFormRangeBit),
		eHasFormName				= (1 << eFormNameBit),
		
		eStandardKeyFormSupport = (eHasFormAbsolutePosition | eHasFormRelativePosition | eHasFormRange | eHasFormName)
	};
	
	
						AEClassIterator(DescType classType, UInt16 keyFormSupport = eStandardKeyFormSupport)
						:	mClass(classType)
						,	mKeyFormSupport(keyFormSupport)
						{}
						
	virtual 				~AEClassIterator() {}
	
	virtual void			GetItemFromContainer(		DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass, 
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken);

	virtual UInt32			GetNumItems(const AEDesc* containerToken) = 0;			// get the number of items

	typedef UInt32			ItemRef;				// runtime, optimized reference to an object
	typedef UInt32			ItemID;				// persistent reference, used for storage

protected:
	
	static const ItemRef		kNoItemRef;			// value for a nonexistent item reference (usu. -1)
	static const ItemID		kNoItemID;			// value for a nonexistent item ID (usu -1)


	virtual ItemRef			GetNamedItemReference(const AEDesc* containerToken, const char *itemName) = 0;
	virtual ItemRef			GetIndexedItemReference(const AEDesc* containerToken, TAEListIndex itemIndex) = 0;
	
	virtual TAEListIndex		GetIndexFromItemID(const AEDesc* containerToken, ItemID itemID) = 0;
	
	virtual ItemID			GetNamedItemID(const AEDesc* containerToken, const char *itemName) = 0;
	virtual ItemID			GetIndexedItemID(const AEDesc* containerToken, TAEListIndex itemIndex) = 0;

	// index to name
	virtual void			GetIndexedItemName(const AEDesc* containerToken, TAEListIndex itemIndex, char *outName, long maxLen) = 0;

	// conversion routines.
	virtual ItemID			GetIDFromReference(const AEDesc* containerToken, ItemRef itemRef) = 0;
	virtual ItemRef			GetReferenceFromID(const AEDesc* containerToken, ItemID itemID) = 0;

	virtual ItemID			GetItemIDFromToken(const AEDesc* token) = 0;
	virtual void			SetItemIDInCoreToken(const AEDesc* containerToken, CoreTokenRecord* tokenRecord, ItemID itemID) = 0;
	
	// Range and index processing routines.
	virtual ItemRef			ProcessFormRelativePostition(const AEDesc* anchorToken, const AEDesc *keyData);
	TAEListIndex			NormalizeAbsoluteIndex(const AEDesc *keyData, TAEListIndex maxIndex, Boolean *isAllItems);
	void					ProcessFormRange(AEDesc *keyData, AEDesc *start, AEDesc *stop);

	void					CheckKeyFormSupport(DescType keyForm);		// throws if unsupported key form
	
	DescType				GetClass() { return mClass; }
	

	DescType				mClass;
	UInt16				mKeyFormSupport;
};


class AENamedClassIterator : public AEClassIterator
{
public:
						AENamedClassIterator(DescType classType)
						: AEClassIterator(classType, eHasFormName)
						{}
						
						~AENamedClassIterator() {}

	virtual void			GetItemFromContainer(		DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass, 
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken);

protected:

	
	// stubs for those pure virtual functions that we can't use with named items
	virtual UInt32			GetNumItems(const AEDesc* containerToken) { return 0; }

	virtual ItemRef			GetNamedItemReference(const AEDesc* containerToken, const char *itemName) { return kNoItemRef; }
	virtual ItemRef			GetIndexedItemReference(const AEDesc* containerToken, TAEListIndex itemIndex) { return kNoItemRef; }
	
	virtual TAEListIndex		GetIndexFromItemID(const AEDesc* containerToken, ItemID itemID) { return 0; }
	
	virtual Boolean			NamedItemExists(const AEDesc* containerToken, const char *itemName) = 0;

	virtual ItemID			GetNamedItemID(const AEDesc* containerToken, const char *itemName) { return kNoItemID; }
	virtual ItemID			GetIndexedItemID(const AEDesc* containerToken, TAEListIndex itemIndex) { return kNoItemID; }

	// index to name
	virtual void			GetIndexedItemName(const AEDesc* containerToken, TAEListIndex itemIndex, char *outName, long maxLen) {}

	// conversion routines.
	virtual ItemID			GetIDFromReference(const AEDesc* containerToken, ItemRef itemRef) { return kNoItemID; }
	virtual ItemRef			GetReferenceFromID(const AEDesc* containerToken, ItemID itemID) { return kNoItemRef; }

	virtual ItemID			GetItemIDFromToken(const AEDesc* token) { return kNoItemID; }
	virtual void			SetItemIDInCoreToken(const AEDesc* containerToken, CoreTokenRecord* tokenRecord, ItemID itemID) {}
	virtual void			SetNamedItemIDInCoreToken(const AEDesc* containerToken, CoreTokenRecord* token, const char *itemName) = 0;

};


// base class for items that cannot be referred to by name
class AEUnnamedClassIterator : public AEClassIterator
{
public:

	enum
	{
		eNoNameKeyFormSupport = (eHasFormAbsolutePosition | eHasFormRelativePosition | eHasFormRange)
	};
	

						AEUnnamedClassIterator(DescType classType)
						: AEClassIterator(classType, eNoNameKeyFormSupport)
						{}
						
						~AEUnnamedClassIterator() {}

protected:

	virtual ItemRef			GetNamedItemReference(const AEDesc* containerToken, const char *itemName) { ThrowOSErr(errAEEventNotHandled); return 0; }
	virtual ItemID			GetNamedItemID(const AEDesc* containerToken, const char *itemName) { ThrowOSErr(errAEEventNotHandled); return 0; }
	virtual void			GetIndexedItemName(const AEDesc* containerToken, TAEListIndex itemIndex, char *outName, long maxLen) { ThrowOSErr(errAEEventNotHandled); }

};


#endif /* __AECLASSITERATOR__ */
