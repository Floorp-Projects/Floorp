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

#ifndef nsAESpyglassSuiteHandler_h_
#define nsAESpyglassSuiteHandler_h_


#include "nsAEUtils.h"

class nsDocLoadObserver;

class AESpyglassSuiteHandler
{
public:
	enum {
		kSuiteSignature					= 'WWW!',
				
		kOpenURLEvent					= 'OURL',			// OpenURL
		kRegisterURLEchoEvent		= 'RGUE',			// Register URL echo handler
		kUnregisterURLEchoEvent	= 'UNRU',			// Unregister URL echo handler

// the following are unhandled in Mozilla
		kRegisterViewerEvent 		= 'RGVW',			// RegisterViewer
		kUnregisterViewerEvent	= 'UNRV',			// UnregisterViewer
		kShowFileEvent					= 'SHWF',			// ShowFile
		kParseAnchorEvent			= 'PRSA',			// ParseAnchor
		kSpyActivateEvent			= 'ACTV',			// Activate
		kSpyListWindowsEvent		= 'LSTW',			// ListWindows
		kGetWindowInfoEvent		= 'WNFO',			// GetWindowInfo
		kRegisterWinCloseEvent	= 'RGWC',			// RegisterWindowClose
		kUnregisterWinCloseEvent	= 'UNRC',			// UnregisterWindowClose
		kRegisterProtocolEvent		= 'RGPR',			// RegisterProtocol
		kUnregisterProtocolEvent	= 'UNRP',			// UnregisterProtocol
		kCancelProgressEvent		= 'CNCL',			// Cancel download
		kFindURLEvent					= 'FURL',			// Find the URL for the file

// Spyglass send events

		kSpyglassSendSignature	= 'WWW?',
		kSendURLEchoEvent			= 'URLE'

	};

	// event params
	enum {
		kParamSaveToFileDest		= 'INTO',		// FSSpec save to file
		kParamOpenInWindow		= 'WIND',		// long, target window (window ID)
		kParamOpenFlags			= 'FLGS',		// long, Binary: any combination of 1, 2 and 4 is allowed: 1 and 2 mean force reload the document. 4 is ignored
		kParamPostData			= 'POST',		// text, Form posting data
		kParamPostType			= 'MIME',		// text, MIME type of the posting data. Defaults to application/x-www-form-urlencoded
		kParamProgressApp			= 'PROG'		// 'psn ', Application that will display progress
	};
	
						AESpyglassSuiteHandler();
						~AESpyglassSuiteHandler();

	void					HandleSpyglassSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);	// throws OSErrs

protected:

	void					HandleOpenURLEvent(AppleEvent *appleEvent, AppleEvent *reply);
	
	void					HandleRegisterURLEchoEvent(AppleEvent *appleEvent, AppleEvent *reply);
	void					HandleUnregisterURLEchoEvent(AppleEvent *appleEvent, AppleEvent *reply);

protected:

    nsDocLoadObserver*  mDocObserver;
    
};



#endif // nsAESpyglassSuiteHandler_h_
