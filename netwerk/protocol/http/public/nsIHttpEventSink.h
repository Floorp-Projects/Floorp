/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIHttpEventSink_h___
#define nsIHttpEventSink_h___

#include "nsISupports.h"
#include "nscore.h"

class nsIUrl;

#define NS_IHTTPEVENTSINK_IID                        \
{ /* b297b0a0-ea35-11d2-931b-00104ba0fd40 */         \
    0xb297b0a0,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

/**
 * An instance of nsIHttpEventSink should be passed as the eventSink
 * argument of nsINetService::NewConnection for http URLs. It defines
 * the callbacks to the application program (the html parser).
 */
class nsIHttpEventSink : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTTPEVENTSINK_IID);

    /**
     * Notify the EventSink that progress as occurred for the URL load.<BR>
     */
    NS_IMETHOD OnProgress(nsIUrl* aURL, PRUint32 aProgress, PRUint32 aProgressMax) = 0;

    /**
     * Notify the EventSink with a status message for the URL load.<BR>
     */
    NS_IMETHOD OnStatus(nsIUrl* aURL, const PRUnichar* aMsg) = 0;

};

#endif /* nsIIHttpEventSink_h___ */
