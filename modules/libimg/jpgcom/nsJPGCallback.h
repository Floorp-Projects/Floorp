/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http:/www.mozilla.org/NPL/
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

/* -*- Mode: C; tab-width: 4 -*-
 *   nsJPGDecoder.cpp --- interface to jpg decoder
 */
#ifndef _nsJPGCallbk_h
#define _nsJPGCallbk_h


#include "nsIImgDecoder.h"
#include "nsJPGDecoder.h"


/* 2b0c6b90-d8b1-11d2-802c-0060088f91a3 */
#define NS_JPGCALLBK_CID \
{ 0x2b0c6b90, 0xd8b1, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* 5cb47c60-d8b1-11d2-802c-0060088f91a3 */
#define NS_JPGCALLBK_IID \
{ 0x5cb47c60, 0xd8b1, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }


class JPGCallbk : public nsIImgDCallbk {
	
public:
  NS_DECL_ISUPPORTS

  il_container *GetContainer() {return mContainer;};
  il_container *SetContainer(il_container *ic) {mContainer=ic; return ic;};

  JPGCallbk(il_container *aContainer){mContainer=aContainer;};
  ~JPGCallbk();

private:
  il_container* mContainer;
};

#endif
