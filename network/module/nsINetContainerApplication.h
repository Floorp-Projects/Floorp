/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsINetContainerApplication_h__
#define nsINetContainerApplication_h__

#include "nsISupports.h"
#include "nsString.h"

#define NS_INETCONTAINERAPPLICATION_IID \
{ 0xca2610f0, 0x1054, 0x11d2,    \
 { 0xb3, 0x26, 0x00, 0x80, 0x5f, 0x8a, 0x38, 0x59 } }

class nsINetContainerApplication : public nsISupports {
public:

    /**
     * Get the application name string that will be used as part
     * of a HTTP request.
     *
     * @param aAppCodeName   The application name string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName)=0;
  
    /**
     * Get the application version string that will be used as part
     * of a HTTP request.
     *
     * @param aAppVersion   The application version string.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD    GetAppVersion(nsString& aAppVersion)=0;

    /**
     * Get the application name.
     *
     * @param aAppName   The application name.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD    GetAppName(nsString& aAppName)=0;

    /**
     * Get the translation of the application. The value for language
     * is usually a 2-letter code such as "en" and occasionally a 
     * five-character code to indicate a language subtype, such as "zh_CN". 
     *
     * @param aLanguage  The application language.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD    GetLanguage(nsString& aLanguage)=0;

    /**
     * Get the current platform (machine type).
     *
     * @param aPlatform   The current platform.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD    GetPlatform(nsString& aPlatform)=0;

};

#endif // nsINetContainerApplication_h__
