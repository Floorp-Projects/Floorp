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


class AESpyglassSuiteHandler
{
public:
	enum {
		kOpenURLEvent		= 'OURL'
	};
	
						AESpyglassSuiteHandler();
						~AESpyglassSuiteHandler();

	void					HandleSpyglassSuiteEvent(AppleEvent *appleEvent, AppleEvent *reply);	// throws OSErrs

protected:

	void					HandleOpenURLEvent(AppleEvent *appleEvent, AppleEvent *reply);
};






#endif // nsAESpyglassSuiteHandler_h_
