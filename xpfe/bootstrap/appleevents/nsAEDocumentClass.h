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


#ifndef __AEDOCUMENTCLASS__
#define __AEDOCUMENTCLASS__

#include "nsAEGenericClass.h"


typedef WindowPtr	DocumentReference;		// for now, we'll refer to document by their window


class AEDocumentClass : public AEGenericClass
{
	friend class AECoreClass;
	
private:
	typedef AEGenericClass	Inherited;
	
protected:
	// only the AECoreClass can instantiate us
						AEDocumentClass();
						~AEDocumentClass();

	void					GetDocumentFromApp(		DescType			desiredClass,		// cDocument
												const AEDesc*		containerToken,	// null container
												DescType			containerClass,  	 // cApplication
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken);		// specified Document is returned in result

	virtual void			GetItemFromContainer(		DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass, 
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken);

public:

	static pascal OSErr		DocumentAccessor(			DescType			desiredClass,		// cDocument
												const AEDesc*		containerToken,	// null container
												DescType			containerClass,  	// cApplication
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken,		// specified Document is returned in result
												long 				refCon);

	static pascal OSErr		PropertyAccessor(			DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass,
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken,
												long 				refCon);

	void					ProcessFormRelativePostition(	const AEDesc* 		anchorToken,
												const AEDesc*		keyData,
												DocumentReference*	document);

	void					CloseWindowSaving(			AEDesc*			token,
												const AEDesc*		saving,
												AEDesc*			savingIn);
	
protected:

	// ----------------------------------------------------------------------------
	//	Get and Set methods for objects and list. Note that the object methods are pure virtual
	// ----------------------------------------------------------------------------
	virtual void			GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data);
	void					GetDataFromList(AEDesc *srcList, AEDesc *desiredTypes, AEDesc *dstList);

	virtual void			SetDataForObject(const AEDesc *token, AEDesc *data);
	void 					SetDataForList(const AEDesc *token, AEDesc *data);

	virtual void			CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier);

	virtual Boolean			CanSetProperty(DescType propertyCode);
	virtual Boolean			CanGetProperty(DescType propertyCode);


	void					SetDocumentProperties(DocumentReference document, AEDesc *propertyRecord);

	DocumentReference		GetDocumentReferenceFromToken(const AEDesc *token);		// these can all throw
	DocumentReference		GetDocumentByName(ConstStr255Param docName);
	DocumentReference		GetDocumentByIndex(long index);
	DocumentReference		GetDocumentByID(long docID);
	
	DocumentReference		GetNextDocument(DocumentReference docRef);
	DocumentReference		GetPreviousDocument(DocumentReference docRef);

public:
	static long				CountDocuments();

};


long		GetDocumentID(DocumentReference docRef);
long		GetDocumentIndex(DocumentReference docRef);
void		GetDocumentName(DocumentReference docRef, Str63 docName);
Boolean	DocumentIsModified(DocumentReference docRef);


#endif /* __AEDOCUMENTCLASS__ */
