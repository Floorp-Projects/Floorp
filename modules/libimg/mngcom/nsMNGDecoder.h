/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Tim Rowley, tor@cs.brown.edu, original author
 */

#ifndef nsMNGDecoder_h___
#define nsMNGDecoder_h___

#include "nsIImgDecoder.h"

/* d73f1676-1dd1-11b2-ba33-aabc09e02d10 */

#define NS_MNGDECODER_CID \
{ 0xd73f1676, 0x1dd1, 0x11b2, \
{ 0xba, 0x33, 0xaa, 0xbc, 0x09, 0xe0, 0x2d, 0x10 } }

class MNGDecoder : public nsIImgDecoder
{
public:
  MNGDecoder(il_container* aContainer);
  virtual ~MNGDecoder();

  NS_DECL_ISUPPORTS

  static NS_METHOD Create(nsISupports *aOuter, const nsIID &aIID, void **aResult);

  /* stream */
  NS_IMETHOD ImgDInit();

  NS_IMETHOD ImgDWriteReady(PRUint32 *max_read);
  NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len);
  NS_IMETHOD ImgDComplete();
  NS_IMETHOD ImgDAbort();

  NS_IMETHOD_(il_container *) SetContainer(il_container *ic){ilContainer = ic; return ic;}
  NS_IMETHOD_(il_container *) GetContainer() {return ilContainer;}

  
private:
  il_container* ilContainer;
};

#endif /* nsMNGDecoder_h___ */
