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

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsAEUtils.h"
#include "nsAESpyglassSuiteHandler.h"

#include "nsDocLoadObserver.h"

nsDocLoadObserver::nsDocLoadObserver()
:	mRegistered(PR_FALSE)
{
	NS_INIT_ISUPPORTS();
}

nsDocLoadObserver::~nsDocLoadObserver()
{
}


NS_IMPL_ISUPPORTS1(nsDocLoadObserver, nsIObserver)

void nsDocLoadObserver::Register()
{
	if (!mRegistered)
	{
		nsresult rv;
		nsCOMPtr<nsIObserverService> anObserverService = do_GetService(NS_OBSERVERSERVICE_PROGID, &rv);
		if (NS_SUCCEEDED(rv))
		{
			if (NS_SUCCEEDED(anObserverService->AddObserver(this, 
				NS_ConvertASCIItoUCS2("EndDocumentLoad").GetUnicode())))
			{
				mRegistered = PR_TRUE;
			}
		}
	}
}

void nsDocLoadObserver::Unregister()
{
	if (mRegistered)
	{
		nsresult rv;
		nsCOMPtr<nsIObserverService> anObserverService = do_GetService(NS_OBSERVERSERVICE_PROGID, &rv);
		if (NS_SUCCEEDED(rv))
		{
			if (NS_SUCCEEDED(anObserverService->RemoveObserver(this, 
				NS_ConvertASCIItoUCS2("EndDocumentLoad").GetUnicode())))
			{
				mRegistered = PR_FALSE;
			}
		}
	}
}

void nsDocLoadObserver::AddEchoRequester(OSType appSignature)
{
  // make sure an application is only in the list once
  mEchoRequesters.RemoveElement((void*)appSignature);
  
	mEchoRequesters.AppendElement((void*)appSignature);
	Register();     // ensure we are registered
}

void nsDocLoadObserver::RemoveEchoRequester(OSType appSignature)
{
	mEchoRequesters.RemoveElement((void*)appSignature);
	if (mEchoRequesters.Count() == 0)
	  Unregister();
}

NS_IMETHODIMP nsDocLoadObserver::Observe(nsISupports* /*aSubject*/,
		const PRUnichar* /*aTopic*/, const PRUnichar* someData)
{
	// we need a URL to forward
	if (!someData)
		return NS_ERROR_NULL_POINTER;

	// are there any echo requesters to notify?
	if (mEchoRequesters.Count() == 0)
		return NS_OK;

	// create the descriptor for identifying this application
	StAEDesc from;
	const OSType mozz = 'MOZZ';
	if (noErr != ::AECreateDesc(typeType, &mozz, sizeof(mozz), &from))
		return NS_ERROR_UNEXPECTED;

	// create the descriptor for the URL
	nsString urlText(someData);
	char* urlString = urlText.ToNewCString();

	StAEDesc url;
	OSErr err = ::AECreateDesc(typeChar, urlString, urlText.Length(), &url);
	nsMemory::Free(urlString);
	if (err != noErr)
		return NS_ERROR_UNEXPECTED;

	// keep track of any invalid requesters and remove them when we're done
	nsVoidArray requestersToRemove;
	
	PRInt32		numRequesters = mEchoRequesters.Count();
	
	// now notify all the echo requesters
	for (PRInt32 i = 0; i < numRequesters; i ++)
	{
		// specify the address of the requester
		StAEDesc targetAddress;
		const OSType target = (OSType)mEchoRequesters.ElementAt(i);
		if (noErr != ::AECreateDesc(typeApplSignature, &target, sizeof(target), &targetAddress))
			return NS_ERROR_UNEXPECTED;

		// create the event
		AppleEvent sendEvent;
		err = ::AECreateAppleEvent(AESpyglassSuiteHandler::kSpyglassSendSignature,
				AESpyglassSuiteHandler::kSendURLEchoEvent, 
				&targetAddress, kAutoGenerateReturnID, kAnyTransactionID, &sendEvent);
		if (noErr != err)
		{	// this target is no longer available
			requestersToRemove.AppendElement((void *)target);
			continue;
		}

		// attach our signature - to let the requester know who we are
		err = ::AEPutParamDesc(&sendEvent, keyOriginalAddressAttr, &from);
		NS_ASSERTION(noErr == err, "AEPutParamDesc");

		// attach the new URL
		err = ::AEPutParamDesc(&sendEvent, keyDirectObject, &url);
		NS_ASSERTION(noErr == err, "AEPutParamDesc");
		
		// send it
		AppleEvent reply;
		err = ::AESend(&sendEvent, &reply, kAENoReply, kAENormalPriority, 180, NULL, NULL);
		NS_ASSERTION(noErr == err, "AESend");
		::AEDisposeDesc(&sendEvent);
	}

	// now remove unresponsive requestors
	for (PRInt32 i = 0; i < requestersToRemove.Count(); i ++)
	{
		OSType	thisRequester = (OSType)requestersToRemove.ElementAt(i);
		mEchoRequesters.RemoveElement((void *)thisRequester);
	}
	
	return NS_OK;
}
