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

#ifndef nsIFtpEventSink_h___
#define nsIFtpEventSink_h___

#include "nsISupports.h"
#include "nscore.h"

class nsIUrl;

// {25029491-F132-11d2-9588-00805F369F95}
#define NS_IFTPEVENTSINK_IID                        \
    { 0x25029491, 0xf132, 0x11d2, { 0x95, 0x88, 0x0, 0x80, 0x5f, 0x36, 0x9f, 0x95 } }


/**
 * An instance of nsIFfpEventSink should be passed as the eventSink
 * argument of nsINetService::NewConnection for ftp URLs. It defines
 * the callbacks to the application program (the html parser).
 */
class nsIFtpEventSink : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFTPEVENTSINK_IID);

    /**
     * Notify the EventSink that progress as occurred for the URL load.<BR>
     */
    NS_IMETHOD OnProgress(nsIUrl* aURL, PRUint32 aProgress, PRUint32 aProgressMax) = 0;

    /**
     * Notify the EventSink with a status message for the URL load.<BR>
     */
    NS_IMETHOD OnStatus(nsIUrl* aURL, const PRUnichar* aMsg) = 0;

};

#endif /* nsIIFtpEventSink_h___ */
