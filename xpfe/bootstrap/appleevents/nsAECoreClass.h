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

	void					HandleCoreSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply);			// throws OSErrs
	void					HandleRequiredSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs

	void					HandleMozillaSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	void					HandleGetURLSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	void					HandleSpyglassSuiteEvent(const AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs

	void					HandleCreateElementEvent(const AppleEvent *appleEvent, AppleEvent *reply);		// throws OSErrs
	
	void					HandleEventSuspend(const AppleEvent *appleEvent, AppleEvent *reply);
	void					ResumeEventHandling(const AppleEvent *appleEvent, AppleEvent *reply, Boolean dispatchEvent);
	
	// AE Handlers
	static pascal OSErr		SuspendEventHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);
	static pascal OSErr		RequiredSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);
	static pascal OSErr		CoreSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);
	static pascal OSErr		CreateElementHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);

	// Handler for Mozilla Suite events
	static pascal OSErr		MozillaSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);

	// Handler for GetURL events
	static pascal OSErr		GetURLSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);

	// Handler for GetURL events
	static pascal OSErr		SpyglassSuiteHandler(const AppleEvent *appleEvent, AppleEvent *reply, UInt32 refCon);


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

