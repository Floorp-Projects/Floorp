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

#include "nsAEClassTypes.h"
#include "nsAEUtils.h"
#include "nsAETokens.h"
#include "nsAECompare.h"

#include "nsAEApplicationClass.h"
#include "nsAEDocumentClass.h"
#include "nsAEWindowClass.h"

/*
#include "nsAETextClass.h"
#include "nsAEWordClass.h"
#include "nsAECharacterClass.h"
*/

#include "nsAECoreClass.h"

AECoreClass*		AECoreClass::sAECoreHandler = nil;

/*----------------------------------------------------------------------------
	AECoreClass 
	
----------------------------------------------------------------------------*/

AECoreClass::AECoreClass(Boolean suspendEvents)
:	mSuspendEventHandlerUPP(nil)
,	mStandardSuiteHandlerUPP(nil)
,	mRequiredSuiteHandlerUPP(nil)
,	mCreateElementHandlerUPP(nil)
,	mMozillaSuiteHandlerUPP(nil)
,	mGetURLSuiteHandlerUPP(nil)
,	mSpyGlassSuiteHandlerUPP(nil)
,	mPropertyFromListAccessor(nil)
,	mAnythingFromAppAccessor(nil)
,	mCountItemsCallback(nil)
,	mCompareItemsCallback(nil)
{
	mSuspendedEvent.descriptorType = typeNull;
	mSuspendedEvent.dataHandle = nil;

	mReplyToSuspendedEvent.descriptorType = typeNull;
	mReplyToSuspendedEvent.dataHandle = nil;
	
	OSErr	err = ::AEObjectInit();
	ThrowIfOSErr(err);
	
	mSuspendEventHandlerUPP = NewAEEventHandlerProc(AECoreClass::SuspendEventHandler);
	ThrowIfNil(mSuspendEventHandlerUPP);

	mRequiredSuiteHandlerUPP = NewAEEventHandlerProc(AECoreClass::RequiredSuiteHandler);
	ThrowIfNil(mRequiredSuiteHandlerUPP);

	mStandardSuiteHandlerUPP = NewAEEventHandlerProc(AECoreClass::CoreSuiteHandler);
	ThrowIfNil(mStandardSuiteHandlerUPP);

	mMozillaSuiteHandlerUPP = NewAEEventHandlerProc(AECoreClass::MozillaSuiteHandler);
	ThrowIfNil(mMozillaSuiteHandlerUPP);

	mGetURLSuiteHandlerUPP = NewAEEventHandlerProc(AECoreClass::GetURLSuiteHandler);
	ThrowIfNil(mGetURLSuiteHandlerUPP);

	mSpyGlassSuiteHandlerUPP = NewAEEventHandlerProc(AECoreClass::SpyglassSuiteHandler);
	ThrowIfNil(mSpyGlassSuiteHandlerUPP);

	InstallSuiteHandlers(suspendEvents);

	mCreateElementHandlerUPP = NewAEEventHandlerProc(AECoreClass::CreateElementHandler);
	ThrowIfNil(mCreateElementHandlerUPP);
	
	// Special handler for StandardSuite Make (CreateElement) event
	// Make (CreateElement) is different than the other events processed above
	// because it passes its ospec in the insertionLoc parameter instead of
	// in the direct object parameter.

	err = ::AEInstallEventHandler(kAECoreSuite, 	kAECreateElement, 
										mCreateElementHandlerUPP, 
										(long)this, 
										false);
	ThrowIfOSErr(err);

	/*	We'd like to be able to install a generic handler that is used to get anything from null.
		Unfortunately, some formRelative requests require a handler for getting an item from
		another of the same type (e.g. "get the window after window 'foo'" requires a cWindow
		from cWindow handler).
	*/
	// Install a generic handler to get an item from the app
	mAnythingFromAppAccessor = NewOSLAccessorProc(AECoreClass::AnythingFromAppAccessor);
	ThrowIfNil(mAnythingFromAppAccessor);
	
	// Install a handler to get properties from anything.
	err = ::AEInstallObjectAccessor(typeWildCard, typeWildCard, mAnythingFromAppAccessor, (long)this, false);
	ThrowIfOSErr(err);
	
	// Install a generic handler to get a property from a typeAEList of tokens
	mPropertyFromListAccessor = NewOSLAccessorProc(AECoreClass::PropertyTokenFromAnything);
	ThrowIfNil(mPropertyFromListAccessor);
	
	// Install a handler to get properties from anything.
	err = ::AEInstallObjectAccessor(cProperty, typeWildCard, mPropertyFromListAccessor, (long)this, false);
	ThrowIfOSErr(err);

	// Install the OSL object callbacks, use for compare and count
	mCountItemsCallback = NewOSLCountProc(AECoreClass::CountObjectsCallback);
	ThrowIfNil(mCountItemsCallback);

	mCompareItemsCallback = NewOSLCompareProc(AECoreClass::CompareObjectsCallback);
	ThrowIfNil(mCompareItemsCallback);
	
	err = ::AESetObjectCallbacks(
						mCompareItemsCallback,
						mCountItemsCallback,
						(OSLDisposeTokenUPP)	nil, 
						(OSLGetMarkTokenUPP)	nil, 
						(OSLMarkUPP)			nil, 
						(OSLAdjustMarksUPP)	nil, 
						(OSLGetErrDescUPP)  	nil);
	ThrowIfOSErr(err);

	// create the dispatchers for various object types. This should be the only place
	// that new classes have to be added
	AEApplicationClass*		appDispatcher = new AEApplicationClass;
	RegisterClassHandler(cApplication, appDispatcher);
	RegisterClassHandler(typeNull, appDispatcher, true);

	AEDocumentClass*		docDispatcher = new AEDocumentClass;
	RegisterClassHandler(cDocument, docDispatcher);

	AEWindowClass*		windowDispatcher = new AEWindowClass(cWindow, kAnyWindowKind);
	RegisterClassHandler(cWindow, windowDispatcher);

/*	
	AETextClass*		textDispatcher = new AETextClass;
	RegisterClassHandler(AETextClass::cTEText, textDispatcher);
	
	AEWordClass*		wordDispatcher = new AEWordClass;
	RegisterClassHandler(cWord, wordDispatcher);
	
	AECharacterClass*	charDispatcher = new AECharacterClass;
	RegisterClassHandler(cChar, charDispatcher);
*/
}


/*----------------------------------------------------------------------------
	~AECoreClass 
	
----------------------------------------------------------------------------*/

AECoreClass::~AECoreClass()
{
	if (mSuspendEventHandlerUPP)
		DisposeAEEventHandlerUPP(mSuspendEventHandlerUPP);

	if (mStandardSuiteHandlerUPP)
		DisposeAEEventHandlerUPP(mStandardSuiteHandlerUPP);
		
	if (mRequiredSuiteHandlerUPP)
		DisposeAEEventHandlerUPP(mRequiredSuiteHandlerUPP);
			
	if (mMozillaSuiteHandlerUPP)
		DisposeAEEventHandlerUPP(mMozillaSuiteHandlerUPP);

	if (mGetURLSuiteHandlerUPP)
		DisposeAEEventHandlerUPP(mGetURLSuiteHandlerUPP);

	if (mSpyGlassSuiteHandlerUPP)
		DisposeAEEventHandlerUPP(mSpyGlassSuiteHandlerUPP);	
	
	if (mCreateElementHandlerUPP)
		DisposeAEEventHandlerUPP(mCreateElementHandlerUPP);
		
	if (mPropertyFromListAccessor)
		DisposeOSLAccessorUPP(mPropertyFromListAccessor);

	if (mAnythingFromAppAccessor)
		DisposeOSLAccessorUPP(mAnythingFromAppAccessor);

	if (mCountItemsCallback)
		DisposeOSLCountUPP(mCountItemsCallback);

	if (mCompareItemsCallback)
		DisposeOSLCompareUPP(mCompareItemsCallback);
}

#pragma mark -


/*----------------------------------------------------------------------------
	HandleCoreSuiteEvent 
	
----------------------------------------------------------------------------*/
void AECoreClass::HandleCoreSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc		directParameter;
	StAEDesc		token;
	OSErr		err = noErr;
	
	// extract the direct parameter (an object specifier)
	err = ::AEGetKeyDesc(appleEvent, keyDirectObject, typeWildCard, &directParameter);
	ThrowIfOSErr(err);

	DescType	tokenType		= typeNull;
	DescType	dispatchClass  	= typeNull;
	
	// check for null descriptor, which AEResolve doesn't handle well
	// If it's not null, then AEResolve will return an application-defined token
	if (directParameter.descriptorType == typeNull) 
	{
		token = directParameter;			// can throw
	}
	else
	{
		// The direct parameter contains an object specifier, or an "reference" in
		// AppleScript terminology, such as "rectangle 1 of document 1". 
		// AEResolve() will recursively call our installed object accessors
		// until it returns a token with data referencing the requested object.
		 
		err = ::AEResolve(&directParameter, kAEIDoMinimum, &token);
	}

	if (err == errAENoSuchObject || err == errAEIllegalIndex)
	{
		// If we were executing an "Exists..." AppleEvent, we can reply it here
		// because we have already determined that it DOES NOT exist.
		// First, we get the event ID. We use "eventError" instead of "err"
		// so that the result of AEGetAttributePtr() does not disturb the
		// errAENoSuchObject result previously returned.
		
		AEEventID		eventID;
		OSType		typeCode;
		Size			actualSize = 0;		
		OSErr		eventError = ::AEGetAttributePtr(appleEvent, 
										  		 keyEventIDAttr, 
										  		 typeType, 
										  		 &typeCode, 
										  		 (Ptr)&eventID, 	// Get the eventID from the AppleEvent
										  		 sizeof(eventID), 
										  		 &actualSize);
										  
		// If event was an "Exists..." message, store the result (false) in the reply
		// because AEResolve() returned errAENoSuchObject.
		
		if (eventError == noErr && eventID == kAEDoObjectsExist)
		{
			Boolean	foundIt = false;
			(void)AEPutParamPtr(reply, keyAEResult, typeBoolean, (Ptr)&foundIt, sizeof(Boolean));
			
			// Now, we set the err to noErr so that the scripting component doesn't complain
			// that the object does not exist. We only do this if we were executing an "Exists..."
			// event. Otherwise, the errAENoSuchObject will be returned.
			ThrowNoErr();
		}
		
		ThrowIfOSErr(err);
		return;
	}

	ThrowIfOSErr(err);
	
	// Pass the token returned by AEResolve(), and the original AppleEvent event and reply,
	// on to the appropriate object dispatcher

	// The token type is, by convention, the same as class ID that handles this event.
	// However, for property tokens, tokenType is cProperty for all objects, so
	// we look inside the token at its dispatchClass to find out which class of object
	// should really handle this AppleEvent.
	
	// Also, if the resolver returned a list of objects in the token, 
	// the type will be typeAEList or one of our special list types, so
	// we set the dispatch class based on the list type
	// instead of on the type of the token itself
	
	if (AEListUtils::TokenContainsTokenList(&token))
	{
		SInt32	numItems;
		
		err = AECountItems(&token, &numItems);
		
		if (numItems == 0)	// could be an empty list
		{
			dispatchClass = typeNull;
		}
		else
		{
			StAEDesc	tempToken;
			err = AEListUtils::GetFirstNonListToken((AEDesc *)&token, &tempToken);
			if (err == noErr && tempToken.descriptorType != typeNull)
			{
				ConstAETokenDesc	tokenDesc(&tempToken);
				dispatchClass = tokenDesc.GetDispatchClass();
			}
			else
			{
				dispatchClass = typeNull;
				err = noErr;
			}
		}
	}
	else if (token.descriptorType == typeNull)	           // make sure we correctly handle things for cApplication that don't have a direct parameter
	{
		dispatchClass = typeNull;
	}
	else
	{
		ConstAETokenDesc	tokenDesc(&token);
		dispatchClass = tokenDesc.GetDispatchClass();
	}
	
	// why is this special-cased?
	if (dispatchClass == cFile)
	{
		AEEventID		eventID	= 0;
		OSType		typeCode 	= 0;
		Size			actualSize	= 0;
		
		OSErr		eventError = AEGetAttributePtr(appleEvent, 	keyEventIDAttr, 
														typeType, 
														&typeCode, 
														(Ptr)&eventID, 	// Get the eventID from the AppleEvent
														sizeof(eventID), 
														&actualSize);										  
	}


	AEDispatchHandler*	handler = GetDispatchHandler(dispatchClass);
	if (!handler)
		ThrowIfOSErr(errAEEventNotHandled);
		
	handler->DispatchEvent(&token, appleEvent, reply);
}


/*----------------------------------------------------------------------------
	HandleRequiredSuiteEvent 
	
----------------------------------------------------------------------------*/
void AECoreClass::HandleRequiredSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc	token;
	
	// ignore error; direct object param might be optional
	(void)::AEGetParamDesc(appleEvent, keyDirectObject, typeAEList, &token);
	
	AEDispatchHandler*	handler = GetDispatchHandler(cApplication);
	if (!handler)
		ThrowIfOSErr(errAEEventNotHandled);
		
	handler->DispatchEvent(&token, appleEvent, reply);
}


/*----------------------------------------------------------------------------
	HandleCreateElementEvent 
	
----------------------------------------------------------------------------*/
void AECoreClass::HandleCreateElementEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	StAEDesc		token;
	OSErr		err = noErr;
	
 	// Extract the type of object we want to create.
 	// We use this to call the event dispatcher for that kind of objects
	err = ::AEGetParamDesc(appleEvent, keyAEObjectClass, typeType, &token);
	ThrowIfOSErr(err);

	DescType	dispatchClass = **(DescType **)(token.dataHandle);

	AEDispatchHandler*	handler = GetDispatchHandler(dispatchClass);
	if (!handler)
		ThrowIfOSErr(errAEEventNotHandled);
		
	handler->DispatchEvent(&token, appleEvent, reply);
}


/*----------------------------------------------------------------------------
	HandleEventSuspend 
	
----------------------------------------------------------------------------*/
void AECoreClass::HandleEventSuspend(const AppleEvent *appleEvent, AppleEvent *reply)
{
	mSuspendedEvent = *appleEvent;
	mReplyToSuspendedEvent = *reply;
	OSErr	err = ::AESuspendTheCurrentEvent(appleEvent);
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	PropertyTokenFromList 
	
----------------------------------------------------------------------------*/
void AECoreClass::PropertyTokenFromList(			DescType			desiredClass,
			 								const AEDesc*		containerToken,
			 								DescType			containerClass,
			 								DescType			keyForm,
		    									const AEDesc*		keyData,
			 								AEDesc*			resultToken)
{
	DescType		handlerClass;
	ConstAETokenDesc	containerDesc(containerToken);
	
	switch (containerClass)
	{
		case cProperty:
			// this is to handle our property from property hack, which enables an article to be a property
			// of an article window, and thence to get properties of that article.
			handlerClass = containerDesc.GetDispatchClass();
			break;

		default:
			handlerClass = containerClass;
			break;
	
	}
	AEDispatchHandler*	handler = GetDispatchHandler(handlerClass);
	if (!handler)
		ThrowIfOSErr(errAEEventNotHandled);
		
	handler->GetProperty(		desiredClass,
							containerToken,
							containerClass,
							keyForm,
							keyData,
							resultToken);
}


/*----------------------------------------------------------------------------
	GetAnythingFromApp 
	
----------------------------------------------------------------------------*/
void AECoreClass::GetAnythingFromApp(				DescType			desiredClass,
			 								const AEDesc*		containerToken,
			 								DescType			containerClass,
			 								DescType			keyForm,
		    									const AEDesc*		keyData,
			 								AEDesc*			resultToken)
{
	AEDispatchHandler*	handler = GetDispatchHandler(desiredClass);
	if (!handler)
		ThrowIfOSErr(errAEEventNotHandled);
		
	handler->GetItemFromContainer(		desiredClass,
									containerToken,
									containerClass,
									keyForm,
									keyData,
									resultToken);
}


#pragma mark -

/*----------------------------------------------------------------------------
	SuspendEventHandler 
	
	This handler receives ALL core suite events EXCEPT Make (CreateElement).
	It simply suspends the current event and returns. Someone has to resume
	this event later, when we are ready to handle it, by calling ResumeEventHandling().

----------------------------------------------------------------------------*/

pascal OSErr AECoreClass::SuspendEventHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleEventSuspend(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	RequiredSuiteHandler 
	
	This handler receives ALL core suite events EXCEPT Make (CreateElement)
	and passes them on to the correct object dispatcher:
	   cApplication, cDocument, cFile, cGraphicObject, and cWindow.
	Make (CreateElement) is different because it passes its ospec in the
	insertionLoc parameter instead of in the direct object parameter.

----------------------------------------------------------------------------*/

pascal OSErr AECoreClass::RequiredSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleRequiredSuiteEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	CoreSuiteHandler 
	
	This handler receives ALL core suite events EXCEPT Make (CreateElement)
	and passes them on to the correct object dispatcher:
	   cApplication, cDocument, cFile, cGraphicObject, and cWindow.
	Make (CreateElement) is different because it passes its ospec in the
	insertionLoc parameter instead of in the direct object parameter.

----------------------------------------------------------------------------*/

pascal OSErr AECoreClass::CoreSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleCoreSuiteEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	CreateElementHandler 
	
----------------------------------------------------------------------------*/

pascal OSErr AECoreClass::CreateElementHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleCreateElementEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}

#pragma mark -

/*----------------------------------------------------------------------------
	MozillaSuiteHandler 
	
----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::MozillaSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleMozillaSuiteEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}


/*----------------------------------------------------------------------------
	GetURLSuiteHandler 
	

----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::GetURLSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleGetURLSuiteEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}

/*----------------------------------------------------------------------------
	SpyGlassSuiteHandler 
	

----------------------------------------------------------------------------*/

pascal OSErr AECoreClass::SpyglassSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon)
{
	AECoreClass*	coreClass = reinterpret_cast<AECoreClass *>(refCon);
	OSErr		err = noErr;
	
	if (coreClass == nil)
		return errAECorruptData;
	
	try
	{
		coreClass->HandleSpyglassSuiteEvent(appleEvent, reply);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch ( ... )
	{
		err = paramErr;
	}
	
	return err;
}


#pragma mark -

/*----------------------------------------------------------------------------
	HandleMozillaSuiteEvent 
	
	We probably want to handle events for this suite off to another class.
----------------------------------------------------------------------------*/
void AECoreClass::HandleMozillaSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	mMozillaSuiteHandler.HandleMozillaSuiteEvent(appleEvent, reply);
}


/*----------------------------------------------------------------------------
	HandleGetURLSuiteEvent 
	
	We probably want to handle events for this suite off to another class.
----------------------------------------------------------------------------*/
void AECoreClass::HandleGetURLSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	mGetURLSuiteHandler.HandleGetURLSuiteEvent(appleEvent, reply);
}


/*----------------------------------------------------------------------------
	HandleSpyGlassSuiteEvent 
	
	We probably want to handle events for this suite off to another class.
----------------------------------------------------------------------------*/
void AECoreClass::HandleSpyglassSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply)
{
	mSpyglassSuiteHandler.HandleSpyglassSuiteEvent(appleEvent, reply);
}


#pragma mark -

/*----------------------------------------------------------------------------
	PropertyTokenFromAnything 
	
----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::PropertyTokenFromAnything(			DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken,
					 								long 				refCon)
{
	AECoreClass*		coreClass = reinterpret_cast<AECoreClass *>(refCon);
	if (!coreClass) return paramErr;
	
	OSErr	err = noErr;
	
	try
	{
		coreClass->PropertyTokenFromList(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch (...)
	{
		err = paramErr;
	}
	
	return err;
}



/*----------------------------------------------------------------------------
	AnythingFromAppAccessor 
	
----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::AnythingFromAppAccessor(			DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken,
					 								long 				refCon)
{
	AECoreClass*		coreClass = reinterpret_cast<AECoreClass *>(refCon);
	
	if (!coreClass)
		return paramErr;
	
	OSErr	err = noErr;
	
	try
	{
		coreClass->GetAnythingFromApp(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch (...)
	{
		err = paramErr;
	}
	
	return err;
}


/*----------------------------------------------------------------------------
	CompareObjectsCallback 
	
----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::CompareObjectsCallback(	DescType			comparisonOperator, 	// operator to use
											const AEDesc *		object,				// left-hand side
											const AEDesc *		descriptorOrObject, 		// right-hand side
											Boolean *			result)
{
	OSErr	err = noErr;
	try
	{
		OSErr		err = noErr;
		StAEDesc		desc1;
		StAEDesc		desc2;

		// This first AEDesc is a token to a specific object, so we resolve it.
		AECoreClass::GetAECoreHandler()->ExtractData(object, &desc1);
			
		// A second AEDesc is either a token to another object or an AEDesc containing literal data.
		AECoreClass::GetAECoreHandler()->ExtractData(descriptorOrObject, &desc2);	

		// Make sure the data type extracted from the second AEDesc is the same as the
		// data specified by the first AEDesc.
		
		if (desc1.descriptorType != desc2.descriptorType) 
		{
			StAEDesc		temp;
			
			// Create a temporary duplicate of desc2 and coerce it to the
			// requested type. This could call a coercion handler you have
			// installed. If there are no errors, stuff the coerced value back into desc2
			err = AEDuplicateDesc(&desc2, &temp);
			ThrowIfOSErr(err);
			
			desc2.Clear();
			
			err = AECoerceDesc(&temp, desc1.descriptorType, &desc2);
			ThrowIfOSErr(err);
		}

		AEDispatchHandler*	handler = AECoreClass::GetDispatchHandlerForClass(desc1.descriptorType);
		if (handler)
			handler->CompareObjects(comparisonOperator, &desc1, &desc2, result);
		else
			*result = AEComparisons::TryPrimitiveComparison(comparisonOperator, &desc1, &desc2);
	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch (...)
	{
		err = paramErr;
	}
	
	return err;
}


/*----------------------------------------------------------------------------
	CountObjectsCallback 
	
----------------------------------------------------------------------------*/
pascal OSErr AECoreClass::CountObjectsCallback(		DescType 		 	desiredType,
											DescType 		 	containerClass,
											const AEDesc *		container,
							   				long *			result)
{
	AEDispatchHandler*	handler = AECoreClass::GetDispatchHandlerForClass(containerClass);
	if (!handler)
		return errAEEventNotHandled;
	
	OSErr	err = noErr;
	try
	{
		handler->CountObjects(				desiredType,
										containerClass,
										container,
										result);

	}
	catch (OSErr catchErr)
	{
		err = catchErr;
	}
	catch (...)
	{
		err = paramErr;
	}
	
	return err;
}

#pragma mark -

/*----------------------------------------------------------------------------
	GetEventKeyDataParameter 
	
	Extract the keyData parameter data from an apple event
---------------------------------------------------------------------------*/
void AECoreClass::GetEventKeyDataParameter(const AppleEvent *appleEvent, DescType requestedType, AEDesc *data)
{
	StAEDesc	keyData;

	OSErr	err = AEGetKeyDesc(appleEvent, keyAEData, requestedType, &keyData);
	ThrowIfOSErr(err);
	
	ExtractData(&keyData, data);
}


/*----------------------------------------------------------------------------
	ExtractData 
	
	ExtractData can receive several types of data:
	Source				 Processing
	-------------------- 	-----------------------------
	an object specifier		call AEResolve() to get token, then handle below
	a property token		call public accessors to get raw data from token
	a object token			if it's not a property token, the token itself is returned
	 raw data				just return it, it's already data!
---------------------------------------------------------------------------*/

void AECoreClass::ExtractData(const AEDesc *source, AEDesc *data)
{
	OSErr		err = noErr;
	StAEDesc		temp;
	DescType		dispatchClass;
		
	if ((source->descriptorType == typeNull) || (source->dataHandle == nil))
		ThrowIfOSErr(errAENoSuchObject);
	
	// If it's an object specifier, resolve into a token
	// Otherwise, just copy it
	
	if (source->descriptorType == typeObjectSpecifier) 
	{
		err = AEResolve(source, kAEIDoMinimum, &temp);
	}
	else
	{
		err = AEDuplicateDesc(source, &temp);
	}
	ThrowIfOSErr(err);
	
	// Next, determine which object should handle it, if any. 
	// If it's a property token, get the dispatch class. 
	// Otherwise, just get the descriptorType. 
	
	if (temp.descriptorType == typeProperty)
	{
		ConstAETokenDesc	tokenDesc(&temp);
		dispatchClass = tokenDesc.GetDispatchClass();
	}
	else
		dispatchClass = temp.descriptorType;
	
	// If it's a property token, get the data it refers to,
	// otherwise duplicate it and return
		
	AEDispatchHandler*	handler = GetDispatchHandler(dispatchClass);
	if (handler)
	{
		/*
		case cApplication:
			// why?
			ThrowIfOSErr(errAEEventNotHandled);
			break;
		*/
		handler->GetDataFromListOrObject(&temp, nil, data);
	}
	else
	{
		err = AEDuplicateDesc(&temp, data);
		ThrowIfOSErr(err);
	}
}

#pragma mark -

/*----------------------------------------------------------------------------
	RegisterClassHandler 
	
	This is where a handler class registers itself
----------------------------------------------------------------------------*/
void AECoreClass::RegisterClassHandler(DescType handlerClass, AEGenericClass* classHandler, Boolean isDuplicate /* = false */)
{
	mDispatchTree.InsertHandler(handlerClass, classHandler, isDuplicate);
}

/*----------------------------------------------------------------------------
	GetDispatchHandler 
	
	Get a dispatch handler for the given class
----------------------------------------------------------------------------*/
AEDispatchHandler* AECoreClass::GetDispatchHandler(DescType dispatchClass)
{
	return mDispatchTree.FindHandler(dispatchClass);
}


/*----------------------------------------------------------------------------
	GetDispatchHandler 
	
	Get a dispatch handler for the given class.
	
	Static
----------------------------------------------------------------------------*/
AEDispatchHandler* AECoreClass::GetDispatchHandlerForClass(DescType dispatchClass)
{
	if (!sAECoreHandler)
		return nil;
	
	return sAECoreHandler->GetDispatchHandler(dispatchClass);
}



/*----------------------------------------------------------------------------
	InstallSuitesGenericHandler 
	
	Install a handler for each of our suites
----------------------------------------------------------------------------*/
void AECoreClass::InstallSuiteHandlers(Boolean suspendEvents)
{
	OSErr	err;

	err = ::AEInstallEventHandler(kCoreEventClass,  	typeWildCard, 
											suspendEvents ? mSuspendEventHandlerUPP : mRequiredSuiteHandlerUPP, 
											(long)this, 
											false);
	ThrowIfOSErr(err);
	
	err = ::AEInstallEventHandler(kAECoreSuite,  		typeWildCard, 
											suspendEvents ? mSuspendEventHandlerUPP : mStandardSuiteHandlerUPP, 
											(long)this, 
											false);
	ThrowIfOSErr(err);
	
	// install the mozilla suite handler
	err = ::AEInstallEventHandler(AEMozillaSuiteHandler::kSuiteSignature,  	typeWildCard, 
											suspendEvents ? mSuspendEventHandlerUPP : mMozillaSuiteHandlerUPP, 
											(long)this,
											false);
	ThrowIfOSErr(err);

	// install the GetURL suite handler
	err = ::AEInstallEventHandler(AEGetURLSuiteHandler::kSuiteSignature,  	typeWildCard, 
											suspendEvents ? mSuspendEventHandlerUPP : mGetURLSuiteHandlerUPP, 
											(long)this, 
											false);
	ThrowIfOSErr(err);
	
	// install the SpyGlass suite handler
	err = ::AEInstallEventHandler(AESpyglassSuiteHandler::kSuiteSignature,  	typeWildCard, 
											suspendEvents ? mSuspendEventHandlerUPP : mSpyGlassSuiteHandlerUPP, 
											(long)this, 
											false);
	ThrowIfOSErr(err);
}


/*----------------------------------------------------------------------------
	HandleCoreSuiteEvent 
	
----------------------------------------------------------------------------*/
void AECoreClass::ResumeEventHandling(const AppleEvent *appleEvent, AppleEvent *reply, Boolean dispatchEvent)
{
	InstallSuiteHandlers(false);

	OSErr	err;

	// now resume the passed in event
	err = ::AEResumeTheCurrentEvent(appleEvent, reply, (AEEventHandlerUPP)(dispatchEvent ? kAEUseStandardDispatch : kAENoDispatch), (long)this);
	ThrowIfOSErr(err);
}
