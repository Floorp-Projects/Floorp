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
#ifndef nsIUnicharStreamLoader_h___
#define nsIUnicharStreamLoader_h___

#include "nsISupports.h"
#include "nsString.h"

#define NS_IUNICHARSTREAMLOADER_IID      \
  {0xa6cf90d8, 0x15b3,  0x11d2,          \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIUnicharStreamLoader;
class nsIURL;

/**
 * Funtion registered with the stream loader. This function is called
 * when the stream is done loading (or has aborted).
 *
 * @param aLoader the loader
 * @param aData the result of the load as a unicode character string
 * @param aRef the opaque data passed to the loader
 * @param aStatus the completion status of the stream
 */
typedef void (*nsStreamCompleteFunc)(nsIUnicharStreamLoader* aLoader,
				     nsString& aData,
				     void* aRef,
				     nsresult aStatus);

/**
 * The purpose of this interface is to provide a mechanism for a
 * byte stream to be loaded asynchronously from a URL, the stream 
 * data to be accumulated, and the result to be returned as a
 * unicode character string.
 */
class nsIUnicharStreamLoader : public nsISupports {
public:
  /**
   * Get the number of bytes read so far.
   * 
   * @param aNumBytes out parameter to get number of unicode 
   *                  characters read.
   */
  NS_IMETHOD GetNumCharsRead(PRInt32* aNumChars) = 0;
};

/**
 * Start loading the specified URL and accumulating the stream data.
 * When the stream is completed, the result is sent to the complete
 * function as a unicode character string.
 *
 * @param aInstancePtrResult new stream loader
 * @param aURL the URL to load
 * @param aFunc the function to call on termination of stream loading
 * @param aRef an opaque value that will later be sent to the termination
 *             function
 */
extern NS_NET nsresult NS_NewUnicharStreamLoader(nsIUnicharStreamLoader** aInstancePtrResult,
						 nsIURL* aURL,
						 nsStreamCompleteFunc aFunc,
						 void* aRef);

#endif /* nsIUnicharStreamLoader_h___ */
