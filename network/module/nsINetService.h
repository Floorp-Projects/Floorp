/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nscore.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsITransport.h"
#ifdef SingleSignon
#include "lo_ele.h"
#endif

class nsIProtocolURLFactory;
class nsIProtocol;

 /* XXX: This should be moved to ns/xpcom/src/nserror.h */
#define NS_OK    0
#define NS_FALSE 1


/* cfb1a480-c78f-11d1-bea2-00805f8a66dc */
#define NS_INETSERVICE_IID   \
{ 0xcfb1a480, 0xc78f, 0x11d1, \
  {0xbe, 0xa2, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/* {3F1BFE70-4D9C-11d2-9E7E-006008BF092E} */
#define NS_NETSERVICE_CID \
{ 0x3f1bfe70, 0x4d9c, 0x11d2, \
  {0x9e, 0x7e, 0x00, 0x60, 0x08, 0xbf, 0x09, 0x2e} }


/**
 * The nsINetService interface provides an API to the networking service.
 * This is a preliminary interface which <B>will</B> change over time!
 * 
 */
struct nsINetService : public nsISupports 
{
	static const nsIID& GetIID() { static nsIID iid = NS_INETSERVICE_IID; return iid; }
    /**
     * Initiate an asynchronous URL load.<BR><BR>
     *
     * @param aUrl      The URL to load.
     * @param aConsumer An object that will receive notifications during the 
     *                  URL loading.  This parameter cannot be NULL.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */ 
    NS_IMETHOD OpenStream(nsIURL *aUrl, 
                          nsIStreamListener *aConsumer) = 0;

    /**
     * Initiate a synchronous URL load.<BR><BR>
     * 
     * @param aUrl      The URL to load.
     * @param aConsumer An object that will receive notifications during the 
     *                  URL loading.  This parameter can be NULL if 
     *                  notifications are not required.
     * @param aNewStream    An output parameter to recieve the blocking stream
     *                      created for this URL load.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD OpenBlockingStream(nsIURL *aUrl, 
                                  nsIStreamListener *aConsumer,
                                  nsIInputStream **aNewStream) = 0;

    /**
     * Interrupt an asynchronous URL load.<BR><BR>
     *
     * @param aUrl  The URL to stop loading.
     * @return Returns NS_OK if successful, or NS_ERROR_FAILURE if an error occurred.
     */ 
    NS_IMETHOD InterruptStream(nsIURL* aURL) = 0;

    /**
     * Get the complete cookie string associated with the URL
     *
     * @param aURL The URL for which to get the cookie string
     * @param aCookie The string object which will hold the result
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetCookieString(nsIURL *aURL, nsString& aCookie)=0;

   /**
     * Set the cookie string associated with the URL
     *
     * @param aURL The URL for which to set the cookie string
     * @param aCookie The string to set
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD SetCookieString(nsIURL *aURL, const nsString& aCookie)=0;

#ifdef CookieManagement
    NS_IMETHOD NET_DisplayCookieInfoAsHTML()=0;
#ifndef HTMLDialogs
    NS_IMETHOD NET_CookieViewerReturn()=0;
#endif
#ifdef PrivacySiteInfo
    NS_IMETHOD NET_DisplayCookieInfoOfSiteAsHTML(char * URLName)=0;
    NS_IMETHOD NET_CookiePermission(char* URLName, PRInt32* permission)=0;
    NS_IMETHOD NET_CookieCount(char* URLName, PRInt32* count)=0;
#endif
#endif

    NS_IMETHOD NET_AnonymizeCookies()=0;
    NS_IMETHOD NET_UnanonymizeCookies()=0;
    NS_IMETHOD SI_AnonymizeSignons()=0;
    NS_IMETHOD SI_UnanonymizeSignons()=0;

   /**
     * Get the http proxy used for http transactions.
     *
     * @param aProxyHTTP The url used as a proxy. The url is of the form 
     *  "host.server:port".
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetProxyHTTP(nsString& aProxyHTTP)=0;

   /**
     * Set the http proxy to be used for http transactions.
     *
     * @param aProxyHTTP The url to use as a proxy. The url is of the form 
     *  "host.server:port".
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD SetProxyHTTP(nsString& aProxyHTTP)=0;

   /**
     * Get the http version number setting.
     *
     * @param aOneOne If true, the client is sending HTTP/1.1, if false HTTP/1.0
     *  is being sent.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetHTTPOneOne(PRBool& aOneOne)=0;

   /**
     * Set the http version number setting.
     *
     * @param aSendOneOne True if you want the http version number sent out
     *  to be 1.1, false if you want it to be 1.0.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD SetHTTPOneOne(PRBool aSendOneOne)=0;

    /**
     * Get the application name string that will be used as part
     * of a HTTP request.
     *
     * @param aAppCodeName   The application name string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppCodeName(nsString& aAppCodeName)=0;
  
    /**
     * Get the application version string that will be used as part
     * of a HTTP request.
     *
     * @param aAppVersion   The application version string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppVersion(nsString& aAppVersion)=0;

    /**
     * Get the application name.
     *
     * @param aAppName   The application name.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetAppName(nsString& aAppName)=0;

    /**
     * Get the translation of the application. The value for language
     * is usually a 2-letter code such as "en" and occasionally a 
     * five-character code to indicate a language subtype, such as "zh_CN". 
     *
     * @param aLanguage  The application language.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetLanguage(nsString& aLanguage)=0;

    /**
     * Get the current platform (machine type).
     *
     * @param aPlatform   The current platform.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetPlatform(nsString& aPlatform)=0;

    /**
     * Get the HTTP advertised user agent string.
     *
     * @param aUA The current user agent string being sent out in HTTP requests.
     * @retrun Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD GetUserAgent(nsString& aUA)=0;

    /**
     * Add a string to the user agent AppVersion.
     *
     * @param aCustom   The string to be inserted into the AppVersion portion
     *  of the user agent string. A user-agent of the form:
     *      Mozilla/x.x [en] (OS; I) becomes Mozilla/x.x [en]C-aCustom (OS; I)
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD SetCustomUserAgent(nsString aCustom)=0;

    // Managing pluggable protocols:

    NS_IMETHOD RegisterProtocol(const nsString& aName, 
                                nsIProtocolURLFactory* aProtocolURLFactory,
                                nsIProtocol* aProtocol) = 0;

    NS_IMETHOD UnregisterProtocol(const nsString& aName) = 0;

    NS_IMETHOD GetProtocol(const nsString& aName,
                           nsIProtocolURLFactory* *aProtocolURLFactory,
                           nsIProtocol* *aProtocol) = 0;

    NS_IMETHOD CreateURL(nsIURL* *aURLResult, 
                         const nsString& aSpec, 
                         const nsIURL* aContextURL = nsnull,
                         nsISupports* aContainer = nsnull,
                         nsIURLGroup* aGroup = nsnull) = 0;

	/**
	 * Protocol connection pools should use this call into the service manager to 
	 * produce a transport interface on a socket. You need to pass in a port and a host
	 * name to associate with the socket.
	*/
     
	NS_IMETHOD CreateSocketTransport(nsITransport **aTransport, PRUint32 aPortToUse, const char * aHostName) = 0;

	/**
	* Protocol connection pools should use this call into the service manager to produce
	* a transport interface to a file based socket. You need to pass in the name of the file
	* you want to associate with the socket transport
	*/

	NS_IMETHOD CreateFileSocketTransport(nsITransport **aTransport, const char * aFileName) = 0;
    
    NS_IMETHOD AreThereActiveConnections(void) = 0;
};


/**
 * Create an instance of the INetService
 *
 */
extern "C" NS_NET nsresult NS_NewINetService(nsINetService** aInstancePtrResult,
                                             nsISupports* aOuter);

extern "C" NS_NET nsresult NS_InitINetService();

extern "C" NS_NET nsresult NS_ShutdownINetService();

#endif /* nsINetService_h___ */
