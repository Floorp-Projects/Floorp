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
#ifndef nsIUnicharInputStream_h___
#define nsIUnicharInputStream_h___

#include "nsIInputStream.h"
#include "nscore.h"

class nsString;

#define NS_IUNICHAR_INPUT_STREAM_IID \
{ 0x2d97fbf0, 0x93b5, 0x11d1,        \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/** Abstract unicode character input stream
 *  @see nsIInputStream
 */
class nsIUnicharInputStream : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUNICHAR_INPUT_STREAM_IID)

  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aOffset,
                  PRUint32 aCount,
                  PRUint32 *aReadCount) = 0;
  NS_IMETHOD Close() = 0;
};

/**
 * Create a nsIUnicharInputStream that wraps up a string. Data is fed
 * from the string out until the done. When this object is destroyed
 * it destroyes the string (so make a copy if you don't want it doing
 * that)
 */
extern NS_COM nsresult
  NS_NewStringUnicharInputStream(nsIUnicharInputStream** aInstancePtrResult,
                                 nsString* aString);

/** Create a new nsUnicharInputStream that provides a converter for the
 * byte input stream aStreamToWrap. If no converter can be found then
 * nsnull is returned and the error code is set to
 * NS_INPUTSTREAM_NO_CONVERTER.
 */
extern NS_COM nsresult
  NS_NewConverterStream(nsIUnicharInputStream** aInstancePtrResult,
                        nsISupports* aOuter,
                        nsIInputStream* aStreamToWrap,
                        PRInt32 aBufferSize = 0,
                        nsString* aCharSet = nsnull);

#endif /* nsUnicharInputStream_h___ */
