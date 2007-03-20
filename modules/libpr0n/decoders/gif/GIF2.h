/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef _GIF_H_
#define _GIF_H_

#define MAX_LZW_BITS          12
#define MAX_BITS            4097 /* 2^MAX_LZW_BITS+1 */
#define MINIMUM_DELAY_TIME   100
#define MAX_COLORS           256
#define MAX_HOLD_SIZE        256

/* gif2.h  
   The interface for the GIF87/89a decoder. 
*/
// List of possible parsing states
typedef enum {
    gif_type,
    gif_global_header,
    gif_global_colormap,
    gif_image_start,            
    gif_image_header,
    gif_image_colormap,
    gif_image_body,
    gif_lzw_start,
    gif_lzw,
    gif_sub_block,
    gif_extension,
    gif_control_extension,
    gif_consume_block,
    gif_skip_block,
    gif_done,
    gif_oom,
    gif_error,
    gif_comment_extension,
    gif_application_extension,
    gif_netscape_extension_block,
    gif_consume_netscape_extension,
    gif_consume_comment
} gstate;

/* A GIF decoder's state */
typedef struct gif_struct {
    void* clientptr;
    /* Parsing state machine */
    gstate state;                   /* Curent decoder master state */
    PRUint32 bytes_to_consume;      /* Number of bytes to accumulate */
    PRUint32 bytes_in_hold;         /* bytes accumulated so far*/

    /* LZW decoder state machine */
    PRUint8 *stackp;              /* Current stack pointer */
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
    int tpixel;                 /* Index of transparent pixel */
    PRInt32 disposal_method;    /* Restore to background, leave in place, etc.*/
    PRUint8 *local_colormap;    /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    PRUint32 delay_time;        /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */

    /* Global (multi-image) state */
    int screen_bgcolor;         /* Logical screen background color */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    PRUintn screen_width;       /* Logical screen width & height */
    PRUintn screen_height;
    int global_colormap_size;   /* Size of global colormap array. */
    int images_decoded;         /* Counts images for multi-part GIFs */
    int loop_count;             /* Netscape specific extension block to control
                                   the number of animation loops a GIF renders. */

    PRPackedBool progressive_display;    /* If TRUE, do Haeberli interlace hack */
    PRPackedBool interlaced;             /* TRUE, if scanlines arrive interlaced order */
    PRPackedBool is_transparent;         /* TRUE, if tpixel is valid */
    PRPackedBool is_local_colormap_defined;

    PRUint16  prefix[MAX_BITS];          /* LZW decoding tables */
    PRUint8   hold[MAX_HOLD_SIZE];       /* Accumulation buffer */
    PRUint8   global_colormap[3*MAX_COLORS];   /* Default colormap if local not supplied, 3 bytes for each color  */
    PRUint8   suffix[MAX_BITS];          /* LZW decoding tables */
    PRUint8   stack[MAX_BITS];           /* Base of LZW decoder stack */

} gif_struct;


/* These are the APIs that the client calls to intialize,
push data to, and shut down the GIF decoder. */
PRBool GIFInit(gif_struct* gs, void* aClientData);

void gif_destroy(gif_struct* aGIFStruct);

PRStatus gif_write(gif_struct* aGIFStruct, const PRUint8 * buf, PRUint32 numbytes);

PRBool gif_write_ready(const gif_struct* aGIFStruct);

#endif

