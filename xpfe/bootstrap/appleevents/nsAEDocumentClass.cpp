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
#include "nsAEApplicationClass.h"

#include "nsAEDocumentClass.h"


/*----------------------------------------------------------------------------
	AEDocumentClass 
	
----------------------------------------------------------------------------*/
AEDocumentClass::AEDocumentClass()
:	AEGenericClass(cDocument, typeNull)
{
}


/*----------------------------------------------------------------------------
	~AEDocumentClass 
	
----------------------------------------------------------------------------*/
AEDocumentClass::~AEDocumentClass()
{
}

#pragma mark -

/*----------------------------------------------------------------------------
	PropertyFromApplicationAccessor 
	
----------------------------------------------------------------------------*/
pascal OSErr AEDocumentClass::PropertyAccessor(		DescType			desiredClass,
											const AEDesc*		containerToken,
											DescType			containerClass,
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken,
											long 				refCon)
{
	AEDocumentClass*	docClass = reinterpret_cast<AEDocumentClass *>(refCon);
	if (!docClass) return paramErr;
	
	OSErr		err = noErr;
	
	try
	{
		docClass->GetProperty(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
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

/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
	Not appropriate for the application
----------------------------------------------------------------------------*/
void AEDocumentClass::GetItemFromContainer(		DescType			desiredClass,
										const AEDesc*		containerToken,
										DescType			containerClass, 
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)
{
	ThrowIfOSErr(errAEEventNotHandled);
}

/*----------------------------------------------------------------------------
	GetDocumentFromApp 
	
----------------------------------------------------------------------------*/
void AEDocumentClass::GetDocumentFromApp(		DescType			desiredClass,		// cDocument
										const AEDesc*		containerToken,	// null container
										DescType			containerClass,  	 // cApplication
										DescType			keyForm,
										const AEDesc*		keyData,
										AEDesc*			resultToken)		// specified Document is returned in result
{
	OSErr				err			= noErr;
	DescType				keyDataType 	= keyData->descriptorType;
	long					index;
	long					numItems;
	Boolean				wantsAllItems	= false;
	StAEDesc 				startObject;	// These are used to resolve formRange
	StAEDesc 				stopObject;
	DocumentReference		document;
	Str63				documentName;
	CoreTokenRecord 		token;
	
	numItems = CountDocuments();

	switch (keyForm) 
	{
		case formName:											// Document by name
			{
				if (DescToPString(keyData, documentName, 63) != noErr)
				{
					err = errAECoercionFail;
				}
				else
				{
					document = GetDocumentByName(documentName);
					if (document == nil)
						err = errAENoSuchObject;
				}
			}
			break;
		
		case formAbsolutePosition:									// Document by number
			err = NormalizeAbsoluteIndex(keyData, &index, numItems, &wantsAllItems);
			if ((err == noErr) && (wantsAllItems == false))
			{
				document = GetDocumentByIndex(index);
				if (document == nil)
					err = errAEIllegalIndex;
			}
			break;	
		
		case formRelativePosition:
			ProcessFormRelativePostition(containerToken, keyData, &document);
			break;	
		
		case formRange:
			switch (keyDataType)
			{		
				case typeRangeDescriptor:
					err = ProcessFormRange((AEDesc *)keyData, &startObject, &stopObject);
					if (err == noErr)
					{
						ConstAETokenDesc	startTokenDesc(&startObject);
						ConstAETokenDesc	stopTokenDesc(&startObject);
						
						DescType startType = startTokenDesc.GetDispatchClass();
						DescType stopType  = stopTokenDesc.GetDispatchClass();
	 
						if (startType != cDocument || stopType != cDocument)
							err = errAEWrongDataType;
					}
					break;

				default:
					err = errAEWrongDataType;
					break;	
			}
			break;	

		default:
			err = errAEEventNotHandled;
			break;
	}
	
	// if user asked for all items, and there aren't any,
	// we'll be kind and return an empty list.

	if (wantsAllItems && (err == errAENoSuchObject || err == errAEIllegalIndex))
	{
		err = AECreateList(NULL, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
		return;
	}

	ThrowIfOSErr(err);
	
	// fill in the result token

	token.dispatchClass  	= GetClass();
	token.objectClass	= GetClass();
	token.propertyCode 	= typeNull;

	if (wantsAllItems)
	{						
		err = AECreateList(NULL, 0, false, (AEDescList*)resultToken);
		
		if (err == noErr)
		{
			for (index = 1; index <= numItems; index++)
			{
				document = GetDocumentByIndex(index);
				ThrowIfOSErr(errAEEventNotHandled);
				
				token.documentID = GetDocumentID(document);
				
				err = AEPutPtr(resultToken, 0, desiredClass, &token, sizeof(token));
				ThrowIfOSErr(err);
			}
		}
	}
	else if (keyForm == formRange)
	{			
		DocumentReference	beginDocument;
		DocumentReference	endDocument;
		long				beginIndex;
		long				endIndex;
		
		beginDocument = GetDocumentReferenceFromToken(&startObject);
		beginIndex = GetDocumentIndex(beginDocument);

		endDocument = GetDocumentReferenceFromToken(&stopObject);
		endIndex = GetDocumentIndex(endDocument);
								
		err = AECreateList(NULL, 0, false, (AEDescList*)resultToken);
		ThrowIfOSErr(err);
			
		if (beginIndex > endIndex) // swap elements
		{
			DocumentReference temp;
			temp			= beginDocument;
			beginDocument	= endDocument;
			endDocument	= temp;
		}
		
		document = beginDocument;
		while (document != nil)
		{
			token.documentID = GetDocumentID(document);
			
			err = AEPutPtr(resultToken, 0, desiredClass, &token, sizeof(token));
			ThrowIfOSErr(err);

			if (document == endDocument)
				break;
			document = GetNextDocument(document);
		}
	}
	else
	{
		token.documentID = GetDocumentID(document);
		err = AECreateDesc(desiredClass, &token, sizeof(token), resultToken);
	}

	ThrowIfOSErr(err);		
}


/*----------------------------------------------------------------------------
	DocumentAccessor 
	
	Document from null accessor
----------------------------------------------------------------------------*/
pascal OSErr AEDocumentClass::DocumentAccessor(		DescType			desiredClass,		// cDocument
											const AEDesc*		containerToken,	// null container
											DescType			containerClass,  	 // cApplication
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken,		// specified Document is returned in result
											long 				refCon)
{
	AEDocumentClass*		docClass = reinterpret_cast<AEDocumentClass *>(refCon);
	if (!docClass) return paramErr;
	
	OSErr		err = noErr;
	
	try
	{
		docClass->GetDocumentFromApp(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
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
	ProcessFormRelativePostition 
	
----------------------------------------------------------------------------*/
void AEDocumentClass::ProcessFormRelativePostition(const AEDesc* anchorToken, const AEDesc *keyData, DocumentReference *document)
{
	ConstAETokenDesc		tokenDesc(anchorToken);
	OSErr			err = noErr;
	DescType 			positionEnum;
	DocumentReference	anchorDocument;
	DocumentReference	relativeDocument = nil;
	
	*document = nil;
	
	anchorDocument = GetDocumentReferenceFromToken(anchorToken);

	if (err == noErr)
	{
	
		switch (keyData->descriptorType)
		{
		   case typeEnumerated:
				if (DescToDescType((AEDesc*)keyData, &positionEnum) != noErr)
				{
					err = errAECoercionFail;
				}
				else
				{
					switch (positionEnum)
					{
						case kAENext:						// get the document behind the anchor
							*document = GetPreviousDocument(anchorDocument);
							if (*document == nil)
								err = errAENoSuchObject;
							break;
							
						case kAEPrevious:					// get the document in front of the anchor
							*document = GetNextDocument(anchorDocument);
							if (*document == nil)
								err = errAENoSuchObject;
							break;
							
						default:
							err = errAEEventNotHandled;
							break;
					}
				}
				break;
				
			default:
				err = errAECoercionFail;
				break;
		}
	}
	
	ThrowIfOSErr(err);
}

#pragma mark -

/*----------------------------------------------------------------------------
	CanGetProperty 
	
----------------------------------------------------------------------------*/
Boolean AEDocumentClass::CanGetProperty(DescType property)
{
	Boolean	result = false;
	
	switch (property)
	{
		// Properties we can get:
		
		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
		
		case pName:
		case pProperties:
		case pIsModified:
			result = true;
			break;
			
		// Properties we can't get:
		default:
			result = Inherited::CanGetProperty(property);
			break;
	}

	return result;
}


/*----------------------------------------------------------------------------
	CanSetProperty 
	
----------------------------------------------------------------------------*/
Boolean AEDocumentClass::CanSetProperty(DescType property)
{
	Boolean	result = false;
	
	switch (property)
	{
		// Properties we can set:
		
		case pName:
			result = true;
			break;
			
		// Properties we can't set:

		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
		
		case pProperties:
		case pIsModified:
			result = false;
			break;
			
		default:
			result = Inherited::CanSetProperty(property);
			break;
	}

	return result;
}


#pragma mark -


/*----------------------------------------------------------------------------
	GetDataFromObject 
	
----------------------------------------------------------------------------*/
void AEDocumentClass::GetDataFromObject(const AEDesc *token, AEDesc *desiredTypes, AEDesc *data)
{
	OSErr			err				= noErr;
	Boolean			usePropertyCode	= false;	
	DocumentReference	document 			= nil;
	ConstAETokenDesc		tokenDesc(token);
	DescType			aType			= cDocument;
	Str63 			documentName;
	Boolean 			isModified;
	
	// ugh
	document = GetDocumentReferenceFromToken(token);
	ThrowIfOSErr(err);
	if (document == nil)
		ThrowIfOSErr(paramErr);

	GetDocumentName(document, documentName);						
	isModified = DocumentIsModified(document);						

	usePropertyCode = tokenDesc.UsePropertyCode();
	
	DescType propertyCode = tokenDesc.GetPropertyCode();
	
	switch (propertyCode)
	{
		case pProperties:
			err = AECreateList(NULL, 0L, true, data);
			ThrowIfOSErr(err);

			err = AEPutKeyPtr(data, pObjectType,	typeType, 	&aType, 		 	sizeof(DescType));
			err = AEPutKeyPtr(data, pName, 		typeChar, 	&documentName[1], 	documentName[0]);
			err = AEPutKeyPtr(data, pIsModified, 	typeBoolean, 	&isModified, 		sizeof(Boolean));
			break;
			
		case pBestType:
		case pClass:
		case pDefaultType:
		case pObjectType:
			err = AECreateDesc(typeType, &aType, sizeof(DescType), data);
			break;
			
		case pName:
			err = AECreateDesc(typeChar, &documentName[1], documentName[0], data);
			break;
			
		case pIsModified:
			err = AECreateDesc(typeBoolean, &isModified, sizeof(Boolean), data);
			break;
			
		default:
			Inherited::GetDataFromObject(token, desiredTypes, data);
			break;
	}
	
	ThrowIfOSErr(err);
}

/*----------------------------------------------------------------------------
	SetDataForObject 
	
----------------------------------------------------------------------------*/
void AEDocumentClass::SetDataForObject(const AEDesc *token, AEDesc *data)
{
	OSErr				err = noErr;		
	Boolean				usePropertyCode;
	DescType				propertyCode;
	DocumentReference 		document = nil;
	ConstAETokenDesc			tokenDesc(token);
	StAEDesc 				propertyRecord;

	usePropertyCode = tokenDesc.UsePropertyCode();
	document = GetDocumentReferenceFromToken(token);
		
	if (usePropertyCode == false)
	{
		err = errAEWriteDenied;
	}
	else
	{
		propertyCode = tokenDesc.GetPropertyCode();
		
		if (data->descriptorType == typeAERecord)
		{		
			SetDocumentProperties(document, data);
		}
		else	// Build a record with one property
		{
			err = AECreateList(NULL, 0L, true, &propertyRecord);
			ThrowIfOSErr(err);
			
			err = AEPutKeyDesc(&propertyRecord, propertyCode, data);
			ThrowIfOSErr(err);
		
			SetDocumentProperties(document, &propertyRecord);
		}
	}

	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	SetDocumentProperties 
	
----------------------------------------------------------------------------*/
void	AEDocumentClass::SetDocumentProperties(DocumentReference document, AEDesc *propertyRecord)
{

}


/*----------------------------------------------------------------------------
	CountDocuments 
	
----------------------------------------------------------------------------*/
long AEDocumentClass::CountDocuments()
{
	return 0;
}


/*----------------------------------------------------------------------------
	GetDocumentByName 
	
----------------------------------------------------------------------------*/
DocumentReference  AEDocumentClass::GetDocumentByName(ConstStr255Param docName)
{
	return nil;
}

/*----------------------------------------------------------------------------
	GetDocumentByIndex 
	
----------------------------------------------------------------------------*/
DocumentReference  AEDocumentClass::GetDocumentByIndex(long index)
{
	return nil;
}

/*----------------------------------------------------------------------------
	GetDocumentByID 
	
----------------------------------------------------------------------------*/
DocumentReference AEDocumentClass::GetDocumentByID(long docID)
{
	return nil;
}

/*----------------------------------------------------------------------------
	GetNextDocument 
	
----------------------------------------------------------------------------*/
DocumentReference AEDocumentClass::GetNextDocument(DocumentReference docRef)
{
	return nil;
}


/*----------------------------------------------------------------------------
	GetPreviousDocument 
	
----------------------------------------------------------------------------*/
DocumentReference AEDocumentClass::GetPreviousDocument(DocumentReference docRef)
{
	return nil;
}


/*----------------------------------------------------------------------------
	GetDocumentReferenceFromToken 
	
----------------------------------------------------------------------------*/
DocumentReference AEDocumentClass::GetDocumentReferenceFromToken(const AEDesc *token)
{
	ConstAETokenDesc	tokenDesc(token);
	long			docID = tokenDesc.GetDocumentID();
	
	return GetDocumentByID(docID);
}

/*----------------------------------------------------------------------------
	CloseWindowSaving 
	
----------------------------------------------------------------------------*/
void AEDocumentClass::CloseWindowSaving(AEDesc *token, const AEDesc *saving, AEDesc *savingIn)
{
	OSErr			err			= noErr;
	DocumentReference	document;

	document = GetDocumentReferenceFromToken(token);
	
	if (document != nil)
	{
		// DestroyDocument(document);
	}
	else
		err = errAEEventNotHandled;
	
	ThrowIfOSErr(err);
}

#pragma mark -

/*----------------------------------------------------------------------------
	CreateSelfSpecifier 

----------------------------------------------------------------------------*/
void AEDocumentClass::CreateSelfSpecifier(const AEDesc *token, AEDesc *outSpecifier)
{
	ThrowIfOSErr(errAENoSuchObject);
}

#pragma mark -

/*----------------------------------------------------------------------------
	GetDocumentID 
	
----------------------------------------------------------------------------*/
long GetDocumentID(DocumentReference docRef)
{
	return 0;
}

	
/*----------------------------------------------------------------------------
	GetDocumentIndex 
	
----------------------------------------------------------------------------*/
long GetDocumentIndex(DocumentReference docRef)
{
	return 0;
}

/*----------------------------------------------------------------------------
	GetDocumentIndex 
	
----------------------------------------------------------------------------*/
void	GetDocumentName(DocumentReference docRef, Str63 docName)
{
	docName[0] = 0;
}


/*----------------------------------------------------------------------------
	GetDocumentIndex 
	
----------------------------------------------------------------------------*/
Boolean DocumentIsModified(DocumentReference docRef)
{
	return false;
}

