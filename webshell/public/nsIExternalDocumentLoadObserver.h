/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**
 * MODULE NOTES:
 *
 * @update  nisheeth 4/25/99
 * 
 */

#ifndef nsIExternalDocumentLoadObserver_h__
#define nsIExternalDocumentLoadObserver_h__

#include "nsIObserver.h"

/* ba3e9e50-fb7f-11d2-aeea-00108300ff91 */
#define NS_IEXTERNALDOCUMENTLOADOBSERVER_IID      \
{ 0xba3e9e50, 0xfb7f, 0x11d2, {0xae, 0xea, 0x00, 0x10, 0x83, 0x00, 0xff, 0x91} }

class nsIExternalDocumentLoadObserver : public nsIObserver {
public:
  /**
   * Notify the observer that a new document will be loaded.  
   *
   * This notification occurs before any DNS resolution occurs, or
   * a connection is established with the server...
   */
  NS_IMETHOD OnStartDocumentLoad(PRUint32 aDocumentID, nsString& aURL, const char* aCommand) = 0;

  /**
   * Notify the observer that a document has been completely loaded.
   */
  NS_IMETHOD OnEndDocumentLoad(PRUint32 aDocumentID, nsString& aURL, PRInt32 aStatus) = 0;

  /**
   * Notify the observer that the specified nsIURL has just started to load.
   *
   * This notification occurs after DNS resolution, and a connection to the
   * server has been established.
   */
  NS_IMETHOD OnStartURLLoad(PRUint32 aDocumentID, nsString& aURL, const char* aContentType) = 0;
  
  /**
   * Notify the observer that progress has occurred in the loading of the 
   * specified URL...
   */
  NS_IMETHOD OnProgressURLLoad(PRUint32 aDocumentID, nsString& aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax) = 0;

  /**
   * Notify the observer that status text is available regarding the URL
   * being loaded...
   */
  NS_IMETHOD OnStatusURLLoad(PRUint32 aDocumentID, nsString& aURL, nsString& aMsg) = 0;

  /**
   * Notify the observer that the specified nsIURL has finished loading.
   */
  NS_IMETHOD OnEndURLLoad(PRUint32 aDocumentID, nsString& aURL, PRInt32 aStatus) = 0;

};

#endif /* nsIExternalDocumentLoadObserver_h__ */

