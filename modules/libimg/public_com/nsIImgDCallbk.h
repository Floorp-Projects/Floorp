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
 */
#ifndef nsIImgDCallbk_h___
#define nsIImgDCallbk_h___

#include "xpcompat.h" // for TimeoutCallbackFunction
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
  NS_IMETHOD ImgDCBCreateGreyScaleColorSpace()=0;

  NS_IMETHOD_(void*) ImgDCBSetTimeout(TimeoutCallbackFunction func, void* closure, PRUint32 msecs)=0;
  NS_IMETHOD ImgDCBClearTimeout(void *timer_id)=0;

  NS_IMETHOD ImgDCBHaveHdr(int destwidth, int destheight )=0;

  NS_IMETHOD ImgDCBHaveRow(PRUint8 *rowbuf, PRUint8* rgbrow, int x_offset, int len,
                            int row, int dup_rowcnt, PRUint8 draw_mode, 
                            int pass )=0;


  NS_IMETHOD ImgDCBHaveImageFrame()=0;
  NS_IMETHOD ImgDCBHaveImageAll()=0;
  NS_IMETHOD ImgDCBError()=0;
};

#endif /* nsIImgDCallbk_h___ */
