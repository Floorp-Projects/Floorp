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


#ifndef __AEGENERICCLASS__
#define __AEGENERICCLASS__


#include "nsAEClassIterator.h"

// pure virtual base class that provides stubs for much objecct functionality.

class AEGenericClass
{
	friend class AEDispatchHandler;
public:
	
						AEGenericClass(DescType classType, DescType containerClass);			// throws OSErrs
	virtual				~AEGenericClass();


	// ----------------------------------------------------------------------------
	//	Dispatch routine
	// ----------------------------------------------------------------------------
	void					DispatchEvent(					AEDesc*			token, 
													const AppleEvent*	appleEvent,
													AppleEvent*		reply);	// throws OSErrs

	// ----------------------------------------------------------------------------
	//	Item from container accessor
	// ----------------------------------------------------------------------------
	static pascal OSErr		ItemFromContainerAccessor(		DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass, 
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken,
													long 				refCon);

protected:	
	
	// MT-NW suite events
	enum {
		kAEExtract			= 'Extr',
		kExtractKeyDestFolder	= 'Fold',
		
		kAESendMessage		= 'Post'
	};
	
	
	virtual void			GetPropertyFromObject(			DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass,
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);

	virtual void			GetProperty(					DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass,
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);
	
	// ----------------------------------------------------------------------------
	//	GetItemFromContainer
	//		
	//	Overridable method to get an item from its container.
	//		
	// ----------------------------------------------------------------------------
	virtual void			GetItemFromContainer(			DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass, 
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken) = 0;
										
	// ----------------------------------------------------------------------------
	//	OSL Callbacks (compare and count)
	// ----------------------------------------------------------------------------
	virtual void 			CompareObjects(				DescType			comparisonOperator,
													const AEDesc *		object,
													const AEDesc *		descriptorOrObject,
													Boolean *			result);

	virtual void 			CountObjects(					DescType 		 	desiredType,
													DescType 		 	containerClass,
													const AEDesc *		container,
									   				long *			result);

	// ----------------------------------------------------------------------------
	//	Core Suite Object Event handlers
	// ----------------------------------------------------------------------------
	virtual void			HandleClose(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleCount(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleSetData(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleGetData(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDataSize(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDelete(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleDuplicate(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleExists(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleMake(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleMove(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleOpen(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleRun(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandlePrint(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleQuit(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleSave(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);

	// ----------------------------------------------------------------------------
	//	MT-NW suite event handlers
	// ----------------------------------------------------------------------------
	virtual void			HandleExtract(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	virtual void			HandleSendMessage(AEDesc *token, const AppleEvent *appleEvent, AppleEvent *reply);
	
	// ----------------------------------------------------------------------------
	//	Get and Set methods for objects and list. Note that the object methods are pure virtual
	// ----------------------------------------------------------------------------
	
	virtual void			GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data) = 0;
	virtual void			SetDataForObject(const AEDesc *token, AEDesc *data) = 0;

	// ----------------------------------------------------------------------------
	//	Methods for creating self and container specifiers
	// ----------------------------------------------------------------------------

	void					CreateSpecifier(const AEDesc *token, AEDesc *outSpecifier);
	virtual void			GetContainerSpecifier(const AEDesc *token, AEDesc *outContainerSpecifier);

	virtual void			CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier) = 0;

	void					GetDataFromListOrObject(const AEDesc *tokenOrTokenList, AEDesc *desiredTypes, AEDesc *data);
	void					SetDataForListOrObject(const AEDesc *tokenOrTokenList, const AppleEvent *appleEvent, AppleEvent *reply);

	void					GetDataFromList(const AEDesc *srcList, AEDesc *desiredTypes, AEDesc *dstList);
	void 					SetDataForList(const AEDesc *token, AEDesc *data);

	void					GetPropertyFromListOrObject(		DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass,
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);
	
	void					GetPropertyFromList(			DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass,
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);

	virtual DescType		GetKeyEventDataAs(DescType propertyCode);
	
	virtual Boolean			CanSetProperty(DescType propertyCode) = 0;
	virtual Boolean			CanGetProperty(DescType propertyCode) = 0;

	DescType				GetClass()				{ return mClass; }


	// ----------------------------------------------------------------------------
	//	Methods called from the main handlers
	// ----------------------------------------------------------------------------

	virtual void			MakeNewObject(				const DescType		insertionPosition,
													const AEDesc*		token,
													const AEDesc*		ptrToWithData, 
													const AEDesc*		ptrToWithProperties,
													AppleEvent*		reply);


protected:

	DescType				mClass;
	DescType				mContainerClass;
	
	OSLAccessorUPP		mItemFromContainerAccessor;

};


#endif /* __AEGENERICCLASS__ */

