/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prmem.h"

#include "nsGIFDecoder2.h"
#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsMemory.h"

#include "imgIContainerObserver.h"

#include "imgILoad.h"

#include "nsRect.h"

//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation
// This is an adaptor between GIF2 and imgIDecoder

NS_IMPL_ISUPPORTS1(nsGIFDecoder2, imgIDecoder)

nsGIFDecoder2::nsGIFDecoder2()
  : mCurrentRow(-1)
  , mLastFlushedRow(-1)
  , mGIFStruct(nsnull)
  , mAlphaLine(nsnull)
  , mRGBLine(nsnull)
  , mBackgroundRGBIndex(0)
  , mCurrentPass(0)
  , mLastFlushedPass(0)
  , mGIFOpen(PR_FALSE)
{
}

nsGIFDecoder2::~nsGIFDecoder2(void)
{
  if (mAlphaLine)
    nsMemory::Free(mAlphaLine);

  if (mRGBLine)
    nsMemory::Free(mRGBLine);

  if (mGIFStruct) {
    gif_destroy(mGIFStruct);
    mGIFStruct = nsnull;
  }
}

//******************************************************************************
/** imgIDecoder methods **/
//******************************************************************************

//******************************************************************************
/* void init (in imgILoad aLoad); */
NS_IMETHODIMP nsGIFDecoder2::Init(imgILoad *aLoad)
{
  mObserver = do_QueryInterface(aLoad);

  mImageContainer = do_CreateInstance("@mozilla.org/image/container;1?type=image/gif");
  aLoad->SetImage(mImageContainer);
  
  /* do gif init stuff */
  /* Always decode to 24 bit pixdepth */
  
  mGIFStruct = (gif_struct *)PR_CALLOC(sizeof(gif_struct));
  NS_ASSERTION(mGIFStruct, "gif_create failed");
  if (!mGIFStruct)
    return NS_ERROR_FAILURE;

  // Call GIF decoder init routine
  GIFInit(mGIFStruct, this);

  return NS_OK;
}




//******************************************************************************
/** nsIOutputStream methods **/
//******************************************************************************

//******************************************************************************
/* void close (); */
NS_IMETHODIMP nsGIFDecoder2::Close()
{
  if (mGIFStruct) {
    nsGIFDecoder2 *decoder = NS_STATIC_CAST(nsGIFDecoder2*, mGIFStruct->clientptr);
    if (decoder->mImageFrame)
      EndImageFrame(mGIFStruct->clientptr, mGIFStruct->images_decoded + 1, 
                    mGIFStruct->delay_time);
    if (decoder->mGIFOpen)
      EndGIF(mGIFStruct->clientptr, mGIFStruct->loop_count);
    gif_destroy(mGIFStruct);
    mGIFStruct = nsnull;
  }

  return NS_OK;
}

//******************************************************************************
/* void flush (); */
NS_IMETHODIMP nsGIFDecoder2::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* static callback from nsIInputStream::ReadSegments */
static NS_METHOD ReadDataOut(nsIInputStream* in,
                             void* closure,
                             const char* fromRawSegment,
                             PRUint32 toOffset,
                             PRUint32 count,
                             PRUint32 *writeCount)
{
  nsGIFDecoder2 *decoder = NS_STATIC_CAST(nsGIFDecoder2*, closure);
  nsresult rv = decoder->ProcessData((unsigned char*)fromRawSegment, count, writeCount);
  if (NS_FAILED(rv)) {
    *writeCount = 0;
    return rv;
  }

  return NS_OK;
}

// Push any new rows according to mCurrentPass/mLastFlushedPass and
// mCurrentRow/mLastFlushedRow.  Note: caller is responsible for
// updating mlastFlushed{Row,Pass}.
NS_METHOD
nsGIFDecoder2::FlushImageData()
{
  PRInt32 imgWidth;
  mImageContainer->GetWidth(&imgWidth);
  nsRect frameRect;
  mImageFrame->GetRect(frameRect);
  
  switch (mCurrentPass - mLastFlushedPass) {
    case 0: {  // same pass
      PRInt32 remainingRows = mCurrentRow - mLastFlushedRow;
      if (remainingRows) {
        nsRect r(0, frameRect.y + mLastFlushedRow + 1,
                 imgWidth, remainingRows);
        mObserver->OnDataAvailable(nsnull, mImageFrame, &r);
      }    
    }
    break;
  
    case 1: {  // one pass on - need to handle bottom & top rects
      nsRect r(0, frameRect.y, imgWidth, mCurrentRow + 1);
      mObserver->OnDataAvailable(nsnull, mImageFrame, &r);
      nsRect r2(0, frameRect.y + mLastFlushedRow + 1,
                imgWidth, frameRect.height - mLastFlushedRow - 1);
      mObserver->OnDataAvailable(nsnull, mImageFrame, &r2);
    }
    break;

    default: {  // more than one pass on - push the whole frame
      nsRect r(0, frameRect.y, imgWidth, frameRect.height);
      mObserver->OnDataAvailable(nsnull, mImageFrame, &r);
    }
  }

  return NS_OK;
}

//******************************************************************************
nsresult nsGIFDecoder2::ProcessData(unsigned char *data, PRUint32 count, PRUint32 *_retval)
{
  // Push the data to the GIF decoder
  
  // First we ask if the gif decoder is ready for more data, and if so, push it.
  // In the new decoder, we should always be able to process more data since
  // we don't wait to decode each frame in an animation now.
  if (gif_write_ready(mGIFStruct)) {
    PRStatus result = gif_write(mGIFStruct, data, count);
    if (result != PR_SUCCESS)
      return NS_ERROR_FAILURE;
  }

  if (mImageFrame && mObserver) {
    FlushImageData();
    mLastFlushedRow = mCurrentRow;
    mLastFlushedPass = mCurrentPass;
  }

  *_retval = count;

  return NS_OK;
}

//******************************************************************************
/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsGIFDecoder2::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  nsresult rv = inStr->ReadSegments(ReadDataOut, this,  count, _retval);

  /* necko doesn't propagate the errors from ReadDataOut - take matters
     into our own hands */
  if (NS_SUCCEEDED(rv) && mGIFStruct && mGIFStruct->state == gif_error)
    return NS_ERROR_FAILURE;

  return rv;
}


//******************************************************************************
// GIF decoder callback methods. Part of pulic API for GIF2
//******************************************************************************

//******************************************************************************
int nsGIFDecoder2::BeginGIF(
  void*    aClientData,
  PRUint32 aLogicalScreenWidth, 
  PRUint32 aLogicalScreenHeight,
  PRUint8  aBackgroundRGBIndex)
{
  // If we have passed an illogical screen size, bail and hope that we'll get
  // set later by the first frame's local image header.
  if(aLogicalScreenWidth == 0 || aLogicalScreenHeight == 0)
    return 0;
    
  // copy GIF info into imagelib structs
  nsGIFDecoder2 *decoder = NS_STATIC_CAST(nsGIFDecoder2*, aClientData);

  decoder->mBackgroundRGBIndex = aBackgroundRGBIndex;

  if (decoder->mObserver)
    decoder->mObserver->OnStartDecode(nsnull);

  decoder->mImageContainer->Init(aLogicalScreenWidth, aLogicalScreenHeight, decoder->mObserver);

  if (decoder->mObserver)
    decoder->mObserver->OnStartContainer(nsnull, decoder->mImageContainer);

  decoder->mGIFOpen = PR_TRUE;
  return 0;
}

//******************************************************************************
int nsGIFDecoder2::EndGIF(
    void*    aClientData,
    int      aAnimationLoopCount)
{
  nsGIFDecoder2 *decoder = NS_STATIC_CAST(nsGIFDecoder2*, aClientData);
  if (decoder->mObserver) {
    decoder->mObserver->OnStopContainer(nsnull, decoder->mImageContainer);
    decoder->mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
  }
  
  decoder->mImageContainer->SetLoopCount(aAnimationLoopCount);
  decoder->mImageContainer->DecodingComplete();

  decoder->mGIFOpen = PR_FALSE;
  return 0;
}

//******************************************************************************
int nsGIFDecoder2::BeginImageFrame(
  void*    aClientData,
  PRUint32 aFrameNumber,   /* Frame number, 1-n */
  PRUint32 aFrameXOffset,  /* X offset in logical screen */
  PRUint32 aFrameYOffset,  /* Y offset in logical screen */
  PRUint32 aFrameWidth,    
  PRUint32 aFrameHeight)
{
  nsGIFDecoder2* decoder = NS_STATIC_CAST(nsGIFDecoder2*, aClientData);
  
  decoder->mImageFrame = nsnull; // clear out our current frame reference
  decoder->mGIFStruct->x_offset = aFrameXOffset;
  decoder->mGIFStruct->y_offset = aFrameYOffset;
  decoder->mGIFStruct->width = aFrameWidth;
  decoder->mGIFStruct->height = aFrameHeight;

  if (aFrameNumber == 1) {
    // Send a onetime OnDataAvailable (Display Refresh) for the first frame
    // if it has a y-axis offset.  Otherwise, the area may never be refreshed
    // and the placeholder will remain on the screen. (Bug 37589)
    PRInt32 imgWidth;
    decoder->mImageContainer->GetWidth(&imgWidth);
    if (aFrameYOffset > 0) {
      nsRect r(0, 0, imgWidth, aFrameYOffset);
      decoder->mObserver->OnDataAvailable(nsnull, decoder->mImageFrame, &r);
    }
  }

  return 0;
}

//******************************************************************************
int nsGIFDecoder2::EndImageFrame(
  void*    aClientData, 
  PRUint32 aFrameNumber,
  PRUint32 aDelayTimeout)  /* Time this frame should be displayed before the next frame 
                              we can't have this in the image frame init because it doesn't
                              show up in the GIF frame header, it shows up in a sub control
                              block.*/
{
  nsGIFDecoder2* decoder = NS_STATIC_CAST(nsGIFDecoder2*, aClientData);
  
  // If mImageFrame hasn't been initialized, call HaveDecodedRow to init it
  // One reason why it may not be initialized is because the frame
  // is out of the bounds of the image.
  if (!decoder->mImageFrame) {
    HaveDecodedRow(aClientData,nsnull,0,0,0);
  } else {
    // We actually have the timeout information before we get the lzw encoded 
    // image data, at least according to the spec, but we delay in setting the 
    // timeout for the image until here to help ensure that we have the whole 
    // image frame decoded before we go off and try to display another frame.
    decoder->mImageFrame->SetTimeout(aDelayTimeout);
  }
  decoder->mImageContainer->EndFrameDecode(aFrameNumber, aDelayTimeout);

  // if the gif is corrupt don't mark the frame as complete, as nsCSSRendering
  // will happily try using it to draw a background
  if (decoder->mObserver && 
      decoder->mImageFrame && 
      decoder->mGIFStruct->state != gif_error) {
    decoder->FlushImageData();

    if (aFrameNumber == 1) {
      // If the first frame is smaller in height than the entire image, send a
      // OnDataAvailable (Display Refresh) for the area it does not have data for.
      // This will clear the remaining bits of the placeholder. (Bug 37589)
      PRInt32 imgHeight;
      PRInt32 realFrameHeight = decoder->mGIFStruct->height + decoder->mGIFStruct->y_offset;

      decoder->mImageContainer->GetHeight(&imgHeight);
      if (imgHeight > realFrameHeight) {
        PRInt32 imgWidth;
        decoder->mImageContainer->GetWidth(&imgWidth);

        nsRect r(0, realFrameHeight, imgWidth, imgHeight - realFrameHeight);
        decoder->mObserver->OnDataAvailable(nsnull, decoder->mImageFrame, &r);
      }
    }

    decoder->mCurrentRow = decoder->mLastFlushedRow = -1;
    decoder->mCurrentPass = decoder->mLastFlushedPass = 0;

    decoder->mObserver->OnStopFrame(nsnull, decoder->mImageFrame);
  }

  decoder->mImageFrame = nsnull;
  PR_FREEIF(decoder->mGIFStruct->local_colormap);
  decoder->mGIFStruct->is_transparent = PR_FALSE;
  return 0;
}
  
//******************************************************************************
// GIF decoder callback notification that it has decoded a row
int nsGIFDecoder2::HaveDecodedRow(
  void* aClientData,
  PRUint8* aRowBufPtr,   // Pointer to single scanline temporary buffer
  int aRowNumber,        // Row number?
  int aDuplicateCount,   // Number of times to duplicate the row?
  int aInterlacePass)    // interlace pass (1-4)
{
  nsGIFDecoder2* decoder = NS_STATIC_CAST(nsGIFDecoder2*, aClientData);
  PRUint32 bpr, abpr;
  // We have to delay allocation of the image frame until now because
  // we won't have control block info (transparency) until now. The conrol
  // block of a GIF stream shows up after the image header since transparency
  // is added in GIF89a and control blocks are how the extensions are done.
  // How annoying.
  if(! decoder->mImageFrame) {
    gfx_format format = gfxIFormats::RGB;
    if (decoder->mGIFStruct->is_transparent) {
      format = gfxIFormats::RGB_A1;
    }

#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
    // XXX this works...
    format += 1; // RGB to BGR
#endif

    // initalize the frame and append it to the container
    decoder->mImageFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (!decoder->mImageFrame || NS_FAILED(decoder->mImageFrame->Init(
          decoder->mGIFStruct->x_offset, decoder->mGIFStruct->y_offset, 
          decoder->mGIFStruct->width, decoder->mGIFStruct->height, format, 24))) {
      decoder->mImageFrame = 0;
      return 0;
    }

    decoder->mImageFrame->SetFrameDisposalMethod(decoder->mGIFStruct->disposal_method);
    decoder->mImageContainer->AppendFrame(decoder->mImageFrame);

    if (decoder->mObserver)
      decoder->mObserver->OnStartFrame(nsnull, decoder->mImageFrame);

    decoder->mImageFrame->GetImageBytesPerRow(&bpr);
    decoder->mImageFrame->GetAlphaBytesPerRow(&abpr);

    decoder->mRGBLine = (PRUint8 *)nsMemory::Realloc(decoder->mRGBLine, bpr);

    if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::BGR_A1) {
      decoder->mAlphaLine = (PRUint8 *)nsMemory::Realloc(decoder->mAlphaLine, abpr);
    }
  } else {
    decoder->mImageFrame->GetImageBytesPerRow(&bpr);
    decoder->mImageFrame->GetAlphaBytesPerRow(&abpr);
  }
  
  if (aRowBufPtr) {
    nscoord width;

    decoder->mImageFrame->GetWidth(&width);

    gfx_format format;
    decoder->mImageFrame->GetFormat(&format);

    // XXX map the data into colors
    int cmapsize;
    PRUint8* cmap;
    cmapsize = decoder->mGIFStruct->global_colormap_size;
    cmap = decoder->mGIFStruct->global_colormap;

    if(decoder->mGIFStruct->global_colormap &&
       decoder->mGIFStruct->screen_bgcolor < cmapsize) {
      gfx_color bgColor = 0;
      PRUint32 bgIndex = decoder->mGIFStruct->screen_bgcolor * 3;
      bgColor |= cmap[bgIndex];
      bgColor |= cmap[bgIndex + 1] << 8;
      bgColor |= cmap[bgIndex + 2] << 16;
      decoder->mImageFrame->SetBackgroundColor(bgColor);
    }
    if (decoder->mGIFStruct->is_local_colormap_defined) {
      cmapsize = decoder->mGIFStruct->local_colormap_size;
      cmap = decoder->mGIFStruct->local_colormap;
    }

    if (!cmap) { // cmap could have null value if the global color table flag is 0
      for (int i = 0; i < aDuplicateCount; ++i) {
        if (format == gfxIFormats::RGB_A1 ||
            format == gfxIFormats::BGR_A1) {
          decoder->mImageFrame->SetAlphaData(nsnull,
                                             abpr, (aRowNumber+i)*abpr);
        }
        decoder->mImageFrame->SetImageData(nsnull,
                                           bpr, (aRowNumber+i)*bpr);
      }
    } else {
      PRUint8* rgbRowIndex = decoder->mRGBLine;
      PRUint8* rowBufIndex = aRowBufPtr;
      
      switch (format) {
        case gfxIFormats::RGB:
        case gfxIFormats::BGR:
        {
          while (rowBufIndex != decoder->mGIFStruct->rowend) {
#if defined(XP_MAC) || defined(XP_MACOSX)
            *rgbRowIndex++ = 0; // Mac is always 32bits per pixel, this is pad
#endif
            if (*rowBufIndex < cmapsize) {
              PRUint32 colorIndex = *rowBufIndex * 3;
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
              *rgbRowIndex++ = cmap[colorIndex + 2]; // blue
              *rgbRowIndex++ = cmap[colorIndex + 1]; // green
              *rgbRowIndex++ = cmap[colorIndex];     // red
#else
              *rgbRowIndex++ = cmap[colorIndex];     // red
              *rgbRowIndex++ = cmap[colorIndex + 1]; // green
              *rgbRowIndex++ = cmap[colorIndex + 2]; // blue
#endif
            } else {
              *rgbRowIndex++ = 0;                    // red
              *rgbRowIndex++ = 0;                    // green
              *rgbRowIndex++ = 0;                    // blue
            }
            ++rowBufIndex;
          }  
          for (int i=0; i<aDuplicateCount; i++) {
            decoder->mImageFrame->SetImageData(decoder->mRGBLine,
                                               bpr, (aRowNumber+i)*bpr);
          }
          break;
        }
        case gfxIFormats::RGB_A1:
        case gfxIFormats::BGR_A1:
        {
          memset(decoder->mRGBLine, 0, bpr);
          memset(decoder->mAlphaLine, 0, abpr);
          for (PRUint32 x = 0; x < (PRUint32)width; ++x) {
            if (*rowBufIndex != decoder->mGIFStruct->tpixel) {
#if defined(XP_MAC) || defined(XP_MACOSX)
              *rgbRowIndex++ = 0; // Mac is always 32bits per pixel, this is pad
#endif
              if (*rowBufIndex < cmapsize) {
                PRUint32 colorIndex = *rowBufIndex * 3;
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
                *rgbRowIndex++ = cmap[colorIndex + 2]; // blue
                *rgbRowIndex++ = cmap[colorIndex + 1]; // green
                *rgbRowIndex++ = cmap[colorIndex];     // red
#else
                *rgbRowIndex++ = cmap[colorIndex];     // red
                *rgbRowIndex++ = cmap[colorIndex + 1]; // green
                *rgbRowIndex++ = cmap[colorIndex + 2]; // blue
#endif
              } else {
                *rgbRowIndex++ = 0;                    // red
                *rgbRowIndex++ = 0;                    // green
                *rgbRowIndex++ = 0;                    // blue
              }
              decoder->mAlphaLine[x>>3] |= 1<<(7-x&0x7);
            } else {
#if defined(XP_MAC) || defined(XP_MACOSX)
              rgbRowIndex+=4;
#else
              rgbRowIndex+=3;
#endif
            }
            ++rowBufIndex;
          }
          for (int i=0; i<aDuplicateCount; i++) {
            decoder->mImageFrame->SetAlphaData(decoder->mAlphaLine,
                                               abpr, (aRowNumber+i)*abpr);
            decoder->mImageFrame->SetImageData(decoder->mRGBLine,
                                               bpr, (aRowNumber+i)*bpr);
          }
          break;
        }
      }
    }

    decoder->mCurrentRow = aRowNumber + aDuplicateCount - 1;
    decoder->mCurrentPass = aInterlacePass;
    if (aInterlacePass == 1)
      decoder->mLastFlushedPass = aInterlacePass;   // interlaced starts at 1
  }

  return 0;
}
