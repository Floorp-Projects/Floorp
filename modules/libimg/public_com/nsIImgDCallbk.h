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

#include "dllcompat.h" // for TimeoutCallbackFunction
#include "nsISupports.h"

/* d34a2f20-cd9f-11d2-802c-0060088f91a3 */
#define NS_IIMGDCALLBK_IID \
{ 0xd34a2f20, 0xcd9f, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

class nsIImgDCallbk : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMGDCALLBK_IID)

  NS_IMETHOD ImgDCBFlushImage()=0;
  NS_IMETHOD ImgDCBImageSize()=0;
  NS_IMETHOD ImgDCBResetPalette()=0;
  NS_IMETHOD ImgDCBInitTransparentPixel()=0;
  NS_IMETHOD ImgDCBDestroyTransparentPixel()=0;
  NS_IMETHOD ImgDCBSetupColorspaceConverter()=0; 
  NS_IMETHOD_(NI_ColorSpace *) ImgDCBCreateGreyScaleColorSpace()=0;

  NS_IMETHOD_(void*) ImgDCBSetTimeout(TimeoutCallbackFunction func, void* closure, uint32 msecs)=0;
  NS_IMETHOD ImgDCBClearTimeout(void *timer_id)=0;

  NS_IMETHOD ImgDCBHaveHdr(int destwidth, int destheight )=0;

  NS_IMETHOD ImgDCBHaveRow(uint8 *rowbuf, uint8* rgbrow, int x_offset, int len,
                            int row, int dup_rowcnt, uint8 draw_mode, 
                            int pass )=0;


  NS_IMETHOD ImgDCBHaveImageFrame()=0;
  NS_IMETHOD ImgDCBHaveImageAll()=0;
  NS_IMETHOD ImgDCBError()=0;

private:
   void *ilContainer;

};

