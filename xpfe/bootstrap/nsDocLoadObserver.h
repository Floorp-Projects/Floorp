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

#ifndef nsDocLoadObserver_h_
#define nsDocLoadObserver_h_

#include "nsIObserver.h"

#include <MacTypes.h>
#include "nsVoidArray.h"

/*----------------------------------------------------------------------------
	nsDocLoadObserver 
	The nsDocLoadObserver is a singleton object that registers for 
	'EndDocumentLoad' events from the Mozilla browser.  It maintains a set of
	'echo requesters'.  When the observer receives an event notification, it
	forwards the event to all echo requesters.  If the forward fails (most 
	likely because the application is no longer in memory), the echo requester
	is automatically removed from the set.

	Usage:
		nsDocLoadObserver& observer = nsDocLoadObserver::Instance();
		observer.Register();
		observer.AddEchoRequester('App1');
		observer.AddEchoRequester('App2');
----------------------------------------------------------------------------*/
class nsDocLoadObserver : public nsIObserver
{
public:

	          nsDocLoadObserver();
	virtual 	~nsDocLoadObserver();
	
	NS_DECL_ISUPPORTS
	NS_DECL_NSIOBSERVER

	// add an application to be notified when an 'EndDocumentLoad' has occurred
	void AddEchoRequester(OSType appSignature);
	void RemoveEchoRequester(OSType appSignature);
	
protected:

	// register the observer with the nsIObserverService.  After this object
	// has been successfully registered, this method will simply return
	// immediately.
	void Register();
	void Unregister();

private:
	// 'true' if this handler has been successfully registered with the nsIObserverService
	PRBool mRegistered;

	nsVoidArray		mEchoRequesters;
};

#endif // nsDocLoadObserver_h_
