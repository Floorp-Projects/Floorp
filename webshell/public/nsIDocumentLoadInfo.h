/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIDocumentLoadInfo_h__
#define nsIDocumentLoadInfo_h__

#include "nsISupports.h"
#include "nsString.h"

#define NS_IDOCUMENTLOADINFO_IID \
{ 0xa6cf9063, 0x15b3, 0x11d2,    \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIURI;
class nsIContentViewerContainer;

class nsIDocumentLoadInfo : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTLOADINFO_IID)

  /**
   * Get the command associated with this document load
   *
   * @param aCommand out parameter for command
   * @result NS_OK if successful
   */
  NS_IMETHOD GetCommand(nsString &aCommand)=0;

  /**
   * Get the URL associated with this document load
   *
   * @param aURL out parameter for url
   * @result NS_OK if successful
   */
  NS_IMETHOD GetURL(nsIURI **aURL)=0;

  /**
   * Get the container associated with this document load
   *
   * @param aContainer out parameter for container
   * @result NS_OK if successful
   */
  NS_IMETHOD GetContainer(nsIContentViewerContainer **aContainer)=0;

  /**
   * Get the extra information associated with this document load
   *
   * @param aExtraInfo out parameter for extra information
   * @result NS_OK if successful
   */
  NS_IMETHOD GetExtraInfo(nsISupports **aExtraInfo)=0;
};

#endif // nsIDocumentLoadInfo_h__
