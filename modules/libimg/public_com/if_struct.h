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

/*   if.h --- Top-level image library internal routines
 *
 * $Id: if_struct.h,v 1.2 1999/04/19 22:22:47 pnunn%netscape.com Exp $
 */

#ifndef _if_h
#ifndef _ifstruct_h
#define _ifstruct_h

#define M12N

#define IL_INTERNAL

#include "prtypes.h"
#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "prtime.h"
#include "prlong.h"

#include "dllcompat.h"
#include "nsIImgDecoder.h"

#include "ntypes.h"             /* typedefs for commonly used Netscape data
                                   structures */

#ifndef STANDALONE_IMAGE_LIB
#include "net.h"
#endif /* STANDALONE_IMAGE_LIB */

typedef struct _IL_GroupContext IL_GroupContext;
typedef struct _IL_ImageReq IL_ImageReq;
typedef struct il_context_list il_context_list;
typedef struct il_container_list il_container_list;
typedef struct il_container_struct il_container;

#include "il_icons.h"           /* Image icons. */
#include "libimg.h"             /* Public API to Image Library. */
#include "il_utilp.h"           /* Private header for colormap/colorspaces. */
#include "il_util.h"            /* Public API for colormaps/colorspaces. */
#include "ilINetContext.h"
#include "ilIURL.h"
#include "ilINetReader.h"
#include "ilIImageRenderer.h"

/***************************** also in dllcompat.h ***********************/
#ifndef STANDALONE_IMAGE_LIB
PR_BEGIN_EXTERN_C
typedef void
(*TimeoutCallbackFunction) (void * closure);

extern void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs);
extern void
FE_ClearTimeout(void *timer_id);
PR_END_EXTERN_C
#endif
/********************************* in dllcompat.h *************************************/

#include "il.h"

/* For debugging counts of loading, looping and aborted images, needed for
   group observer notification. */
/* #define DEBUG_GROUP_OBSERVER */

#ifdef DEBUG
#define Debug 1
#endif

#ifdef XP_WIN
#define _USD 1              /* scanlines upside-down */ 
#endif

//extern PRLogModuleInfo *il_log_module;

#ifdef DEBUG
//#define ILTRACE(l,t) { if(il_debug>l) {PR_LOG(il_log_module, 1, t);} }
#else
#define ILTRACE(l,t) {}
#endif

#define FREE_IF_NOT_NULL(x)    do {if (x) {PR_FREEIF(x); (x) = NULL;}} while (0)

#include "prtypes.h"  /* for IS_LITTLE_ENDIAN / IS_BIG_ENDIAN */

/* The imagelib labels bits in a 32-bit word from 31 on the left to 0 on the right.
   This macro performs the necessary conversion to make that definition work on
   little-endian platforms */
#if defined(IS_LITTLE_ENDIAN)
#    define M32(bit)  ((bit) ^ 0x18)
#elif defined(IS_BIG_ENDIAN)
#    define M32(bit)  (bit)
#else
     ENDIANNESS UNKNOWN!
#endif

/* Don't change these unless you know what you're doing or you will
   break 16-bit binaries. */
#define MAX_IMAGE_WIDTH  8000
#define MAX_IMAGE_HEIGHT 8000

/* MAX and MIN are almost universal, but be paranoid and use our
   definitions. */
#ifdef MAX
#undef MAX
#endif
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifdef MIN
#undef MIN
#endif
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Last output pass of an image */
#define IL_FINAL_PASS     -1

/* Nature of the conversion from source image colorspace to target image
   colorspace. */
typedef enum _IL_ConversionType {
    IL_TrueToTrue     = 0x09,
    IL_TrueToPseudo   = 0x0a,
    IL_TrueToGrey     = 0x0c,
    IL_PseudoToTrue   = 0x11,
    IL_PseudoToPseudo = 0x12,
    IL_PseudoToGrey   = 0x14,
    IL_GreyToTrue     = 0x21,
    IL_GreyToPseudo   = 0x22,
    IL_GreyToGrey     = 0x24
} IL_ConversionType;

typedef void (*il_converter)(il_container *ic, const uint8 *mask, 
                             const uint8 *sp, int x_offset,
                             int num, void XP_HUGE *out);

enum icstate {
    IC_VIRGIN       = 0x00, /* Newly-created container */
    IC_START        = 0x01, /* Requested stream from netlib, but no data yet */
    IC_STREAM       = 0x02, /* Stream opened, but insufficient data
                               received to determine image size  */ 
    IC_SIZED        = 0x04, /* Image size determined - still loading */
    IC_MULTI        = 0x06, /* Same as IC_SIZED, but for second or
                               subsequent images in multipart MIME */
    IC_NOCACHE      = 0x11, /* Image deferred for loading later */
    IC_COMPLETE     = 0x20, /* Image loaded - no errors */
    IC_BAD          = 0x21, /* Corrupt or illegal image data */
    IC_INCOMPLETE   = 0x22, /* Partially loaded image data */
    IC_MISSING      = 0x23, /* No such file on server */
    IC_ABORT_PENDING= 0x24  /* Image download abort in progress */
};

/* Still receiving data from the netlib ? */
#define IMAGE_CONTAINER_ACTIVE(ic)  ((ic)->state <= IC_MULTI)

/* Force memory cache to be flushed ? */
#define FORCE_RELOAD(reload_method)                                           \
 (((reload_method)==NET_NORMAL_RELOAD) || ((reload_method)==NET_SUPER_RELOAD))

/* Simple list of image contexts. */
struct il_context_list {
    IL_GroupContext *img_cx;
    struct il_context_list *next;
};

/* Simple list of image containers. */
struct il_container_list {
    il_container *ic;
    struct il_container_list *next;
};


/* There is one il_container per real image */
struct il_container_struct {
    il_container *next;         /* Cache bidirectional linked list */
    il_container *prev;

    ilIURL *url;
    char *url_address;          /* Same as url->address if there is no redirection*/

    uint32 hash;
    uint32 urlhash;
    
    enum icstate state;
    int sized;

    int is_alone;               /* only image on a page */
    int is_in_use;              /* Used by some context */
    int32 loop_count;           /* Remaining number of times to repeat image,
                                   -1 means loop infinitely */
    int is_looping;             /* TRUE if this is the second or subsequent
                                   pass of an animation. */
    int is_aborted;             /* True if aborted by NetLib. */
	PRPackedBool is_url_loading;/* TRUE if NetLib is currently loading the URL. */
    int is_multipart;           /* TRUE if stream is known to be multipart. */
    int multi;                  /* Known to be either multipart-MIME 
                                   or multi-image format */
    int new_data_for_fe;        /* Any Scanlines that FE doesn't know about ? */
    int update_start_row;       /* Scanline range to send to FE */
    int update_end_row;

    uint32 bytes_consumed;      /* Bytes read from the stream so far */

    NI_PixmapHeader *src_header; /* Source image header information. */
    IL_Pixmap *image;           /* Destination image pixmap structure. */
    IL_Pixmap *mask;            /* Destination mask pixmap structure. */

    intn type;
    void *ds;                   /* decoder's private data */

    il_converter converter;
    void *quantize;             /* quantizer's private data */

    class nsIImgDecoder *imgdec;
    class nsIImgDCallbk *imgdcb;

    void *row_output_timeout;
    uint8 *scalerow;
    int pass;                   /* pass (scan #) of a multi-pass image.
                                   Used for interlaced GIFs & p-JPEGs */

    int forced;
    uint32 content_length;

    int dest_width, dest_height; /* Target dimensions of the image */
    PRPackedBool natural_size;  /* True if the image is decoded to its natural
                                   size. */
    PRPackedBool aspect_distorted; /* True if the image undergoes aspect ratio
                                      distortion during decoding. */

    IL_IRGB *background_color;  /* The requested background color for this
                                   image (only applies if the image is
                                   determined to be transparent.)  A mask will
                                   be created for a transparent image only if
                                   no background color was requested. */
    
    char *comment;              /* Human-readable text stored in image */
    int comment_length;

    int colormap_serial_num;    /* serial number of last installed colormap */

    int dont_use_custom_palette;
    int rendered_with_custom_palette;
    IL_DitherMode dither_mode;  /* ilDither or ilClosestColor */

    IL_GroupContext *img_cx;    /* The context in which this image was created.
                                   Used during image decoding only. */
    IL_DisplayType display_type; /* Type of display for which the container
                                    is created. */
    ilIImageRenderer *img_cb;
    ilINetContext *net_cx;      /* Context which initiated this transfer. */

    IL_ImageReq *clients;       /* List of clients of this container. */
    IL_ImageReq *lclient;       /* Last client in the client list. */
    il_context_list *img_cx_list; /* List of contexts which have clients of
                                     this container. */

    IL_Rect displayable_rect;   /* The area of the image pixmap which is in a
                                   displayable state.  Used as a filter
                                   between client calls to IL_DisplaySubImage
                                   and Image Library calls to DisplayPixmap, in
                                   the event that the client requests us to
                                   draw a part of the pixmap that has yet to
                                   be decoded. */

    time_t expires;             /* Expiration date for the corresponding URL */

#ifdef DEBUG
    PRTime start_time;
#endif
    char *fetch_url;            /* actual url address used */
};


typedef enum { ilUndefined, ilCI, ilGrey, ilRGB } il_mode;

typedef enum il_draw_mode 
{
    ilErase,                    /* Transparent areas are reset to background */
    ilOverlay                   /* Transparent areas overlay existing data */
} il_draw_mode;
    

/* A context for a group of images. */
struct _IL_GroupContext {
    ilIImageRenderer *img_cb;
    void *dpy_cx;               /* An opaque pointer passed back to all
                                   callbacks in the interface vtable. */

    IL_DisplayType display_type; /* IL_Console, IL_Printer or IL_PostScript. */
    IL_ColorSpace *color_space;  /* Display colorspace. */

    /* Preferences */
    PRPackedBool progressive_display; /* If TRUE, images are displayed while
                                         loading */
    IL_DitherMode dither_mode;  /* IL_ClosestColor, IL_Dither or IL_Auto. */
    int dontscale;              /* Used for Macs, which do their own scaling */
    int nolowsrc;               /* If TRUE, never display LOSRC images */

    /* Per-context state */
    il_container_list *container_list;/* List of containers in this context. */
    int32 num_containers;       /* Number of containers in this context. */
    int32 num_loading;          /* Number of containers which are currently
                                   loading. */
    int32 num_looping;          /* Number of containers which are currently
                                   looping i.e. on second or subsequent
                                   iteration of an animation. */
    int32 num_aborted;          /* Number of containers which have aborted
                                   so far. */
    
    XP_ObserverList obs_list;   /* List of observers for this image group. */

    struct _IL_GroupContext *next; /* Next entry in a list of image group
                                      contexts. */
};


/* Tag to indicate whether a request represents an image or an icon. */
typedef enum 
{
    IL_IMAGE,
    IL_ICON
} IL_ImageType;


/* This is Image Library's internal representation of a client's image request.
   It represents a handle on a specific instance of an image container.  */
struct _IL_ImageReq {
    il_container *ic;           /* The image container for this request (may
                                   be shared with other requests.) */
    IL_ImageType image_type;    /* Image or icon. */

    IL_GroupContext *img_cx;    /* The group context to which this request
                                   belongs. */
    ilINetContext *net_cx;      /* A clone of the net context which the image
                                   library was given when this image handle was
                                   created.  This serves as a backup in case
                                   the image container's net_cx becomes invalid,
                                   (for example, when the client for which the
                                   container was initially created is destroyed.) */							   
    PRPackedBool stopped;       /* TRUE - if user hit "Stop" button */
    int is_view_image;          /* non-zero if client is
                                   internal-external-reconnect */

    XP_ObserverList obs_list;   /* List of observers for this request. */

    struct _IL_ImageReq *next;  /* Next entry in a list of image requests. */
};

extern int il_debug;
#ifdef DEBUG
#define ILTRACE(l,t) {}
#else
#define ILTRACE(l,t) {}
#endif

#endif
#endif
