/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
    DISPOSE_OVERWRITE_PREVIOUS = 3  /* Save-under */
} gdispose;

/* A GIF decoder's state */
typedef struct gif_struct {
    void* clientptr;
    /* Parsing state machine */
    gstate state;               /* Curent decoder master state */
    PRUint8 *hold;                /* Accumulation buffer */
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
    int interlaced;             /* TRUE, if scanlines arrive interlaced order */
    int tpixel;                 /* Index of transparent pixel */
    int is_transparent;         /* TRUE, if tpixel is valid */
    int control_extension;      /* TRUE, if image control extension present */
    int is_local_colormap_defined;
    gdispose disposal_method;   /* Restore to background, leave in place, etc.*/
    PRUint8 *local_colormap;    /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    PRUint32 delay_time;        /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */

    /* Global (multi-image) state */
    int screen_bgcolor;         /* Logical screen background color */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    PRUintn screen_width;       /* Logical screen width & height */
    PRUintn screen_height;
    PRUint8 *global_colormap;   /* Default colormap if local not supplied  */
    int global_colormap_size;   /* Size of global colormap array. */
    int images_decoded;         /* Counts images for multi-part GIFs */
    int progressive_display;    /* If TRUE, do Haeberli interlace hack */
    int loop_count;             /* Netscape specific extension block to control
                                   the number of animation loops a GIF renders. */
} gif_struct;


/* These are the APIs that the client calls to intialize,
push data to, and shut down the GIF decoder. */
PRBool GIFInit(gif_struct* gs, void* aClientData);

void gif_destroy(gif_struct* aGIFStruct);

PRStatus gif_write(gif_struct* aGIFStruct, const PRUint8 * buf, PRUint32 numbytes);

PRBool gif_write_ready(const gif_struct* aGIFStruct);

#endif

