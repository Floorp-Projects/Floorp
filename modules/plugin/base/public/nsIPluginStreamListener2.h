/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * This is a temporary internal interface...see bug 82415
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

 */

#ifndef nsIPluginStreamListener2_h___
#define nsIPluginStreamListener2_h___

#include "nsplugindefs.h"
#include "nsISupports.h"
#include "nsIPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"
#include "nsIInputStream.h"

#define NS_IPLUGINSTREAMLISTENER2_IID                \
{ /* dfc67e80-1dd1-11b2-a6ae-f99f07cc1f63 */         \
    0xdfc67e80,                                      \
    0x1dd1,                                          \
    0x11b2,                                          \
    {0xa6, 0xae, 0xf9, 0x9f, 0x07, 0xcc, 0x1f, 0x63} \
}

/**
 * The nsIPluginStreamListener interface defines the minimum set of functionality that
 * the browser will support if it allows plugins. Plugins can call QueryInterface
 * to determine if a plugin manager implements more specific APIs or other 
 * browser interfaces for the plugin to use (e.g. nsINetworkManager).
 *
 * This is an extended interface to allow for offset requests in OnDataAvail
 * see bugzilla 82415.
 */
class nsIPluginStreamListener2 : public nsIPluginStreamListener {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINSTREAMLISTENER2_IID)

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param aIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param offset location of this chunk of data from the start of the stream
     * @param length    The amount of data that was just pushed into the stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 offset, PRUint32 length) = 0;

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStreamListener2_h___ */
