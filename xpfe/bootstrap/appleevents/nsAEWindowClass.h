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


#ifndef __AEWINDOWCLASS__
#define __AEWINDOWCLASS__



#include "nsAEGenericClass.h"


class AEWindowIterator : public AEClassIterator
{
public:

						AEWindowIterator(DescType classType, TWindowKind windowKind)
						: AEClassIterator(classType)
						,	mWindowKind(windowKind)
						{}
						
	virtual UInt32			GetNumItems(const AEDesc* containerToken);
	virtual ItemRef			GetNamedItemReference(const AEDesc* containerToken, const char *itemName);
	virtual ItemRef			GetIndexedItemReference(const AEDesc* containerToken, TAEListIndex itemIndex);
	
	virtual TAEListIndex		GetIndexFromItemID(const AEDesc* containerToken, ItemID itemID);

	virtual ItemID			GetNamedItemID(const AEDesc* containerToken, const char *itemName);
	virtual ItemID			GetIndexedItemID(const AEDesc* containerToken, TAEListIndex itemIndex);

	// index to name
	virtual void			GetIndexedItemName(const AEDesc* containerToken, TAEListIndex itemIndex, char *outName, long maxLen);

	// conversion routines.
	virtual ItemID			GetIDFromReference(const AEDesc* containerToken, ItemRef itemRef);
	virtual ItemRef			GetReferenceFromID(const AEDesc* containerToken, ItemID itemID);

	virtual ItemID			GetItemIDFromToken(const AEDesc* token);
	virtual void			SetItemIDInCoreToken(const AEDesc* containerToken, CoreTokenRecord* tokenRecord, ItemID itemID);
	
	TWindowKind			GetThisWindowKind() { return mWindowKind; }

protected:
	
	TWindowKind			mWindowKind;
};



class AEWindowClass : public AEGenericClass
{
	friend class AECoreClass;

private:
	typedef AEGenericClass	Inherited;
	
protected:
	// only the AECoreClass can instantiate us
						AEWindowClass(DescType classType, TWindowKind windowKind);
						~AEWindowClass();


	void 					GetDocumentFromWindow(	DescType			desiredClass,		// cWindow
											const AEDesc*		containerToken,	// null container
											DescType			containerClass,  	 // cApplication
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken);		// specified Window is returned in result

	virtual void			GetItemFromContainer(	DescType			desiredClass,
											const AEDesc*		containerToken,
											DescType			containerClass, 
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken);

	virtual void 			CompareObjects(		DescType			comparisonOperator,
											const AEDesc *		object,
											const AEDesc *		descriptorOrObject,
											Boolean *			result);

	virtual void 			CountObjects(			DescType 		 	desiredType,
											DescType 		 	containerClass,
											const AEDesc *		container,
							   				long *			result);

public:
	// AE Handlers
	
	static pascal OSErr		DocumentAccessor(		DescType			desiredClass,		// cDocument
											const AEDesc*		containerToken,	// null container
											DescType			containerClass,  	 // cApplication
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken,		// specified Document is returned in result
											long 				refCon);

protected:

	// ----------------------------------------------------------------------------
	//	Core Suite Object Event handlers
	// ----------------------------------------------------------------------------
	virtual void			HandleClose(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDataSize(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDelete(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDuplicate(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleExists(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleMake(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleMove(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleOpen(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandlePrint(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleQuit(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleSave(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	
	// ----------------------------------------------------------------------------
	//	Methods for creating self and container specifiers
	// ----------------------------------------------------------------------------

	virtual void			CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier);

	// ----------------------------------------------------------------------------
	//	Get and Set methods for objects and list
	// ----------------------------------------------------------------------------

	virtual void			GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data);
	virtual void			SetDataForObject(const AEDesc *token, AEDesc *data);

	virtual Boolean			CanSetProperty(DescType propertyCode);
	virtual Boolean			CanGetProperty(DescType propertyCode);

	void					SetWindowProperties(WindowPtr wind, const AEDesc *propertyRecord);
	void					MakeWindowObjectSpecifier(WindowPtr wind, AEDesc *outSpecifier);
	
	WindowPtr			GetWindowByIndex(TWindowKind windowKind, long index);
	WindowPtr			GetWindowByTitle(TWindowKind windowKind, ConstStr63Param title);
	
	long					GetWindowIndex(TWindowKind windowKind, WindowPtr window);
	WindowPtr			GetPreviousWindow(TWindowKind windowKind, WindowPtr wind);

	TWindowKind			GetThisWindowKind()		{ return mWindowKind; }
	
public:

	static long				CountWindows(TWindowKind windowKind);

protected:

	TWindowKind			mWindowKind;

protected:

	OSLAccessorUPP		mDocumentAccessor;
	
};


#endif /* __AEWINDOWCLASS__ */


