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

#include "if_struct.h"
#include "ni_pixmp.h"

#include "nsISupports.h"
#include "nsImgDecCID.h"


class nsIImgDecoder : public nsISupports
{
public:

	NS_IMETHOD ImgDInit()=0;

	NS_IMETHOD ImgDWriteReady()=0;
	NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len)=0;
	NS_IMETHOD ImgDComplete()=0;
	NS_IMETHOD ImgDAbort()=0;

private:
   void *mContainer;

};

NS_DEFINE_IID(kImgDCallbkIID, NS_IMGDCALLBK_IID);
NS_DEFINE_CID(kImgDCallbkCID, NS_IMGDCALLBK_CID);

class nsIImgDCallbk : public nsISupports
{
public:

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
   void *mContainer;

};



#endif
