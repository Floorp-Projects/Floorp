/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
#ifndef _GIF_H_
#define _GIF_H_

/* gif2.h  
   The interface for the GIF87/89a decoder. 
*/
// List of possible parsing states
typedef enum {
    gif_gather,
    gif_init,                   //1
    gif_type,
    gif_version,
    gif_global_header,
    gif_global_colormap,
    gif_image_start,            //6
    gif_image_header,
    gif_image_colormap,
    gif_image_body,
    gif_lzw_start,
    gif_lzw,                    //11
    gif_sub_block,
    gif_extension,
    gif_control_extension,
    gif_consume_block,
    gif_skip_block,
    gif_done,                   //17
    gif_oom,
    gif_error,
    gif_comment_extension,
    gif_application_extension,
    gif_netscape_extension_block,
    gif_consume_netscape_extension,
    gif_consume_comment,
    gif_delay,
    gif_wait_for_buffer_full,
    gif_stop_animating   //added for animation stop 
} gstate;

/* "Disposal" method indicates how the image should be handled in the
   framebuffer before the subsequent image is displayed. */
typedef enum 
{
    DISPOSE_NOT_SPECIFIED      = 0,
    DISPOSE_KEEP               = 1, /* Leave it in the framebuffer */
    DISPOSE_OVERWRITE_BGCOLOR  = 2, /* Overwrite with background color */
    DISPOSE_OVERWRITE_PREVIOUS = 4  /* Save-under */
} gdispose;

/* A RGB triplet representing a single pixel in the image's colormap
   (if present.) */
typedef struct _GIF_RGB
{
  PRUint8 red, green, blue, pad; /* Windows requires the fourth byte &
                                    many compilers pad it anyway. */
                    
  /* XXX: hist_count appears to be unused */                                  
  //PRUint16 hist_count;           /* Histogram frequency count. */
} GIF_RGB;
    
/* Colormap information. */
typedef struct _GIF_ColorMap {
    int32 num_colors;           /* Number of colors in the colormap.
                                   A negative value can be used to denote a
                                   possibly non-unique set. */
    GIF_RGB *map;                /* Colormap colors. */
    PRUint8 *index;             /* NULL, if map is in index order.  Otherwise
                                   specifies the indices of the map entries. */
    void *table;                /* Lookup table for this colormap.  Private to
                                   the Image Library. */
} GIF_ColorMap;
    
/* An indexed RGB triplet. */
typedef struct _GIF_IRGB {
    PRUint8 index;
    PRUint8 red, green, blue;
} GIF_IRGB;

/* A GIF decoder's state */
typedef struct gif_struct {
    void* clientptr;
    /* Callbacks for this decoder instance*/
    int (PR_CALLBACK *GIFCallback_NewPixmap)();
    int (PR_CALLBACK *GIFCallback_BeginGIF)(
      void*    aClientData,
      PRUint32 aLogicalScreenWidth, 
      PRUint32 aLogicalScreenHeight,
      PRUint8  aLogicalScreenBackgroundRGBIndex);
      
    int (PR_CALLBACK* GIFCallback_EndGIF)(
      void*    aClientData,
      int      aAnimationLoopCount);
      
    int (PR_CALLBACK* GIFCallback_BeginImageFrame)(
      void*    aClientData,
      PRUint32 aFrameNumber,   /* Frame number, 1-n */
      PRUint32 aFrameXOffset,  /* X offset in logical screen */
      PRUint32 aFrameYOffset,  /* Y offset in logical screen */
      PRUint32 aFrameWidth,    
      PRUint32 aFrameHeight,   
      GIF_RGB* aTransparencyChromaKey);
    int (PR_CALLBACK* GIFCallback_EndImageFrame)(
      void* aClientData,
      PRUint32 aFrameNumber,
      PRUint32 aDelayTimeout,
      PRUint32 aDisposal);
    int (PR_CALLBACK* GIFCallback_SetupColorspaceConverter)();
    int (PR_CALLBACK* GIFCallback_ResetPalette)(); 
    int (PR_CALLBACK* GIFCallback_InitTransparentPixel)();
    int (PR_CALLBACK* GIFCallback_DestroyTransparentPixel)();
    int (PR_CALLBACK* GIFCallback_HaveDecodedRow)(
      void* aClientData,
      PRUint8* aRowBufPtr,   /* Pointer to single scanline temporary buffer */
      int aXOffset,          /* With respect to GIF logical screen origin */
      int aLength,           /* Length of the row? */
      int aRow,              /* Row number? */
      int aDuplicateCount,   /* Number of times to duplicate the row? */
      PRUint8 aDrawMode,     /* il_draw_mode */
      int aInterlacePass);
    int (PR_CALLBACK *GIFCallback_HaveImageAll)(
      void* aClientData);
        
    /* Parsing state machine */
    gstate state;               /* Curent decoder master state */
    PRUint8 *hold;                /* Accumulation buffer */
    int32 hold_size;            /* Capacity, in bytes, of accumulation buffer */
    PRUint8 *gather_head;         /* Next byte to read in accumulation buffer */
    int32 gather_request_size;  /* Number of bytes to accumulate */
    int32 gathered;             /* bytes accumulated so far*/
    gstate post_gather_state;   /* State after requested bytes accumulated */
    int32 requested_buffer_fullness; /* For netscape application extension */

    /* LZW decoder state machine */
    PRUint8 *stack;               /* Base of decoder stack */
    PRUint8 *stackp;              /* Current stack pointer */
    PRUint16 *prefix;
    PRUint8 *suffix;
    int datasize;
    int codesize;
    int codemask;
    int clear_code;             /* Codeword used to trigger dictionary reset */
    int avail;                  /* Index of next available slot in dictionary */
    int oldcode;
    PRUint8 firstchar;
    int count;                  /* Remaining # bytes in sub-block */
    int bits;                   /* Number of unread bits in "datum" */
    int32 datum;                /* 32-bit input buffer */

    /* Output state machine */
    int ipass;                  /* Interlace pass; Ranges 1-4 if interlaced. */
    PRUintn rows_remaining;        /* Rows remaining to be output */
    PRUintn irow;                  /* Current output row, starting at zero */
    PRUint8 *rowbuf;              /* Single scanline temporary buffer */
    PRUint8 *rowend;              /* Pointer to end of rowbuf */
    PRUint8 *rowp;                /* Current output pointer */

    /* Parameters for image frame currently being decoded*/
    PRUintn x_offset, y_offset;    /* With respect to "screen" origin */
    PRUintn height, width;
    PRUintn last_x_offset, last_y_offset; /* With respect to "screen" origin */
    PRUintn last_height, last_width;
    int interlaced;             /* TRUE, if scanlines arrive interlaced order */
    int tpixel;                 /* Index of transparent pixel */
    GIF_IRGB* transparent_pixel;
    int is_transparent;         /* TRUE, if tpixel is valid */
    int control_extension;      /* TRUE, if image control extension present */
    int is_local_colormap_defined;
    gdispose disposal_method;   /* Restore to background, leave in place, etc.*/
    gdispose last_disposal_method;
    GIF_RGB *local_colormap;     /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    PRUint32 delay_time;        /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */

    /* Global (multi-image) state */
    int screen_bgcolor;         /* Logical screen background color */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    PRUintn screen_width;       /* Logical screen width & height */
    PRUintn screen_height;
    GIF_RGB *global_colormap;    /* Default colormap if local not supplied  */
    int global_colormap_size;   /* Size of global colormap array. */
    int images_decoded;         /* Counts images for multi-part GIFs */
    int destroy_pending;        /* Stream has ended */
    int progressive_display;    /* If TRUE, do Haeberli interlace hack */
    int loop_count;             /* Netscape specific extension block to control
                                   the number of animation loops a GIF renders. */
} gif_struct;


/* Create a new gif_struct */
extern PRBool gif_create(gif_struct **gs);

/* These are the APIs that the client calls to intialize,
push data to, and shut down the GIF decoder. */
PRBool GIFInit(
  gif_struct* gs,

  void* aClientData,
  
  int (*PR_CALLBACK GIFCallback_NewPixmap)(),
  
  int (*PR_CALLBACK GIFCallback_BeginGIF)(
    void* aClientData,
    PRUint32 aLogicalScreenWidth, 
    PRUint32 aLogicalScreenHeight,
    PRUint8  aBackgroundRGBIndex),
    
  int (*PR_CALLBACK GIFCallback_EndGIF)(
    void*    aClientData,
    int      aAnimationLoopCount),
  
  int (*PR_CALLBACK GIFCallback_BeginImageFrame)(
    void*    aClientData,
    PRUint32 aFrameNumber,   /* Frame number, 1-n */
    PRUint32 aFrameXOffset,  /* X offset in logical screen */
    PRUint32 aFrameYOffset,  /* Y offset in logical screen */
    PRUint32 aFrameWidth,    
    PRUint32 aFrameHeight,   
    GIF_RGB* aTransparencyChromaKey),
  
  int (*PR_CALLBACK GIFCallback_EndImageFrame)(
    void* aClientData,
    PRUint32 aFrameNumber,
    PRUint32 aDelayTimeout,
    PRUint32 aDisposal),
  
  int (*PR_CALLBACK GIFCallback_SetupColorspaceConverter)(),
  
  int (*PR_CALLBACK GIFCallback_ResetPalette)(),
  
  int (*PR_CALLBACK GIFCallback_InitTransparentPixel)(),
  
  int (*PR_CALLBACK GIFCallback_DestroyTransparentPixel)(),
  
  int (*PR_CALLBACK GIFCallback_HaveDecodedRow)(
    void* aClientData,
    PRUint8* aRowBufPtr,   /* Pointer to single scanline temporary buffer */
    int aXOffset,          /* With respect to GIF logical screen origin */
    int aLength,           /* Length of the row? */
    int aRow,              /* Row number? */
    int aDuplicateCount,   /* Number of times to duplicate the row? */
    PRUint8 aDrawMode,     /* il_draw_mode */
    int aInterlacePass),
    
  int (*PR_CALLBACK GIFCallback_HaveImageAll)(
    void* aClientData)
  );

extern void gif_destroy(gif_struct* aGIFStruct);

int gif_write(gif_struct* aGIFStruct, const PRUint8 * buf, PRUint32 numbytes);

PRUint8 gif_write_ready(gif_struct* aGIFStruct);

extern void gif_complete(gif_struct** aGIFStruct);
extern void gif_delay_time_callback(/* void *closure */);


/* Callback functions that the client must implement and pass in 
pointers for during the GIFInit call. These will be called back
when the decoder has a decoded rows, frame size information, etc.*/

/* GIFCallback_LogicalScreenSize is called only once to notify the client
of the logical screen size, which will be the size of the total image. */
typedef int (*PR_CALLBACK BEGINGIF_CALLBACK)(
  void*    aClientData,
  PRUint32 aLogicalScreenWidth, 
  PRUint32 aLogicalScreenHeight,
  PRUint8  aLogicalScreenBackgroundRGBIndex);
                            
typedef int (PR_CALLBACK *GIFCallback_EndGIF)(
  void*    aClientData,
  int      aAnimationLoopCount);

/* GIFCallback_BeginImageFrame is called at the beginning of each frame of
a GIF.*/
typedef int (PR_CALLBACK *GIFCallback_BeginImageFrame)(
  void*    aClientData,
  PRUint32 aFrameNumber,   /* Frame number, 1-n */
  PRUint32 aFrameXOffset,  /* X offset in logical screen */
  PRUint32 aFraqeYOffset,  /* Y offset in logical screen */
  PRUint32 aFrameWidth,    
  PRUint32 aFrameHeight); 

extern int GIFCallback_EndImageFrame(
  void*    aClientData,
  PRUint32 aFrameNumber,
  PRUint32 aDelayTimeout,
  PRUint32 aDisposal); /* Time in milliseconds this frame should be displayed before the next frame.
                              This information appears in a sub control block, so we don't
                              transmit it back to the client until we're done with the frame. */

/*
extern int GIFCallback_SetupColorspaceConverter();
extern int GIFCallback_ResetPalette(); 
extern int GIFCallback_InitTransparentPixel();
extern int GIFCallback_DestroyTransparentPixel();
*/
extern int GIFCallback_HaveDecodedRow();
extern int GIFCallback_HaveImageAll();

#endif

