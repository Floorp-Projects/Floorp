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

/*   if.h --- Top-level image library internal routines
 *
 * $Id: if.h,v 3.22 2000/08/16 01:19:44 pnunn%netscape.com Exp $
 */

#ifndef _if_h
#define _if_h
/*************************************************/
#ifndef _ifstruct_h
#define M12N

#define IL_INTERNAL

#include "prtypes.h"
#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "prtime.h"
#include "prlong.h"
#include "nsIPresContext.h"

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


#include "il.h"


#if defined(XP_WIN) || defined(XP_OS2)
#define _USD 1              /* scanlines upside-down */ 
#endif



/* For debugging counts of loading, looping and aborted images, needed for
   group observer notification. */
/* #define DEBUG_GROUP_OBSERVER */



#ifdef DEBUG
extern int il_debug;
extern PRLogModuleInfo *il_log_module;
#define ILTRACE(l,t) { if(il_debug>l) {PR_LOG(il_log_module, 1, t);} }
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

/* types that can be used to, for example, differentiate chrome
   images from others */

#define TYPE_DEFAULT 0		// unspecified
#define TYPE_CHROME 1		// chrome URL

/* Conversion of imglib errors to XPCOM errors */
#define NS_CONVERT_ERROR_CODE(e)  \
      (NS_ERROR_GENERATE((e) ? NS_ERROR_SEVERITY_ERROR : NS_ERROR_SEVERITY_SUCCESS, NS_ERROR_MODULE_IMGLIB, (e) & 0xFFFF))


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

typedef void (*il_converter)(il_container *ic, const PRUint8 *mask, 
                             const PRUint8 *sp, int x_offset,
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
    (reload_method = DONT_USE_IMG_CACHE)

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

    PRUint32 hash;
    PRUint32 urlhash;
    
    enum icstate state;
    int sized;

    int moz_type;		/* TYPE_CHROME, etc. */
    int is_alone;               /* only image on a page */
    int is_in_use;              /* Used by some context */
    int32 loop_count;           /* Remaining number of times to repeat image,
                                   -1 means loop infinitely */
    int is_looping;             /* TRUE if this is the second or subsequent
                                   pass of an animation. */
    nsImageAnimation animate_request;        /* Set in nsPresContext.
                                    normal = 0; one frame = 1; one loop = 2 */
    int is_aborted;             /* True if aborted by NetLib. */
	PRPackedBool is_url_loading;/* TRUE if NetLib is currently loading the URL. */
    int is_multipart;           /* TRUE if stream is known to be multipart. */
    int multi;                  /* Known to be either multipart-MIME 
                                   or multi-image format */
    int new_data_for_fe;        /* Any Scanlines that FE doesn't know about ? */
    int update_start_row;       /* Scanline range to send to FE */
    int update_end_row;

    PRUint32 bytes_consumed;      /* Bytes read from the stream so far */

    NI_PixmapHeader *src_header; /* Source image header information. */
    IL_Pixmap *image;           /* Destination image pixmap structure. */
    IL_Pixmap *mask;            /* Destination mask pixmap structure. */

    char* type;                 /* mimetype string ptn 10.13.99*/
    void *ds;                   /* decoder's private data */

    il_converter converter;
    void *quantize;             /* quantizer's private data */

    class nsIImgDecoder *imgdec;
    class ImgDCallbk *imgdcb;

    void *row_output_timeout;
    PRUint8 *scalerow;
    int pass;                   /* pass (scan #) of a multi-pass image.
                                   Used for interlaced GIFs & p-JPEGs */

    int forced;
    PRUint32 content_length;

    PRUint32 dest_width, dest_height; /* Target dimensions of the image */
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

/********************** end of ifstruct_h test *********************************************************/
#endif

extern int il_debug;
extern PRUint8 il_identity_index_map[];

extern void il_delete_container(il_container *ic);
extern il_container *il_removefromcache(il_container *ic);
extern void il_image_abort(il_container *ic);
extern void il_image_complete(il_container *ic);
extern PRBool il_image_stopped(il_container *ic);

extern ilINetReader *IL_NewNetReader(il_container *ic);
extern il_container *IL_GetNetReaderContainer(ilINetReader *reader);


extern int IL_StreamWriteReady(il_container *ic);
extern int IL_StreamFirstWrite(il_container *ic, const unsigned char *str, int32 len);
extern int IL_StreamWrite(il_container *ic, const unsigned char *str, int32 len);
extern void IL_StreamAbort(il_container *ic, int status);
extern void IL_StreamComplete(il_container *ic, PRBool is_multipart);
extern void IL_NetRequestDone(il_container *ic, ilIURL *urls, int status);
extern PRBool IL_StreamCreated(il_container *ic, ilIURL *urls, char* type);

/* Allocate and initialize the destination image's transparent_pixel with
   the Image Library's preferred transparency color i.e. the background color
   passed into IL_GetImage.  The image decoder is encouraged to use this
   color, but may opt not to do so. */
extern int il_init_image_transparent_pixel(il_container *ic);

/* Destroy the destination image's transparent pixel. */
extern void il_destroy_image_transparent_pixel(il_container *ic);

/* Inform the Image Library of the source image's dimensions.  This function
   determines the size of the destination image, and calls the front end to
   allocate storage for the destination image's bits.  If the source image's
   transparent pixel is set, and a background color was not specified for this
   image request, then a mask will also be allocated for the destination
   image. */
extern int  il_size(il_container *);
extern void  il_dimensions_notify(il_container *ic, int dest_width, int dest_height);

extern int  il_setup_quantize(void);
extern int  il_init_quantize(il_container *ic);
extern void il_free_quantize(il_container *ic);
extern void il_quantize_fs_dither(il_container * ic,
                                  const PRUint8* mask,
                                  const PRUint8 *samp_in,
                                  int x_offset,
                                  PRUint8 XP_HUGE *samp_out,
                                  int width);

extern PRBool il_emit_row(il_container *ic, PRUint8 *buf, PRUint8 *rgbbuf,
                        int start_column, int len, int row, int row_count,
                        il_draw_mode draw_mode, int ipass);

extern void il_flush_image_data(il_container *ic);
extern PRBool il_setup_color_space_converter(il_container *ic);

extern void il_convert_image_to_default_colormap(il_container *ic);

#ifndef M12N                    /* XXXM12N Fix custom colormaps. */
extern int  il_set_color_palette(MWContext *cx, il_container *ic);
#endif /* M12N */

extern PRBool il_reset_palette(il_container *ic);

extern void il_reverse_bits(PRUint8 *buf, int n);
extern void il_reconnect(il_container *cx);
extern void il_abort_reconnect(void);

/* Get an image container from the image cache.  If no cache entry exists,
   then create and return a new container. */
extern il_container
*il_get_container(IL_GroupContext *image_context,
                  ImgCachePolicy reload_cache_policy,
                  const char *image_url,
                  IL_IRGB *background_color,
                  IL_DitherMode dither_mode,
                  int req_depth,
                  int req_width,
                  int req_height);

/* Destroy an IL_Pixmap. */
extern void
il_destroy_pixmap(ilIImageRenderer *img_cb, IL_Pixmap *pixmap);

extern PRUint32 il_hash(const char *ubuf);

extern void il_partial(il_container *ic, int row, int row_count, int pass);
extern void il_scour_container(il_container *ic);
extern void il_adjust_cache_fullness(int32 bytes);
extern PRBool il_add_client(IL_GroupContext *img_cx, il_container *ic,
                            IL_ImageReq *image_req, int is_view_image);
extern PRBool il_delete_client(il_container *ic, IL_ImageReq *image_req);

extern void il_reduce_image_cache_size_to(PRUint32 new_size);


/************************ Image observer notifiers ***************************/

/* Notify observers of image progress. */
extern void
il_progress_notify(il_container *ic);

/* Notify observers that the image pixmap has been updated. */
extern void
il_pixmap_update_notify(il_container *ic);

/* Notify observers that the image has finished decoding. */
extern void
il_image_complete_notify(il_container *ic);

/* Notify observers that one frame of image/animation has finished decoding. */
extern void
il_frame_complete_notify(il_container *ic);

/* Notify observers of a cached image that has already completed decoding.
   Note that this notification is per image request and not per image
   container. */
extern void
il_cache_return_notify(IL_ImageReq *image_req);

/* Notify observers that the image request is being destroyed.  This
   provides an opportunity for observers to remove themselves from the
   observer callback list and to do any related cleanup. */
extern void
il_image_destroyed_notify(IL_ImageReq *image_req);


/********************* Image group observer notifier *************************/

/* Notify observers that images have started/stopped loading in the context,
   or started/stopped looping in the context. */
extern void
il_group_notify(IL_GroupContext *img_cx, XP_ObservableMsg message);


#endif /* _if_h */

