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



#ifndef __AECORECLASS__
#define __AECORECLASS__

#ifdef __cplusplus


#include <AEDataModel.h>
#include <AppleEvents.h>
#include <AEObjects.h>

#include "nsAEClassDispatcher.h"

#include "nsAEMozillaSuiteHandler.h"
#include "nsAEGetURLSuiteHandler.h"
#include "nsAESpyglassSuiteHandler.h"

class AEApplicationClass;
class AEDocumentClass;
class AEWindowClass;

class AECoreClass
{
public:
	
						AECoreClass(Boolean suspendEvents = false);	// throws OSErrs
						~AECoreClass();

	void					HandleCoreSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);			// throws OSErrs
	void					HandleRequiredSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs

	void					HandleMozillaSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	void					HandleGetURLSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	void					HandleSpyglassSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs

	void					HandleCreateElementEvent(AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	
	void					HandleEventSuspend(AppleEvent *appleEvent, AppleEvent *reply);
	void					ResumeEventHandling(AppleEvent *appleEvent, AppleEvent *reply, Boolean dispatchEvent);
	
	// AE Handlers
	static pascal OSErr		SuspendEventHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);
	static pascal OSErr		RequiredSuiteHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);
	static pascal OSErr		CoreSuiteHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);
	static pascal OSErr		CreateElementHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);

	// Handler for Mozilla Suite events
	static pascal OSErr		MozillaSuiteHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);

	// Handler for GetURL events
	static pascal OSErr		GetURLSuiteHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);

	// Handler for GetURL events
	static pascal OSErr		SpyglassSuiteHandler(AppleEvent *appleEvent, AppleEvent *reply, long refCon);


	AEDispatchHandler*		GetDispatchHandler(DescType dispatchClass);
	
	
	void					GetSuspendedEvent(AppleEvent *appleEvent, AppleEvent *reply)
						{
							*appleEvent = mSuspendedEvent;
							*reply = mReplyToSuspendedEvent;
						}
						
protected:

	// Property token from list of tokens accessor
	void					PropertyTokenFromList(			DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken);
	
	void					GetAnythingFromApp(			DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken);
	
	void					RegisterClassHandler(			DescType			handlerClass,
													AEGenericClass*	classHandler,
													Boolean			isDuplicate = false);


	void					InstallSuiteHandlers(				Boolean			suspendEvents);
	
public:

	void					GetEventKeyDataParameter(		const AppleEvent*	appleEvent,
													DescType			requestedType,
													AEDesc*			data);

	static pascal OSErr		PropertyTokenFromAnything(		DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken,
					 								long 				refCon);

	static pascal OSErr		AnythingFromAppAccessor(		DescType			desiredClass,
					 								const AEDesc*		containerToken,
					 								DescType			containerClass,
					 								DescType			keyForm,
				    									const AEDesc*		keyData,
					 								AEDesc*			resultToken,
					 								long 				refCon);

	static pascal OSErr		CompareObjectsCallback(			DescType			comparisonOperator, 	// operator to use
													const AEDesc *		object,				// left-hand side
													const AEDesc *		descriptorOrObject, 		// right-hand side
													Boolean *			result);


	static pascal OSErr		CountObjectsCallback(			DescType 		 	desiredType,
													DescType 		 	containerClass,
													const AEDesc *		container,
									   				long *			result);

	void					ExtractData(					const AEDesc*		source,
													AEDesc*			data);
	
	
	static AEDispatchHandler*	GetDispatchHandlerForClass(	DescType			dispatchClass);
	
protected:
	
	AEDispatchTree			mDispatchTree;				// tree of handler dispatchers

	AEEventHandlerUPP		mSuspendEventHandlerUPP;
	
	AEEventHandlerUPP		mStandardSuiteHandlerUPP;
	AEEventHandlerUPP		mRequiredSuiteHandlerUPP;
	AEEventHandlerUPP		mCreateElementHandlerUPP;

	AEEventHandlerUPP		mMozillaSuiteHandlerUPP;
	AEEventHandlerUPP		mGetURLSuiteHandlerUPP;
	AEEventHandlerUPP		mSpyGlassSuiteHandlerUPP;

        AEMozillaSuiteHandler      mMozillaSuiteHandler;
        AEGetURLSuiteHandler      mGetURLSuiteHandler;
        AESpyglassSuiteHandler   mSpyglassSuiteHandler;
        
	OSLAccessorUPP		mPropertyFromListAccessor;
	OSLAccessorUPP		mAnythingFromAppAccessor;

	OSLCountUPP			mCountItemsCallback;
	OSLCompareUPP		mCompareItemsCallback;

private:

	AppleEvent			mSuspendedEvent;
	AppleEvent			mReplyToSuspendedEvent;
	
public:

	static AECoreClass*		GetAECoreHandler() { return sAECoreHandler; }
	static AECoreClass*		sAECoreHandler;

};

#endif	//__cplusplus

#endif /* __AECORECLASS__ */

