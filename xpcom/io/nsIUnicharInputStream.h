/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIUnicharInputStream_h___
#define nsIUnicharInputStream_h___

#include "nsIInputStream.h"
#include "nsString.h"
#include "nscore.h"

class nsIUnicharInputStream;

typedef NS_CALLBACK(nsWriteUnicharSegmentFun)(nsIUnicharInputStream *aInStream,
                                              void *aClosure,
                                              const PRUnichar *aFromSegment,
                                              PRUint32 aToOffset,
                                              PRUint32 aCount,
                                              PRUint32 *aWriteCount);
/* c4bcf6ee-3a79-4d77-8d48-f17be3199b3b */
#define NS_IUNICHAR_INPUT_STREAM_IID \
{ 0xc4bcf6ee, 0x3a79, 0x4d77,        \
  {0x8d, 0x48, 0xf1, 0x7b, 0xe3, 0x19, 0x9b, 0x3b} }

/** Abstract unicode character input stream
 *  @see nsIInputStream
 */
class NS_NO_VTABLE nsIUnicharInputStream : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUNICHAR_INPUT_STREAM_IID)

  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aCount,
                  PRUint32 *aReadCount) = 0;
  NS_IMETHOD Close() = 0;
  NS_IMETHOD ReadSegments(nsWriteUnicharSegmentFun aWriter,
                          void* aClosure,
                          PRUint32 aCount,
                          PRUint32 *aReadCount) = 0;
};

/**
 * Create a nsIUnicharInputStream that wraps up a string. Data is fed
 * from the string out until the done. When this object is destroyed
 * it destroys the string by calling |delete| on the pointer if
 * aTakeOwnership is set.  If aTakeOwnership is not set, you must
 * ensure that the string outlives the stream!
 */
extern NS_COM nsresult
  NS_NewStringUnicharInputStream(nsIUnicharInputStream** aInstancePtrResult,
                                 const nsAString* aString,
                                 PRBool aTakeOwnerhip);

/** Create a new nsUnicharInputStream that provides a converter for the
 * byte input stream aStreamToWrap. If no converter can be found then
 * nsnull is returned and the error code is set to
 * NS_INPUTSTREAM_NO_CONVERTER.
 */
extern NS_COM nsresult
  NS_NewUTF8ConverterStream(nsIUnicharInputStream** aInstancePtrResult,
                            nsIInputStream* aStreamToWrap,
                            PRInt32 aBufferSize = 0);

#endif /* nsUnicharInputStream_h___ */
