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

#include "nsMNGDecoder.h"
#include "nsIImgDCallbk.h"
#include "nsMemory.h"
#include "libmng.h"

// Define this if you just want an RGB (no alpha) target
//#define NO_ALPHA

#ifdef NO_ALPHA
#define CHANNELS 3
#else
#define CHANNELS 4
#endif

// Decrease this to a sane value after this bug is fixed:
//   http://bugzilla.mozilla.org/show_bug.cgi?id=41831
#define MOZ_MNG_BUFSIZE 262144

typedef struct ipng_str {

  mng_handle handle;
  PRUint32 width;
  PRUint32 height;
  PRUint8 *image;               /* RGBA full image buffer */
  PRUint8 *rowbuf;              /* ImgDCBHaveRow is destructive.  Grrr... */

  void *timer_id;
  
  PRUint8 writeBuffer[MOZ_MNG_BUFSIZE];   /* Circular buffer */
  PRUint32 bytesBuffered;
  PRUint32 writeStart;

  PRUint32 bytesNeeded;      /* number of bytes libmng wants (to know when */
                             /*   we can call read_resume) */
  
  il_container *ic;

} imng_struct, *imng_structp;


#define EXTRACT_STRUCTS \
  il_container *ic = (il_container *)mng_get_userdata(handle); \
  imng_structp imng_p = (imng_structp)ic->ds

#ifdef DEBUG_tor
#define dprintf(x) fprintf x
#else
#define dprintf(x) 
#endif

// Callbacks for libmng
//===========================================================

static mng_bool
il_mng_openstream(mng_handle handle)
{
  return MNG_TRUE;
}


static mng_bool
il_mng_closestream(mng_handle handle)
{
  return MNG_TRUE;
}

static mng_bool
il_mng_readdata(mng_handle handle, mng_ptr buf,
                mng_uint32 size, mng_uint32 *stored)
{
  EXTRACT_STRUCTS;

  if (imng_p->bytesBuffered >= size) {
    PRUint32 endChunk = (MOZ_MNG_BUFSIZE-imng_p->writeStart);

    memcpy(buf, imng_p->writeBuffer+imng_p->writeStart,
           PR_MIN(endChunk, size));
    
    if (endChunk < size)
      memcpy((PRUint8 *)buf+endChunk, imng_p->writeBuffer, size-endChunk);
    
    imng_p->bytesBuffered -= size;
    imng_p->writeStart = (imng_p->writeStart+size) % MOZ_MNG_BUFSIZE;
    imng_p->bytesNeeded = 0;

    *stored = size;
    return MNG_TRUE;
  } else {
    imng_p->bytesNeeded = size;

    *stored = 0;
    return MNG_FALSE;
  }
}

static mng_bool
il_mng_processheader(mng_handle handle, mng_uint32 width, mng_uint32 height)
{
  EXTRACT_STRUCTS;

  imng_p->width = width;
  imng_p->height = height;
  ic->src_header->width = width;
  ic->src_header->height = height;

#ifndef NO_ALPHA
  if (mng_get_simplicity(handle) & MNG_SIMPLICITY_TRANSPARENCY) {
    dprintf((stderr, "--- MNG ALPHA 8-BIT\n"));
    ic->image->header.alpha_bits = 8;
  } else {
    dprintf((stderr, "--- MNG ALPHA THRESHHOLD\n"));
    ic->image->header.alpha_bits = 1;
  }
  ic->image->header.alpha_shift = 0;
  ic->image->header.is_interleaved_alpha = TRUE;
#endif

  imng_p->image =
    (unsigned char*)nsMemory::Alloc(CHANNELS*width*height);
  memset(imng_p->image, 0, CHANNELS*width*height);

  imng_p->rowbuf = (unsigned char*)nsMemory::Alloc(CHANNELS*width);

  ic->imgdcb->ImgDCBImageSize();
  ic->imgdcb->ImgDCBSetupColorspaceConverter(); 

  return MNG_TRUE;
}

static mng_ptr
il_mng_getcanvasline(mng_handle handle, mng_uint32 iLinenr)
{
  EXTRACT_STRUCTS;

  return imng_p->image+CHANNELS*imng_p->width*iLinenr;
}

static mng_bool
il_mng_refresh(mng_handle handle,
	       mng_uint32 left, mng_uint32 top,
	       mng_uint32 width, mng_uint32 height)
{
//  dprintf((stderr, "=== refresh(top=%d left=%d width=%d height=%d)\n",
//           top, left, width, height));

  EXTRACT_STRUCTS;

  for (mng_uint32 y=top; y<top+height; y++) {
    memcpy(imng_p->rowbuf, 
           imng_p->image+y*CHANNELS*imng_p->width,
           CHANNELS*imng_p->width);
    ic->imgdcb->ImgDCBHaveRow(0              /* color index data */,
                              imng_p->rowbuf /* rgb[a] */,
                              0              /* x-offset */,
                              imng_p->width  /* width in pixels */,
                              y              /* start row */,
                              1              /* row duplication count */,
                              ilErase        /* draw mode */,
                              0              /* pass */);
  }

  return MNG_TRUE;
}


static mng_uint32
il_mng_gettickcount(mng_handle handle)
{
//  dprintf((stderr, "=== gettickcount()\n"));
  return PR_IntervalToMilliseconds(PR_IntervalNow());
}

static void
il_mng_timeout_func(void *data)
{
  mng_handle handle = (mng_handle)data;
  EXTRACT_STRUCTS;

//  dprintf((stderr, "il_mng_timeout_func\n"));

  imng_p->timer_id = 0;
  mng_display_resume(handle);
}

static mng_bool
il_mng_settimer(mng_handle handle, mng_uint32 msec)
{
  dprintf((stderr, "=== settimer(%d)\n", msec));

  EXTRACT_STRUCTS;

  imng_p->timer_id = 
    ic->imgdcb->ImgDCBSetTimeout(il_mng_timeout_func,
                                 (void *)handle,
                                 msec);
  return MNG_TRUE;
}  

static mng_ptr
il_mng_alloc(mng_size_t size)
{
  void *ptr = nsMemory::Alloc(size);
  memset(ptr, 0, size);
  return ptr;
}

static void
il_mng_free(mng_ptr ptr, mng_size_t size)
{
  nsMemory::Free(ptr);
}


// Boilerplate methods... *yawn*
//===========================================================

MNGDecoder::MNGDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
}


MNGDecoder::~MNGDecoder(void)
{
}


NS_IMPL_ISUPPORTS1(MNGDecoder, nsIImgDecoder)


NS_METHOD
MNGDecoder::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  il_container *ic = new il_container();
  if (!ic) return NS_ERROR_OUT_OF_MEMORY;

  MNGDecoder *decoder = new MNGDecoder(ic);
  if (!decoder) {
    delete ic;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(decoder);
  rv = decoder->QueryInterface(aIID, aResult);
  NS_RELEASE(decoder);

  /* why are we creating and destroying this object for no reason? */
  delete ic; /* is a place holder */

  return rv;
}

// Hooking mozilla and libmng together...
//===========================================================

NS_IMETHODIMP
MNGDecoder::ImgDInit()
{
  if( ilContainer != NULL ) {
    imng_structp imng_p;
    imng_p = (imng_structp) nsMemory::Alloc(sizeof(imng_struct));
    memset(imng_p, 0, sizeof(imng_struct));
    if (!imng_p) 
        return PR_FALSE;
    
    ilContainer->image->header.width = ilContainer->dest_width;
    ilContainer->image->header.height = ilContainer->dest_height;
    ilContainer->ds = imng_p;
    imng_p->ic = ilContainer;
    
    /* Initialize the container's source image header. */
    /* Always decode to 24 bit pixdepth */
    
    NI_ColorSpace *src_color_space = ilContainer->src_header->color_space;
    src_color_space->type = NI_TrueColor;
    src_color_space->pixmap_depth = 24;
    src_color_space->bit_alloc.index_depth = 0;
    
    /* pass ic as user data */
    imng_p->handle = 
      mng_initialize(ilContainer, il_mng_alloc, il_mng_free, NULL);

////////////
// Gamma correction - gross hack, but it's what mozilla's PNG
//                    decoder does and nobody has complained yet...
    double LUT_exponent, CRT_exponent = 2.2, display_exponent;

    /* set up gamma correction for Mac, Unix and (Win32 and everything else)
     * using educated guesses for display-system exponents; do preferences
     * later */

#if defined(XP_MAC)
    LUT_exponent = 1.8 / 2.61;
#elif defined(XP_UNIX)
# if defined(__sgi)
    LUT_exponent = 1.0 / 1.7;   /* typical default for SGI console */
# elif defined(NeXT)
    LUT_exponent = 1.0 / 2.2;   /* typical default for NeXT cube */
# else
    LUT_exponent = 1.0;         /* default for most other Unix workstations */
# endif
#else
    LUT_exponent = 1.0;         /* virtually all PCs and most other systems */
#endif

    /* (alternatively, could check for SCREEN_GAMMA environment variable) */
    display_exponent = LUT_exponent * CRT_exponent;
    mng_set_dfltimggamma(imng_p->handle, 0.45455);
    mng_set_displaygamma(imng_p->handle, display_exponent);
////////////

#ifndef NO_ALPHA
    mng_set_canvasstyle(imng_p->handle, MNG_CANVAS_RGBA8);
#endif

    mng_setcb_openstream(imng_p->handle, il_mng_openstream);
    mng_setcb_closestream(imng_p->handle, il_mng_closestream);
    mng_setcb_readdata(imng_p->handle, il_mng_readdata);
    mng_setcb_processheader(imng_p->handle, il_mng_processheader);
    mng_setcb_getcanvasline(imng_p->handle, il_mng_getcanvasline);
    mng_setcb_refresh(imng_p->handle, il_mng_refresh);
    mng_setcb_gettickcount(imng_p->handle, il_mng_gettickcount);
    mng_setcb_settimer(imng_p->handle, il_mng_settimer);
    mng_setcb_memalloc(imng_p->handle, il_mng_alloc);
    mng_setcb_memfree(imng_p->handle, il_mng_free);

    mng_readdisplay(imng_p->handle);
  }
  return NS_OK;
}


NS_IMETHODIMP 
MNGDecoder::ImgDWriteReady(PRUint32 *max_read)
{
  dprintf((stderr, "MNG::ImgDWriteReady() = "));

  imng_structp imng_p = (imng_structp)ilContainer->ds;

  *max_read = MOZ_MNG_BUFSIZE-imng_p->bytesBuffered;
  
  dprintf((stderr, "%d\n", *max_read));

  return NS_OK;
}


NS_IMETHODIMP
MNGDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  dprintf((stderr, "MNG::ImgDWrite(%d)\n", len));

  if (ilContainer != NULL) {
    imng_structp imng_p = (imng_structp)ilContainer->ds;
    
    if (PRUint32(len)>MOZ_MNG_BUFSIZE-imng_p->bytesBuffered) {
      fprintf(stderr, "MNG too large - abort, abort!\n");
      fprintf(stderr, " http://bugzilla.mozilla.org/show_bug.cgi?id=41831\n");
      return NS_ERROR_FAILURE;
    }

    int32 endChunk =
      (MOZ_MNG_BUFSIZE-(imng_p->writeStart+imng_p->bytesBuffered));
    memcpy(imng_p->writeBuffer+imng_p->writeStart+imng_p->bytesBuffered,
           buf, PR_MIN(endChunk,len));
    if (endChunk < len)
      memcpy(imng_p->writeBuffer, buf+endChunk, len-endChunk);
    imng_p->bytesBuffered += len;

    if (imng_p->bytesNeeded && 
        (imng_p->bytesBuffered >= imng_p->bytesNeeded)) {
      mng_read_resume(imng_p->handle);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP 
MNGDecoder::ImgDComplete()
{
  return NS_OK;
}


NS_IMETHODIMP 
MNGDecoder::ImgDAbort()
{
  if( ilContainer != NULL ) {
    imng_structp imng_p = (imng_structp)ilContainer->ds;

    if (!imng_p)
      return NS_OK;

    if (imng_p->timer_id) {
      ilContainer->imgdcb->ImgDCBClearTimeout(imng_p->timer_id);
      imng_p->timer_id = 0;
    }

    mng_display_freeze(imng_p->handle);
    mng_cleanup(&imng_p->handle);
    nsMemory::Free(imng_p->image);
    nsMemory::Free(imng_p->rowbuf);
    nsMemory::Free(imng_p);
    imng_p = NULL;
  }
  return NS_OK;
}

