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

#ifndef nsImgDecCIID_h__
#define nsImgDecCIID_h__

#include "nsRepository.h"


/* f00c22b0-bbd2-11d2-802c-0060088f91a3 */
#define NS_IIMGDECODER_IID \
{ 0xf00c22b0, 0xbbd2, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }


/////////////////////////////////////////////////

#define NS_IMGDECODER_CID \
{ 0xc9089cc0, 0xbaf4, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* bc60b730-bbcf-11d2-802c-0060088f91a3 */
#define NS_IMGDECODER_IID \
{ 0xbc60730, 0xbbcf, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

NS_DEFINE_IID(kImgDecoderIID, NS_IMGDECODER_IID);
NS_DEFINE_IID(kImgDecoderCID, NS_IMGDECODER_CID);

////////////////////////////////////////////////

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
/////////////////////////////////////////////////
/* e41ac650-cd9f-11d2-802c-0060088f91a3 */
#define NS_IMGDCALLBK_CID \
{ 0xe41ac650, 0xcd9f, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* d34a2f20-cd9f-11d2-802c-0060088f91a3 */
#define NS_IMGDCALLBK_IID \
{ 0xd34a2f20, 0xcd9f, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }




#endif

