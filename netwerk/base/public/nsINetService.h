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

#ifndef nsINetService_h___
#define nsINetService_h___

#include "nsISupports.h"
#include "nsString2.h"

class nsIUrl;
class nsIProtocolConnection;
class nsIConnectionGroup;
class nsIProtocolHandler;

#define NS_INETSERVICE_IID                           \
{ /* 3c70f340-ea35-11d2-931b-00104ba0fd40 */         \
    0x3c70f340,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_NETSERVICE_CID                            \
{ /* 451ec5e0-ea35-11d2-931b-00104ba0fd40 */         \
    0x451ec5e0,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsINetService : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_INETSERVICE_IID);

    NS_IMETHOD GetProtocolHandler(const char* scheme,
                                  nsIProtocolHandler* *result) = 0;

    NS_IMETHOD NewConnectionGroup(nsIConnectionGroup* *result) = 0;

    /**
     * Returns the absolute URL given a (possibly) relative spec, and 
     * a base URL. Returns a string that must be freed with delete[].
     */
    NS_IMETHOD MakeAbsoluteUrl(const char* aSpec,
                               nsIUrl* aBaseUrl,
                               char* *result) = 0;

    /**
     * This method constructs a new URL by first determining the scheme
     * of the url spec, and then delegating the construction of the URL
     * to the protocol handler for that scheme. QueryInterface can be used
     * on the resulting URL object to obtain a more specific type of URL.
     */
    NS_IMETHOD NewUrl(const char* aSpec,
                        nsIUrl* *result,
                        nsIUrl* aBaseUrl=0) = 0;

    NS_IMETHOD NewConnection(nsIUrl* url,
                             nsISupports* eventSink,
                             nsIConnectionGroup* group,
                             nsIProtocolConnection* *result) = 0;

    /**
     * @return NS_OK if there are active connections
     * @return NS_COMFALSE if there are not.
     */
    NS_IMETHOD HasActiveConnections() = 0;


    /**
     * Get the application name string that will be used as part
     * of a HTTP request.
     *
     * @param aAppCodeName   The application name string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppCodeName(nsString2& aAppCodeName)=0;
  
    /**
     * Get the application version string that will be used as part
     * of a HTTP request.
     *
     * @param aAppVersion   The application version string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppVersion(nsString2& aAppVersion)=0;

    /**
     * Get the application name.
     *
     * @param aAppName   The application name.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppName(nsString2& aAppName)=0;

    /**
     * Get the translation of the application. The value for language
     * is usually a 2-letter code such as "en" and occasionally a 
     * five-character code to indicate a language subtype, such as "zh_CN". 
     *
     * @param aLanguage  The application language.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetLanguage(nsString2& aLanguage)=0;

    /**
     * Get the current platform (machine type).
     *
     * @param aPlatform   The current platform.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetPlatform(nsString2& aPlatform)=0;

    /**
     * Get the HTTP advertised user agent string.
     *
     * @param aUA The current user agent string being sent out in HTTP requests.
     * @retrun Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetUserAgent(nsString2& aUA)=0;

};

#endif /* nsIINetService_h___ */
