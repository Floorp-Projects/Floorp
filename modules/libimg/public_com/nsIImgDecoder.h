/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsIImgDecoder_h
#define _nsIImgDecoder_h

#include "if_struct.h" // for il_container
#include "ni_pixmp.h"
#include "nsISupports.h"

/* f00c22b0-bbd2-11d2-802c-0060088f91a3 */
#define NS_IIMGDECODER_IID \
{ 0xf00c22b0, 0xbbd2, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

#define NS_IIMGDECODER_BASE_PROGID "component://netscape/image/decoder&type="

class nsIImgDecoder : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMGDECODER_IID)

	NS_IMETHOD ImgDInit()=0;

	NS_IMETHOD ImgDWriteReady(PRUint8 *modata)=0;
	NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len)=0;
	NS_IMETHOD ImgDComplete()=0;
	NS_IMETHOD ImgDAbort()=0;

    // XXX Need to fix this to make sure return type is nsresult
    NS_IMETHOD_(il_container *) SetContainer(il_container *ic) = 0;
    NS_IMETHOD_(il_container *) GetContainer() = 0;
};



#endif
