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

/* -*- Mode: C; tab-width: 4 -*-
 *  il_types.h --- Image library data types and structures.
 *  $Id: il_types.h,v 3.6 2000/07/20 01:49:58 pnunn%netscape.com Exp $
 */


#ifndef _IL_TYPES_H_
#define _IL_TYPES_H_

#include "prtypes.h"

#define IL_EXTERN(__type) PR_EXTERN(__type)
#define IL_IMPLEMENT(__type) PR_IMPLEMENT(__type)

/************************** Notes ********************************************
* 1. Required #defines:
*    IL_CLIENT - This should only be defined by clients of the Image Library.
*    IL_INTERNAL - Defined only by the Image Library.
*
* 2. The Image Library clients and display front ends should only use types
*    with an IL_ prefix, instead of the equivalent types with an NI_ prefix.
*    The NI_ prefix will be used by Image Plugins. 
******************************************************************************/

/************************** Miscellaneous Data Types *************************/

/* Flags to be passed into IL_GetImage. */
#define IL_HIGH_PRIORITY    0x01   /* For important images, like backdrops. */
#define IL_STICKY           0x02   /* Don't throw this image out of cache.  */
#define IL_BYPASS_CACHE     0x04   /* Don't get image out of image cache.   */
#define IL_ONLY_FROM_CACHE  0x08   /* Don't load if image cache misses.     */

typedef enum
{   
     USE_IMG_CACHE,        /* use imgcache */
     DONT_USE_IMG_CACHE,   /* dont use imgcache AND if found in imgcache delete entry */
     SYNTH_IMGDOC_NEEDS_IMG_CACHE   /* needed for view-image/image doc */
     
} ImgCachePolicy;



/* A rectangle structure. */
typedef struct _IL_Rect {
  PRUint16 x_origin;
  PRUint16 y_origin;
  PRUint16 width;
  PRUint16 height;
} IL_Rect;

/* Pixmap control messages issued by the imagelib to indicate that it
   has acquired or relinquished control over a IL_Pixmap's bits. */
typedef enum {
    IL_LOCK_BITS,               /* Issued prior to modifying the bits. */
    IL_UNLOCK_BITS,             /* Issued after modifying the bits. */
    IL_RELEASE_BITS             /* Issued once the bits will no longer
                                   be modified or accessed by the imagelib. */
} IL_PixmapControl;


/*********** Cross Platform Pixmaps and Colorspace Information ***************/

/* The IL_Pixmap data structure is a cross-platform representation of a
   pixmap, which contains information about the dimensions, colorspace and
   bits of an image.  The IL_Pixmap structure is exported to the various
   Front Ends through the header file ni_pixmap.h, and it will also be
   exported to image decoder plugins, when they become available. */

#ifdef IL_CLIENT

typedef void IL_Pixmap;         /* Cross platform pixmap. */
typedef void IL_ColorSpace;     /* Colorspace. */

/* An indexed RGB triplet. */
typedef struct _IL_IRGB {
    PRUint8 index;
    PRUint8 red, green, blue;
} IL_IRGB;

/* A RGB triplet representing a single pixel in the image's colormap
   (if present.) */
typedef struct _IL_RGB
{
    PRUint8 red, green, blue, pad; /* Windows requires the fourth byte &
                                    many compilers pad it anyway. */
    PRUint16 hist_count;           /* Histogram frequency count. */
} IL_RGB;

#else  /* Image Library and Front Ends. */

#include "ni_pixmp.h"
typedef NI_Pixmap IL_Pixmap;
typedef NI_ColorMap IL_ColorMap;
typedef NI_ColorSpace IL_ColorSpace;
typedef NI_RGBBits IL_RGBBits;  /* RGB bit allocation. */
typedef NI_IRGB IL_IRGB;
typedef NI_RGB IL_RGB;

#endif /* IL_CLIENT */


/*********************** Image Contexts and Handles **************************/

#ifndef IL_INTERNAL
/* A temporary placeholder for a cross-platform representation of
   shared netlib state to be passed to netlib functions.  This will
   actually be defined by netlib in some other header file.  */
/* typedef void NetGpCxt;*/

/* An opaque handle to an instance of a single image request, passed
   as the return value of IL_GetImage().  Although multiple requests
   for the same URL may be coalesced within the image library, a
   unique IL_ImageReq handle is still generated for every request. */
typedef void IL_ImageReq;

/* An opaque handle to shared image state and callback functions.
   Image library API functions are provided to perform certain
   operations on all IL_ImageReq's that share the same IL_GroupContext.
   The expectation is that many images will share a single
   IL_GroupContext, i.e. all the images in a document or window. */
typedef void IL_GroupContext;

#endif /* IL_INTERNAL */


/*********************** Image Observer Notification *************************
* - for observers of an IL_ImageReq.
******************************************************************************/

/* Possible image observer notification messages. */
enum {
  IL_START_URL,                 /* Start of decode/display for URL. */
  IL_DESCRIPTION,               /* Availability of image description. */
  IL_DIMENSIONS,                /* Availability of image dimensions. */
  IL_IS_TRANSPARENT,            /* Is this image transparent. */
  IL_PIXMAP_UPDATE,             /* Change in a rectangular area of pixels. */
  IL_FRAME_COMPLETE,            /* Completion of a frame of an animated
                                   image.*/
  IL_PROGRESS,                  /* Notification of percentage decoded. */
  IL_IMAGE_COMPLETE,            /* Completion of image decoding.  There
                                   may be multiple instances of this
                                   event per URL due to server push,
                                   client pull or looping GIF animation. */
  IL_STOP_URL,                  /* Completion of decode/display for URL. */
  IL_IMAGE_DESTROYED,           /* Finalization of an image request.  This
                                   is an indication to remove observers from
                                   the observer list and to perform any
                                   observer related cleanup. */
  IL_ABORTED,                   /* Image decode was aborted by either
                                   netlib or IL_InterruptContext(). */
  IL_NOT_IN_CACHE,              /* Image URL not available in cache when 
                                   IL_ONLY_FROM_CACHE flag was set. */
  IL_ERROR_NO_DATA,             /* Netlib unable to fetch provided URL. */
  IL_ERROR_IMAGE_DATA_CORRUPT,	/* Checksum error of some kind in image
                                   data. */
  IL_ERROR_IMAGE_DATA_TRUNCATED, /* Missing data at end of stream. */
  IL_ERROR_IMAGE_DATA_ILLEGAL,	/* Generic image data error. */
  IL_INTERNAL_IMAGE,            /* Internal image icon. */
  IL_ERROR_INTERNAL             /* Internal Image Library error. */
};


/* Message data passed to the client-provided image observer callbacks. */
typedef struct {
    void *display_context;      /* The Front End display context associated
                                   with the image group context. */
    IL_ImageReq *image_instance; /* The image being observed. */

    /* Data for IL_DESCRIPTION message. */
    char *description;          /* Human readable description of an image, e.g.
                                   "GIF89a 320 x 240 pixels".  The string
                                   storage is static, so it must be copied if
                                   it is to be preserved after the call to the
                                   observer. The intention is that this can be
                                   used to title a document window. */

    /* Data for IL_PIXMAP_UPDATE message. */
    IL_Rect update_rect;        /* A rectangular area of pixels which has been
                                   modified by the image library.  This
                                   notification enables the client to drive
                                   the display via an immediate or deferred
                                   call to IL_DisplaySubImage.  */

    /* Data for IL_PROGRESS message. */
    int percent_progress;       /* Estimated percentage decoded.  This
                                   notification occurs at unspecified
                                   intervals.  Provided that decoding proceeds
                                   without error, it is guaranteed that
                                   notification will take place on completion
                                   with a percent_progress value of 100. */

    /* Data for IL_DIMENSIONS message. */
    PRUint16 width;               /* Image width. */
    PRUint16 height;              /* Image height. */

    /* Data for IL_INTERNAL_IMAGE message, or for error messages which require
       an icon to be displayed: IL_ERROR_NO_DATA, IL_ERROR_IMAGE_DATA_CORRUPT,
       IL_ERROR_IMAGE_DATA_TRUNCATED, IL_ERROR_IMAGE_DATA_ILLEGAL or
       IL_ERROR_INTERNAL. */
    PRUint16 icon_width;               /* Icon width. */
    PRUint16 icon_height;              /* Icon height. */
    int32 icon_number;               /* Icon number. */

} IL_MessageData;


/************************* Image Group Observer Notification *****************
* - for observers of an IL_GroupContext.
******************************************************************************/

/* Possible image group observer notification messages.  Note: animations are
   considered to have loaded at the end of the first iteration.  A looping
   animation is one which is on its second or a subsequent iteration. */
enum {
    IL_STARTED_LOADING,         /* Start of image loading.  Sent when a loading
                                   image is added to an image group which
                                   currently has no loading images. */
    IL_ABORTED_LOADING,         /* Some images were aborted.  A finished
                                   loading message will not be sent until the
                                   aborted images have been destroyed. */
    IL_FINISHED_LOADING,        /* End of image loading.  Sent when the last
                                   of the images currently in the image group
                                   has finished loading. */
    IL_STARTED_LOOPING,         /* Start of image looping.  Sent when an
                                   animated image starts looping in an image
                                   group which currently has no looping
                                   animations. */
    IL_FINISHED_LOOPING         /* End of image looping.  Sent when the last
                                   of the images currently in the image group
                                   has finished looping. */
};


/* Message data passed to the client-provided image group observer
   callbacks. */
typedef struct {
    void *display_context;          /* The Front End display context associated
                                       with the image group context. */
    IL_GroupContext *image_context; /* The image group context being
                                       observed. */
} IL_GroupMessageData;


/************************** Display Preferences ******************************/

/* Flags to be passed into IL_SetDisplayMode. */
#define IL_DISPLAY_CONTEXT     0x01
#define IL_DISPLAY_TYPE        0x02
#define IL_COLOR_SPACE         0x04
#define IL_PROGRESSIVE_DISPLAY 0x08
#define IL_DITHER_MODE         0x10

/* The display type. */
typedef enum {
    IL_Console    = 0,
    IL_Printer    = 1,
    IL_PostScript = 2,
    IL_MetaFile   = 3
} IL_DisplayType;

/* Dithering preference. */
typedef enum IL_DitherMode {
    IL_ClosestColor = 0,
    IL_Dither       = 1,
    IL_Auto         = 2
} IL_DitherMode;

/* Data for setting the display mode. */
typedef struct {
    void *display_context;       /* The FE's display context. */
    IL_DisplayType display_type; /* IL_Console, IL_Printer, IL_PostScript, or
                                    IL_MetaFile. */
    IL_ColorSpace *color_space;  /* Display colorspace. */
    PRBool progressive_display;  /* Toggle for progressive image display. */
    IL_DitherMode dither_mode;   /* IL_ClosestColor, IL_Dither or IL_Auto. */
} IL_DisplayData;

#endif /* _IL_TYPES_H_ */

