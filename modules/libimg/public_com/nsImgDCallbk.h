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

/* -*- Mode: C; tab-width: 4 -*-
 *
 */


#include "nsIImgDecoder.h"

/*---------------------------------------------*/
/*-----------------class-----------------------*/

class ImgDCallbk : public nsIImgDCallbk
{
public:
  NS_DECL_ISUPPORTS
  ImgDCallbk(il_container *aContainer){mContainer=aContainer;};
  ~ImgDCallbk(){};


  NS_IMETHOD ImgDCBFlushImage();
  NS_IMETHOD ImgDCBImageSize();
  NS_IMETHOD ImgDCBResetPalette();
  NS_IMETHOD ImgDCBInitTransparentPixel();
  NS_IMETHOD ImgDCBDestroyTransparentPixel();
  NS_IMETHOD ImgDCBSetupColorspaceConverter();
  NS_IMETHOD_(NI_ColorSpace *) ImgDCBCreateGreyScaleColorSpace();

  NS_IMETHOD_(void*) ImgDCBSetTimeout(TimeoutCallbackFunction func,
                               void* closure, uint32 msecs);
  NS_IMETHOD ImgDCBClearTimeout(void *timer_id);


  /* callbacks from the decoder */
  NS_IMETHOD ImgDCBHaveHdr(int destwidth, int destheight);
  NS_IMETHOD ImgDCBHaveRow(uint8*, uint8*,
                            int, int, int, int,
                            uint8 , int);

  NS_IMETHOD ImgDCBHaveImageFrame();
  NS_IMETHOD ImgDCBHaveImageAll();
  NS_IMETHOD ImgDCBError();

  NS_IMETHODIMP CreateInstance(const nsCID &aClass,
                             il_container* ic,
                             const nsIID &aIID,
                             void **ppv) ;

  il_container *GetContainer() {return mContainer; };
  il_container *SetContainer(il_container *ic) {mContainer=ic; return ic; };

private:
  il_container* mContainer;
}; 

/*-------------------------------*/    
