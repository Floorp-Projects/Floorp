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

#ifndef nsIConnectionInfo_h__
#define nsIConnectionInfo_h__

#include "nsISupports.h"

#define NS_ICONNECTIONINFO_IID \
{ 0xa6cf9062, 0x15b3, 0x11d2,  \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

class nsIURL;
class nsIInputStream;
class nsIOutputStream;
class nsIStreamListener;

class nsIConnectionInfo : public nsISupports {
public:

	static const nsIID& GetIID() { static nsIID iid = NS_ICONNECTIONINFO_IID; return iid; }

  /**
   * Get the URL associated with this connection.
   *
   * @param aURL Out parameter
   * @result NS_OK if successful
   */
  NS_IMETHOD GetURL(nsIURL **aURL)=0;

  /**
   * Get the input stream associated with this connection
   *
   * @param aStream Out parameter
   * @result NS_OK if successful
   */
  NS_IMETHOD GetInputStream(nsIInputStream **aStream)=0;

  /**
   * Get the output stream associated with this connection
   *
   * @param aStream Out parameter
   * @result NS_OK if successful
   */
  NS_IMETHOD GetOutputStream(nsIOutputStream **aStream)=0;

  /**
   * Get the stream listener associated with this connection
   *
   * @param aConsumer Out parameter
   * @result NS_OK if successful
   */
  NS_IMETHOD GetConsumer(nsIStreamListener **aConsumer)=0;
};

#endif // nsIConnectionInfo_h__
