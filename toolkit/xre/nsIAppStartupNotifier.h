/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIAppStartupNotifier_h___
#define nsIAppStartupNotifier_h___

#include "nsIObserver.h"

/*
 Some components need to be run at the startup of mozilla or embedding - to 
 start new services etc.

 This interface provides a generic way to start up arbitrary components 
 without requiring them to hack into main1() (or into NS_InitEmbedding) as 
 it's currently being done for services such as wallet, command line handlers 
 etc.

 We will have a category called "app-startup" which components register 
 themselves in using the CategoryManager.

 Components can also (optionally) add the word "service," as a prefix 
 to the "value" they pass in during a call to AddCategoryEntry() as
 shown below:

    categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "testcomp",
                        "service," NS_WALLETSERVICE_CONTRACTID
                        true, true,
                        getter_Copies(previous));

 Presence of the "service" keyword indicates the components desire to 
 be started as a service. When the "service" keyword is not present
 we just do a do_CreateInstance.

 When mozilla starts (and when NS_InitEmbedding()) is invoked
 we create an instance of the AppStartupNotifier component (which 
 implements nsIObserver) and invoke its Observe() method. 

 Observe()  will enumerate the components registered into the
 APPSTARTUP_CATEGORY and notify them that startup has begun
 and release them.
*/

#define NS_APPSTARTUPNOTIFIER_CONTRACTID "@mozilla.org/embedcomp/appstartup-notifier;1"

#define APPSTARTUP_CATEGORY "app-startup"
#define APPSTARTUP_TOPIC    "app-startup"


/*
 Please note that there's not a new interface in this file.
 We're just leveraging nsIObserver instead of creating a
 new one

 This file exists solely to provide the defines above
*/

#endif /* nsIAppStartupNotifier_h___ */
