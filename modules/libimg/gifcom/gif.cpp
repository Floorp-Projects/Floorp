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
 * The Original Code is mozilla.org code.
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

/* -*- Mode: C; tab-width: 4 -*-
 *   gif.c --- GIF87 & GIF89 Image decoder
 */



/* 
The Graphics Interchange Format(c) is the copyright property of CompuServe
Incorporated. Only CompuServe Incorporated is authorized to define, redefine,
enhance, alter, modify or change in any way the definition of the format.

CompuServe Incorporated hereby grants a limited, non-exclusive, royalty-free
license for the use of the Graphics Interchange Format(sm) in computer
software; computer software utilizing GIF(sm) must acknowledge ownership of the
Graphics Interchange Format and its Service Mark by CompuServe Incorporated, in
User and Technical Documentation. Computer software utilizing GIF, which is
distributed or may be distributed without User or Technical Documentation must
display to the screen or printer a message acknowledging ownership of the
Graphics Interchange Format and the Service Mark by CompuServe Incorporated; in
this case, the acknowledgement may be displayed in an opening screen or leading
banner, or a closing screen or trailing banner. A message such as the following
may be used:

      "The Graphics Interchange Format(c) is the Copyright property of
      CompuServe Incorporated. GIF(sm) is a Service Mark property of
      CompuServe Incorporated."

For further information, please contact :

      CompuServe Incorporated
      Graphics Technology Department
      5000 Arlington Center Boulevard
      Columbus, Ohio  43220
      U. S. A.

CompuServe Incorporated maintains a mailing list with all those individuals and
organizations who wish to receive copies of this document when it is corrected
or revised. This service is offered free of charge; please provide us with your
mailing address.

*/

#include "nsIImgDecoder.h"
#include "il.h"
#include "prmem.h"

#include "ilErrors.h"
#include "nsCRT.h"

#include "gif.h"
#include "nsIImgDCallbk.h"

#include "ilISystemServices.h"

#define HOWMANY(x, r)     (((x) + ((r) - 1)) / (r))
#define ROUNDUP(x, r)     (HOWMANY(x, r) * (r))

#ifdef DEBUG
static int il_debug_gif = 0;
static PRLogModuleInfo *il_log_module_gif = NULL;
#define ILTRACE(l,t) { if(il_debug_gif>l) {PR_LOG(il_log_module_gif, 1, t);} } 
#else
#define ILTRACE(l,t) {}
#endif

#ifndef MAX
#    define MAX(x, y)   (((x) > (y)) ? (x) : (y))
#endif

/* List of possible parsing states */
typedef enum {
    gif_gather,
    gif_init,                   /*1*/
    gif_type,
    gif_version,
    gif_global_header,
    gif_global_colormap,
    gif_image_start,            /*6*/
    gif_image_header,
    gif_image_colormap,
    gif_image_body,
    gif_lzw_start,
    gif_lzw,                    /*11*/
    gif_sub_block,
    gif_extension,
    gif_control_extension,
    gif_consume_block,
    gif_skip_block,
    gif_done,                   /*17*/
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

#define MAX_HOLD 768        /* for now must be big enough for a cmap */

#define MAX_LZW_BITS          12
#define MAX_BITS            4097 /* 2^MAX_LZW_BITS+1 */
#define MINIMUM_DELAY_TIME    10


/* A GIF decoder */

typedef struct
gif_struct {
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
    PRUint8 *rgbrow;              /* Temporary storage for dithering/mapping */
    PRUint8 *rowbuf;              /* Single scanline temporary buffer */
    PRUint8 *rowend;              /* Pointer to end of rowbuf */
    PRUint8 *rowp;                /* Current output pointer */

    /* Image parameters */
    PRUintn x_offset, y_offset;    /* With respect to "screen" origin */
    PRUintn height, width;
    PRUintn last_x_offset, last_y_offset; /* With respect to "screen" origin */
    PRUintn last_height, last_width;
    int interlaced;             /* TRUE, if scanlines arrive interlaced order */
    int tpixel;                 /* Index of transparent pixel */
    int is_transparent;         /* TRUE, if tpixel is valid */
    int control_extension;      /* TRUE, if image control extension present */
    int is_local_colormap_defined;
    gdispose disposal_method;   /* Restore to background, leave in place, etc.*/
    gdispose last_disposal_method;
    IL_RGB *local_colormap;     /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    int progressive_display;    /* If TRUE, do Haeberli interlace hack */
    PRUint32 delay_time;          /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */

    /* Global (multi-image) state */
    int screen_bgcolor;         /* Logical screen background color */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    PRUintn screen_width;          /* Logical screen width & height */
    PRUintn screen_height;
    IL_RGB *global_colormap;    /* Default colormap if local not supplied  */
    int global_colormap_size;   /* Size of global colormap array. */
    int images_decoded;         /* Counts images for multi-part GIFs */
    il_container *ic;           /* Back pointer to requesting image_container */
    void *delay_timeout;        /* Used to delay after displaying picture */
    int destroy_pending;        /* Stream has ended */
} gif_struct;

/* Gather n characters from the input stream and then enter state s. */
#define GETN(n,s)                                                             \
do {gs->state=gif_gather; gs->gather_request_size = (n);                      \
    gs->post_gather_state = s;} while (0)

/* Get a 16-bit value stored in little-endian format */
#define GETINT16(p)   ((p)[1]<<8|(p)[0])

/* Get a 32-bit value stored in little-endian format */
#define GETINT32(p)   (((p)[3]<<24) | ((p)[2]<<16) | ((p)[1]<<8) | ((p)[0]))

/*  binary block Allocate and Concatenate
 *
 *   destination_length  is the length of the existing block
 *   source_length   is the length of the block being added to the 
 *   destination block
 */
static char * 
il_BACat (char **destination, 
           size_t destination_length, 
           const char *source, 
           size_t source_length)
{
    if (source) 
      {
        if (*destination) 
          {
            *destination = (char *) PR_REALLOC (*destination, destination_length + source_length);
            if (*destination == NULL) 
              return(NULL);

            nsCRT::memmove(*destination + destination_length, source, source_length);

          } 
        else 
          {
            *destination = (char *) PR_MALLOC (source_length);
            if (*destination == NULL) 
              return(NULL);

            nsCRT::memcpy(*destination, source, source_length);
          }
    }

  return *destination;
}
#undef BlockAllocCat
#define BlockAllocCat(dest, dest_length, src, src_length)  il_BACat(&(dest), dest_length, src, src_length)

/* Send the data to the display front-end. */
static void
output_row(gif_struct *gs)
{
    int width, drow_start, drow_end;
    
    drow_start = drow_end = gs->irow;

    /*
     * Haeberli-inspired hack for interlaced GIFs: Replicate lines while
     * displaying to diminish the "venetian-blind" effect as the image is
     * loaded. Adjust pixel vertical positions to avoid the appearance of the
     * image crawling up the screen as successive passes are drawn.
     */
    if (gs->progressive_display && gs->interlaced && (gs->ipass < 4))
    {
        PRUintn row_dup=0, row_shift=0;

        switch (gs->ipass) {
        case 1:
            row_dup = 7;
            row_shift = 3;
            break;
        case 2:
            row_dup = 3;
            row_shift = 1;
            break;
        case 3:            
            row_dup = 1;
            row_shift = 0;
            break;
        default:
            ILTRACE(0,("Illegal interlace pass"));
            break;
        }

        drow_start -= row_shift;
        drow_end = drow_start + row_dup;

        /* Extend if bottom edge isn't covered because of the shift upward. */
        if (((gs->height - 1) - drow_end) <= row_shift)
            drow_end = gs->height - 1;

        /* Clamp first and last rows to upper and lower edge of image. */
        if (drow_start < 0)
            drow_start = 0; 
        if ((PRUintn)drow_end >= gs->height)
            drow_end = gs->height - 1;
    }

    /* Check for scanline below edge of logical screen */
    if ((gs->y_offset + gs->irow) < gs->screen_height) {
        il_draw_mode draw_mode;

        if (gs->images_decoded >= 1)
            draw_mode = ilOverlay;
        else
            draw_mode = ilErase;

        /* Clip if right edge of image exceeds limits */
        if ((gs->x_offset + gs->width) > gs->screen_width)
            width = gs->screen_width - gs->x_offset;
        else
            width = gs->width;

        if (width > 0)
          if(gs->ic->imgdcb)
            gs->ic->imgdcb->ImgDCBHaveRow(gs->rowbuf, gs->rgbrow,
                    gs->x_offset, width, gs->y_offset + drow_start,
                    drow_end - drow_start + 1, draw_mode, gs->ipass);
           
    }

    gs->rowp = gs->rowbuf;

    if(!gs->interlaced)
    {
        gs->irow++;
    }
    else
    {
        do{
            switch(gs->ipass)
            {
                case 1:
                    gs->irow += 8;
                    if(gs->irow >= gs->height)
                    {
                        gs->ipass++;
                        gs->irow = 4;
                    }
                    break;

                case 2:
                    gs->irow += 8;
                    if(gs->irow >= gs->height)
                    {
                        gs->ipass++;
                        gs->irow = 2;
                    }
                    break;

                case 3:
                    gs->irow += 4;
                    if(gs->irow >= gs->height)
                    {
                        gs->ipass++;
                        gs->irow = 1;
                    }
                    break;

                case 4:
                    gs->irow += 2;
                    if(gs->irow >= gs->height){
                        gs->ipass++;
                        gs->irow = 0;
                    }
                    break;

                default:
                    PR_ASSERT(0);
            }
        }while(gs->irow > gs->height - 1);
    }
}

/* Perform Lempel-Ziv-Welch decoding */
static int 
do_lzw(gif_struct *gs, const PRUint8 *q)
{
    int code;
    int incode;
    const PRUint8 *ch;

    /* Copy all the decoder state variables into locals so the compiler
     * won't worry about them being aliased.  The locals will be homed
     * back into the GIF decoder structure when we exit.
     */
    int avail       = gs->avail;
    int bits        = gs->bits;
    int codesize    = gs->codesize;
    int codemask    = gs->codemask;
    int count       = gs->count;
    int oldcode     = gs->oldcode;
    int clear_code  = gs->clear_code;
    PRUint8 firstchar = gs->firstchar;
    int32 datum     = gs->datum;
    PRUint16 *prefix  = gs->prefix;
    PRUint8 *stackp   = gs->stackp;
    PRUint8 *suffix   = gs->suffix;
    PRUint8 *stack    = gs->stack;
    PRUint8 *rowp     = gs->rowp;
    PRUint8 *rowend   = gs->rowend;
    PRUintn rows_remaining = gs->rows_remaining;


#define OUTPUT_ROW(gs)                                                        \
    {                                                                         \
        output_row(gs);                                                       \
        rows_remaining--;                                                     \
        rowp = gs->rowp;                                                      \
        if (!rows_remaining)                                                  \
            goto END;                                                         \
    }

    for (ch=q; count-- > 0; ch++) 
    {
        /* Feed the next byte into the decoder's 32-bit input buffer. */
        datum += ((int32) *ch) << bits;
        bits += 8;
        
        /* Check for underflow of decoder's 32-bit input buffer. */
        while (bits >= codesize) 
        {
            /* Get the leading variable-length symbol from the data stream */
            code = datum & codemask;
            datum >>= codesize;
            bits -= codesize;

            /* Reset the dictionary to its original state, if requested */
            if (code == clear_code) 
            {
                codesize = gs->datasize + 1;
                codemask = (1 << codesize) - 1;
                avail = clear_code + 2;
                oldcode = -1;
                continue;
            }

            /* Check for explicit end-of-stream code */
            if (code == (clear_code + 1))
                   return 0;

            if (oldcode == -1) 
            {
                *rowp++ = suffix[code];
                if (rowp == rowend) {
                    OUTPUT_ROW(gs);
        }
                
                firstchar = oldcode = code;
                continue;
            }

            /* Check for a code not defined in the dictionary yet. */
            if (code > avail) 
            {
                ILTRACE(3,("il:gif: code too large %d %d", code, avail));
                return -1;
            }

            incode = code;
            if (code == avail) 
            {      
                /* the first code is always < avail */
                *stackp++ = firstchar;
                code = oldcode;
            }

            int code2=0;
            while(code > clear_code)
            {     
                code2 = code;
                if(code == prefix[code])
                    return -1;

                *stackp++ = suffix[code];
                code = prefix[code];
                
                if(code2 == prefix[code])
                    return -1;
            }

            /* Define a new codeword in the dictionary. */
            *stackp++ = firstchar = suffix[code];
            prefix[avail] = oldcode;
            suffix[avail] = firstchar;
            avail++;
            if(avail >= MAX_BITS)
                return -1;

            /* If we've used up all the codewords of a given length
             * increase the length of codewords by one bit, but don't
             * exceed the specified maximum codeword size of 12 bits.
             */
            if (((avail & codemask) == 0) && (avail < 4096)) 
            {
                codesize++;
                codemask += avail;
            }
            oldcode = incode;
            
            /* Copy the decoded data out to the scanline buffer. */
            do {
                *rowp++ = *--stackp;
                if (rowp == rowend) {
                    OUTPUT_ROW(gs);
                }
            } while (stackp > stack);
        }
    }

  END:

    /* Home the local copies of the GIF decoder state variables */
    gs->avail = avail;
    gs->bits = bits;
    gs->codesize = codesize;
    gs->codemask = codemask;
    gs->count = count;
    gs->oldcode = oldcode;
    gs->firstchar = firstchar;
    gs->datum = datum;
    gs->stackp = stackp;
    gs->rowp = rowp;
    gs->rows_remaining = rows_remaining;

    return 0;
}

/*
 * setup an ic for gif decoding
 */
PRBool
il_gif_init(il_container *ic)
{
    gif_struct *gs;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;

    gs = PR_NEWZAP(gif_struct);
    if (gs) 
    {
        ic->ds = gs;
        gs->state = gif_init;
        gs->post_gather_state = gif_error;
                gs->gathered = 0;
        gs->ic = ic;
    }

    /* Initialize the container's source image header. */
    src_color_space->type = NI_PseudoColor;
    src_color_space->pixmap_depth = 8;
    src_color_space->bit_alloc.index_depth = 8;

    return (gs != 0);
}

static int
il_gif_init_transparency(il_container *ic, int index)
{
    IL_IRGB *src_trans_pixel = ic->src_header->transparent_pixel;
    IL_IRGB *img_trans_pixel;
    
    if (!src_trans_pixel) {
        src_trans_pixel = PR_NEWZAP(IL_IRGB);
        if (!src_trans_pixel)
            return PR_FALSE;
        ic->src_header->transparent_pixel = src_trans_pixel;

        /* Initialize the destination image's transparent pixel. */
        if(ic->imgdcb)
          ic->imgdcb->ImgDCBInitTransparentPixel();

        /* Set the source image's transparent pixel color to be the preferred
           transparency color of the destination image. */
        img_trans_pixel = ic->image->header.transparent_pixel;
        src_trans_pixel->red = img_trans_pixel->red;
        src_trans_pixel->green = img_trans_pixel->green;
        src_trans_pixel->blue = img_trans_pixel->blue;
    }
        
    /* Set the source image's transparent pixel index.  Do this even if the source
       image's transparent pixel has previously been set, since the index can vary
       from frame to frame in an animated gif. */
    src_trans_pixel->index = index;

    return PR_TRUE;
}


static void
il_gif_destroy_transparency(il_container *ic)
{
    NI_PixmapHeader *src_header = ic->src_header;
    
    if (src_header->transparent_pixel) {
        /* Destroy the source image's transparent pixel. */
        PR_FREEIF(src_header->transparent_pixel);
        src_header->transparent_pixel = NULL;

        /* Destroy the destination image's transparent pixel. */
        if(ic->imgdcb)
          ic->imgdcb->ImgDCBDestroyTransparentPixel();
    }
}


int
il_gif_compute_percentage_complete(int row, il_container *ic)
{
    PRUintn percent_height;
    int percent_done = 0;
    
    percent_height = (PRUintn)(row * (PRUint32)100 / ic->image->header.height);
    switch(ic->pass) {
    case 0: percent_done = percent_height; /* non-interlaced GIF */
        break;
    case 1: percent_done =      percent_height / 8;
        break;
    case 2: percent_done = 12 + percent_height / 8;
        break;
    case 3: percent_done = 25 + percent_height / 4;
        break;
    case 4: percent_done = 50 + percent_height / 2;
        break;
    default:
        ILTRACE(0,("Illegal interlace pass"));
        break;
    }

    return percent_done;
}

/* Maximum # of bytes to read ahead while waiting for delay_time to expire.
   We no longer limit this number to remain within WIN16 malloc limitations
   of 0xffff */

#define MAX_READ_AHEAD  (0xFFFFFFL)

PRUint8
il_gif_write_ready(il_container *ic)
{
    gif_struct *gs = (gif_struct *)ic->ds;
    int32 max;
    
    if (!gs)
        return 1;               /* Let imglib generic code decide */

    max = MAX(MAX_READ_AHEAD, gs->requested_buffer_fullness);
    if (gs->gathered < max)
        return 1;               /* Let imglib generic code decide */
    else
        return 0;               /* No more data until timeout expires */
}


static void
process_buffered_gif_input_data(gif_struct* gs)
{
    gstate state;
    il_container *ic = gs->ic;
    PRUint8 err = 0;

    /* Force any data we've buffered up to be processed. */
    err = il_gif_write(ic, (PRUint8 *) "", 0);

    /* The stream has already finished delivering data and the stream
       completion routine has been called sometime in the past. Now that
       we're actually done handling all of that data, call the stream
       completion routine again, but this time for real. */
    state = gs->state;

    if(ic->state==IC_ABORT_PENDING)
        state = gif_error;

    /* test, stop loopers if error */
    if( state == gif_error){
            ic->loop_count = 0;
            gs->destroy_pending = PR_TRUE;
            ic->state = IC_ABORT_PENDING;
    }
    if (gs->destroy_pending &&
        ((state == gif_done) || (state == gif_error) || (state == gif_oom))) {

        il_gif_abort(ic);

        if(ic->imgdcb)
          ic->imgdcb->ImgDCBHaveImageAll();
    }
}

void
gif_delay_time_callback(void *closure)
{
  il_container *ic = (il_container*)closure;
  gif_struct *gs = NULL;
  
  if((ic)&&(ic->ds))
     gs = (gif_struct *)ic->ds;
  else
     return;   //error


  PR_ASSERT(gs->state == gif_delay);
  gs->delay_timeout = NULL;

  if(gs->ic){
     if(gs->ic->type == NULL)
        gs->ic->type = nsCRT::strdup("image/gif"); //mime string
  }
  else
      return;  //error

  if (gs->ic->state == IC_ABORT_PENDING)
        return;
    
    gs->delay_time = 0;         /* Reset for next image */

    if (gs->state == gif_delay) {
        GETN(1, gif_image_start);
        process_buffered_gif_input_data(gs);
    }
    return;
}

/*
 * For the first images in the sequence clear the logical
 * screen to the background color, unless the first image
 * completely covers the logical screen, in which case
 * it's unnecessary.  XXX - This can be optimized.
 */

static int
gif_clear_screen(gif_struct *gs)
{
    PRUintn erase_width=0, erase_height=0, erase_x_offset=0, erase_y_offset=0;
    PRBool erase;
    il_container *ic = gs->ic;

    erase = PR_FALSE;
    if (gs->images_decoded == 0)
    {
        if ((gs->width  != gs->screen_width) ||
            (gs->height != gs->screen_height) ||
             gs->is_transparent)
        {
            erase = PR_TRUE;
            erase_width  = gs->screen_width;
            erase_height = gs->screen_height;
            erase_x_offset = erase_y_offset = 0;
        }
    }
    else
    {
        if (gs->last_disposal_method == DISPOSE_OVERWRITE_BGCOLOR)
        {
            erase = PR_TRUE;
            erase_width    = gs->last_width;
            erase_height   = gs->last_height;
            erase_x_offset = gs->last_x_offset;
            erase_y_offset = gs->last_y_offset;
        }
    }

    gs->last_disposal_method = gs->disposal_method;
    gs->last_width = gs->width;
    gs->last_height = gs->height;
    gs->last_x_offset = gs->x_offset;
    gs->last_y_offset = gs->y_offset;
            
    if (erase)
    {
        PRUintn i;
        int src_trans_pixel_index;
        PRUint8 *rowbuf = gs->rowbuf;
        NI_PixmapHeader *src_header = ic->src_header;
        IL_IRGB *saved_src_trans_pixel, *saved_img_trans_pixel;

        /* Catch images that fall outside the logical screen. */
        if ((erase_x_offset + erase_width) > gs->screen_width)
            erase_width = gs->screen_width - erase_x_offset;

        /* We have to temporarily pretend the image is transparent
           so we can clear using the context's background color. */
        saved_img_trans_pixel = ic->image->header.transparent_pixel;
        saved_src_trans_pixel = src_header->transparent_pixel;
        src_header->transparent_pixel = NULL;
        ic->image->header.transparent_pixel = NULL;

        /* Pick an index for the source image's temporary transparent pixel.
           The actual choice is immaterial since it will only be used for
           the clear screen operation. */

        src_trans_pixel_index = 0;
        if (!il_gif_init_transparency(ic, src_trans_pixel_index))
            return MK_OUT_OF_MEMORY;

        /* Now fill in the row buffer. */
        for (i = 0; i < erase_width; i++)
            rowbuf[i] = src_trans_pixel_index;
                    
        /* Note: We deliberately lie about the interlace
           pass number so that calls to il_flush_image_data()
           are done using a timer. */
        if (erase_width > 0)
          if(gs->ic->imgdcb)
            gs->ic->imgdcb->ImgDCBHaveRow(gs->rowbuf, gs->rgbrow,
                    erase_x_offset, erase_width,
                    erase_y_offset,erase_height, ilErase, 2);
           
        /* Reset the source image's transparent pixel to its former state. */
        il_gif_destroy_transparency(ic);
        src_header->transparent_pixel = saved_src_trans_pixel;
        ic->image->header.transparent_pixel = saved_img_trans_pixel;
    }
    return 0;
}


/*
 * process data arriving from the stream for the gif decoder
 */
 
int
il_gif_write(il_container *ic, const PRUint8 *buf, int32 len)
{
  int status;
  gif_struct *gs = (gif_struct *)ic->ds;
  NI_PixmapHeader *src_header = ic->src_header;
  NI_ColorMap *cmap = &src_header->color_space->cmap;
  const PRUint8 *q, *p=buf,*ep=buf+len;


    /* If this assert fires, chances are the netlib flubbed and
       continued to send data after the image stream was closed. */
    PR_ASSERT(gs);
    if (!gs) {
#ifdef DEBUG
        ILTRACE(1,("Netlib Error - imagelib image stream is closed\n"));
#endif
        return MK_IMAGE_LOSSAGE;
    }

    /* If this assert fires, some upstream data provider ignored the
       zero return value from il_gif_write_ready() which says not to
       send any more data to this stream until the delay timeout fires. */
  //  PR_ASSERT ((len == 0) || (gs->gathered < MAX_READ_AHEAD));
    if (!((len == 0) || (gs->gathered < MAX_READ_AHEAD)))
        return MK_INTERRUPTED;
    
    q = NULL;                   /* Initialize to shut up gcc warnings */
    
    while (p <= ep)
    {
        ILTRACE(9,("il:gif: state %d len %d buf %u p %u q %u ep %u",
                   gs->state,len,buf,p,q,ep));
        switch(gs->state)
        {
        case gif_lzw:
            if (do_lzw(gs, q) < 0)
            {
                gs->state=gif_error;
                break;
            }           
            GETN(1,gif_sub_block);
            break;
        
        case gif_lzw_start:
        {
            int i;

            cmap->map = gs->is_local_colormap_defined ?
                gs->local_colormap : gs->global_colormap;

            PR_ASSERT(cmap->map);
            if (!cmap->map)
                return MK_IMAGE_LOSSAGE;

            /* Now we know how many colors are in our colormap. */
            if (gs->is_local_colormap_defined || (gs->images_decoded == 0)){
                if(ic->imgdcb){
                nsresult rv = ic->imgdcb->ImgDCBSetupColorspaceConverter(); 
                if(NS_FAILED(rv))
                    return MK_IMAGE_LOSSAGE;
                }
            }

            status = gif_clear_screen(gs);             
            if (status < 0)
               return status;

            /* Initialize LZW parser/decoder */
            gs->datasize = *q;
            if(gs->datasize > MAX_LZW_BITS)
            {
                gs->state=gif_error;
                break;
            }

            gs->clear_code = 1 << gs->datasize;
            gs->avail = gs->clear_code + 2;
            gs->oldcode = -1;
            gs->codesize = gs->datasize + 1;
            gs->codemask = (1 << gs->codesize) - 1;

            gs->datum = gs->bits = 0;

            if (!gs->prefix)        
                gs->prefix = (PRUint16 *)PR_Calloc(sizeof(PRUint16), MAX_BITS);
            if (!gs->suffix)
                gs->suffix = ( PRUint8 *)PR_Calloc(sizeof(PRUint8),  MAX_BITS);
            if (!gs->stack)
                gs->stack  = ( PRUint8 *)PR_Calloc(sizeof(PRUint8),  MAX_BITS);
                    
            if( !gs->prefix || !gs->suffix || !gs->stack)
            {
                /* complete from abort will free prefix & suffix */
                ILTRACE(0,("il:gif: MEM stack"));
                gs->state=gif_oom;
                break;
            }

            if(gs->clear_code >= MAX_BITS)
            {
                gs->state=gif_error;
                break;
            }

            /* init the tables */
            for (i=0; i < gs->clear_code; i++) 
                gs->suffix[i] = i;

            gs->stackp = gs->stack;

            GETN(1,gif_sub_block);
        }
        break;

        /* We're positioned at the very start of the file. */
        case gif_init:
        {
            GETN(3,gif_type);
            break;
        }

        /* All GIF files begin with "GIF87a" or "GIF89a" */
        case gif_type:
        {
            if (strncmp((char*)q,"GIF",3))
            {
                ILTRACE(2,("il:gif: not a GIF file"));
                gs->state=gif_error;
                break;
            }
            GETN(3,gif_version);
        }
        break;

        case gif_version:
        {
            if(!strncmp((char*)q,"89a",3))
            {
                gs->version=89;
            }
            else
            {
                if(!strncmp((char*)q,"87a",3))
                {
                    gs->version=87;
                }
                else
                {
                    ILTRACE(2,("il:gif: unrecognized GIF version number"));
                    gs->state=gif_error;
                    break;
                }
            }
            ILTRACE(2,("il:gif: %d gif", gs->version));
            GETN(7,gif_global_header);
        }
        break;

        case gif_global_header:
        {
            /* This is the height and width of the "screen" or
             * frame into which images are rendered.  The
             * individual images can be smaller than the
             * screen size and located with an origin anywhere
             * within the screen.
             */
                    
            gs->screen_width = GETINT16(q); 
            gs->screen_height = GETINT16(q+2);

            gs->screen_bgcolor = q[5];

            gs->global_colormap_size = 2<<(q[4]&0x07);

            /* A -ve value for cmap->num_colors indicates that the colors may
               be non-unique.*/
            cmap->num_colors = -gs->global_colormap_size;
            cmap->map = NULL;

            if(q[6])
            {
                /* should assert gif89 */
                if(q[6] != 49)
                {
#ifdef DEBUG
                    float aspect = (float)((q[6] + 15) / 64.0);
                    ILTRACE(2, ("il:gif: %f aspect ratio", aspect));
#endif
                }
            }

            if( q[4] & 0x80 ) /* global map */
            {
                GETN(gs->global_colormap_size*3, gif_global_colormap);
            }
            else
            {
                GETN(1, gif_image_start);
            }
        }
        break;

        case gif_global_colormap:
        {
            IL_RGB* map;
            int i;

            if(!(map = (IL_RGB*)PR_Calloc(gs->global_colormap_size,
                                          sizeof(IL_RGB))))
            {
                ILTRACE(0,("il:gif: MEM map"));
                gs->state=gif_oom;
                break;
            }
           
            if(ic->imgdcb)
               ic->imgdcb->ImgDCBResetPalette();
            gs->global_colormap = map;

#ifndef M12N                    /* Fix me. */
#ifdef XP_MAC
            im->hasUniqueColormap = 1;
#endif
#endif /* M12N */
            for (i=0; i < gs->global_colormap_size; i++, map++) 
            {
                map->red = *q++;
                map->green = *q++;
                map->blue = *q++;
            }

            GETN(1,gif_image_start);
        }
        break;

        case gif_image_start:
        {
            if(*q==';') /* terminator */
            {
                gs->state = gif_done;
                break;
            }

            if(*q=='!') /* extension */
            {
                GETN(2,gif_extension);
                break;
            }
                        
            if(*q!=',') /* invalid start character */
            {
                ILTRACE(2,("il:gif: bogus start character 0x%02x",
                           (int)*q));
                gs->state=gif_error;
                break;
            }
            else
            {
                /* If this is a multi-part GIF, flush the last image */
                if (gs->images_decoded) {
                    ic->multi++;    /* Avoid progressive display */
                }
            
                GETN(9, gif_image_header);
            }
        }
        break;

        case gif_extension:
        {
            int len = gs->count = q[1];
            gstate es = gif_skip_block;

            ILTRACE(2,("il:gif: %d byte extension %x", len, (int)*q));
            switch(*q)
            {
            case 0xf9:
                es = gif_control_extension;
                break;

            case 0x01:
                ILTRACE(2,("il:gif: ignoring plain text extension"));
                break;
                            
            case 0xff:
                es = gif_application_extension;
                break;

            case 0xfe:
                es = gif_consume_comment;
                break;
            }
            
            if (len)
                GETN(len, es);
            else
                GETN(1, gif_image_start);
        }
        break;

        case gif_consume_block:
        {
            if(!*q)
            {
                GETN(1, gif_image_start);
            }
            else
            {
                GETN(*q, gif_skip_block);
            }
        }
        break;
            
        case gif_skip_block:
            GETN(1, gif_consume_block);
            break;

        case gif_control_extension:
        {
            if(*q & 0x1)
            {
                gs->tpixel = *(q+3);
                ILTRACE(2,("il:gif: transparent pixel %d", gs->tpixel));
                if (!il_gif_init_transparency(ic, gs->tpixel))
                    return MK_OUT_OF_MEMORY;
                gs->is_transparent = PR_TRUE;
            }
            else
            {
                ILTRACE(2,("il:gif: ignoring gfx control extension"));
            }
            gs->control_extension = PR_TRUE;
            gs->disposal_method = (gdispose)(((*q) >> 2) & 0x7);
            gs->delay_time = GETINT16(q + 1) * 10;
            GETN(1,gif_consume_block);
        }
        break;

        case gif_comment_extension:
        {
            gs->count = *q;
            if (gs->count) 
                GETN(gs->count, gif_consume_comment);
            else
                GETN(1, gif_image_start);
        }
        break;

        case gif_consume_comment:
        {           
            BlockAllocCat(ic->comment, ic->comment_length, (char*)q, gs->count);          
            ic->comment_length += gs->count;
            BlockAllocCat(ic->comment, ic->comment_length, "", 1);
            ic->comment_length++;
            GETN(1, gif_comment_extension);
        }
        break;

        case gif_application_extension:
            /* Check for netscape application extension */
            if (!strncmp((char*)q, "NETSCAPE2.0", 11) ||
                !strncmp((char*)q, "ANIMEXTS1.0", 11))
                GETN(1, gif_netscape_extension_block);
            else
                GETN(1, gif_consume_block);
            break;

        /* Netscape-specific GIF extension: animation looping */
        case gif_netscape_extension_block:
            if (*q)
                GETN(*q, gif_consume_netscape_extension);
            else
                GETN(1, gif_image_start);
            break;

        /* Parse netscape-specific application extensions */
        case gif_consume_netscape_extension:
        {
            int netscape_extension = q[0] & 7;

            /* Loop entire animation specified # of times.  Only read the
               loop count during the first iteration. */
            if (netscape_extension == 1) {
                if (!ic->is_looping) {
                    ic->loop_count = GETINT16(q + 1);

                    /* Zero loop count is infinite animation loop request */
                    if (ic->loop_count == 0)
                        ic->loop_count = -1;

                    /* Tell the front end that the stop state might have changed */
                    /* because of the looping GIF.                               */
#ifndef M12N                    /* XXXM12N Fix me. */
                    if (ic->net_cx)
                      FE_UpdateStopState(ic->net_cx);
#endif /* M12N */
                }

                GETN(1, gif_netscape_extension_block);
            }

            /* Wait for specified # of bytes to enter buffer */
            else if (netscape_extension == 2)
            {
                gs->requested_buffer_fullness = GETINT32(q + 1);
                GETN(gs->requested_buffer_fullness, gif_wait_for_buffer_full);
            }

            break;
        }

        case gif_wait_for_buffer_full:
            gs->gathered = gs->requested_buffer_fullness;
            GETN(1, gif_netscape_extension_block);
            break;

        case gif_image_header:
        {
            PRUintn height, width;
                                                                                                                                    
            /* Get image offsets, with respect to the screen origin */
            gs->x_offset = GETINT16(q);
            gs->y_offset = GETINT16(q + 2);

            /* Get image width and height. */
            width  = GETINT16(q + 4);            
            height = GETINT16(q + 6);

            ILTRACE(2,("il:gif: screen %dx%d, image %dx%d", 
                       gs->screen_width, gs->screen_height, width, height));

            /* Work around broken GIF files where the logical screen
             * size has weird width or height.  We assume that GIF87a
             * files don't contain animations.
             */
         if ((gs->images_decoded == 0) &&
                ((gs->screen_height < height) || (gs->screen_width < width) ||
                 (gs->version == 87))){           
                gs->screen_height = height;
                gs->screen_width = width;
                gs->x_offset = 0;
                gs->y_offset = 0;
            }
            /* Work around more broken GIF files that have zero image
               width or height */
            if (!height || !width) 
            {
                height = gs->screen_height;
                width = gs->screen_width;
            }

            gs->height = height;
            gs->width = width;

            /* This case will never be taken if this is the first image */
            /* being decoded. If any of the later images are larger     */
            /* than the screen size, we need to reallocate buffers.     */
            if (gs->screen_width < width) {

                gs->rgbrow = (PRUint8*)PR_REALLOC(gs->rgbrow, 3 * width);
                gs->rowbuf = (PRUint8*)PR_REALLOC(gs->rowbuf, width);

                if((!gs->rgbrow)||(!gs->rowbuf)){
                  gs->state = gif_oom;
                  break;
                }
            
                gs->screen_width = width;
                if(gs->screen_height < gs->height )
                    gs->screen_height = gs->height;

                ic->src_header->width = gs->width;
                ic->src_header->height = gs->height;
                ic->image->header.width = gs->width;
                ic->image->header.height = gs->height;
                ic->image->header.widthBytes = (ic->image->header.width * ic->image->header.color_space->pixmap_depth + 7)/8; 

                ic->image->header.widthBytes = ROUNDUP(ic->image->header.widthBytes, 4);

                if(ic->imgdcb){
                  nsresult rv = ic->imgdcb->ImgDCBImageSize();
                 
                  if(NS_FAILED(rv)){
                      gs->state = gif_error;
                      break;
                  }
                }
                else{    //no callback is an error too.
                    gs->state = gif_error;
                    break;
                }

                ic->img_cx->img_cb->NewPixmap(ic->img_cx->dpy_cx, ic->dest_width,
                            ic->dest_height, ic->image, ic->mask);
    
                if((!ic->scalerow)||(!ic->image->bits)||(ic->mask && !ic->mask->bits)){
                    gs->state=gif_oom;
                    break;
                }
        }
        else{
            if (!gs->rgbrow)
                gs->rgbrow = (PRUint8*)PR_MALLOC(3 * gs->screen_width);

            if (!gs->rowbuf)
                gs->rowbuf = (PRUint8*)PR_MALLOC(gs->screen_width);
        }

            if (!gs->rowbuf || !gs->rgbrow)
            {
                    ILTRACE(0,("il:gif: MEM row"));
                    gs->state=gif_oom;
                    break;
            }
                    
            /* Free transparency from earlier image in multi-image sequence. */
            if (!gs->is_transparent && src_header->transparent_pixel) { 
                il_gif_destroy_transparency(ic);
            }

            if (gs->images_decoded == 0) {
                src_header->width = gs->screen_width;
                src_header->height = gs->screen_height;

                if(ic->imgdcb){
                    nsresult rv = ic->imgdcb->ImgDCBImageSize();
                 
                    if(NS_FAILED(rv)){
                        gs->state = gif_error;
                        break;
                    }
                }
                else{    //no callback is an error too.
                    gs->state = gif_error;
                    break;
                }
            }

            if ( *(q+8) & 0x40 )
            {
                ILTRACE(2,("il:gif: interlaced"));
                gs->interlaced = PR_TRUE;
                gs->ipass = 1;
            } else {
                gs->interlaced = PR_FALSE;
                gs->ipass = 0;
            }
            
            if (gs->images_decoded == 0)
            {
                gs->progressive_display = ic->img_cx->progressive_display;
            } else {

                /* Overlaying interlaced, transparent GIFs over
                   existing image data using the Haeberli display hack
                   requires saving the underlying image in order to
                   avoid jaggies at the transparency edges.  We are
                   unprepared to deal with that, so don't display such
                   images progressively */
                gs->progressive_display = ic->img_cx->progressive_display &&
                    !(gs->interlaced && gs->is_transparent);
            }

            /* Clear state from last image */
            gs->requested_buffer_fullness = 0;
            gs->irow = 0;
            gs->rows_remaining = gs->height;
            gs->rowend = gs->rowbuf + gs->width;
            gs->rowp = gs->rowbuf;

            /* bits per pixel is 1<<((q[8]&0x07)+1); */

            if ( *(q+8) & 0x80 ) 
            {
                int num_colors = 2 << (*(q + 8) & 0x7);
                
                if ((num_colors > gs->local_colormap_size) &&
                    gs->local_colormap)
                {
                    PR_FREEIF(gs->local_colormap);
                    gs->local_colormap = NULL;
                }
                gs->local_colormap_size = num_colors;
                /* A -ve value for cmap->num_colors indicates that the colors
                   may be non-unique. */
                cmap->num_colors = -num_colors;

                /* Switch to the new local palette after it loads */
                if(ic->imgdcb)
                   ic->imgdcb->ImgDCBResetPalette();

                gs->is_local_colormap_defined = PR_TRUE;
                GETN(gs->local_colormap_size * 3, gif_image_colormap);
            }
            else
            {
                /* Switch back to the global palette */
                if (gs->is_local_colormap_defined)
                  if(ic->imgdcb)
                    ic->imgdcb->ImgDCBResetPalette();

                gs->is_local_colormap_defined = PR_FALSE;
                GETN(1, gif_lzw_start);
            }
        }
        break;

        case gif_image_colormap:
        {
            IL_RGB* map;
            int i;

            ILTRACE(2,("il:gif: local colormap"));
                    
            map = gs->local_colormap;
            if (!map) 
            {
                map = gs->local_colormap = (IL_RGB*)PR_Calloc(
                    gs->local_colormap_size, sizeof(IL_RGB));

                if(!map)
                {
                    ILTRACE(0,("il:gif: MEM map"));
                    gs->state=gif_oom;
                    break;
                }
            }

#ifndef M12N                    /* Fix me */
#ifdef XP_MAC
            im->hasUniqueColormap = 1;
#endif
#endif /* M12N */

            for (i=0; i < gs->local_colormap_size; i++, map++) 
            {
                map->red   = *q++;
                map->green = *q++;
                map->blue  = *q++;
            }

            GETN(1,gif_lzw_start);
        }
        break;

        case gif_sub_block:
        {
            if ((gs->count = *q) != 0)
            /* Still working on the same image: Process next LZW data block */
            {
                /* Make sure there are still rows left. If the GIF data */
                /* is corrupt, we may not get an explicit terminator.   */
                if (gs->rows_remaining == 0) {
                    ILTRACE(3,("il:gif: missing image terminator, continuing"));
                    /* This is an illegal GIF, but we remain tolerant. */
#ifdef DONT_TOLERATE_BROKEN_GIFS
                    gs->state=gif_error;
                    break;
#else
                    GETN(1,gif_sub_block);
#endif
                }
                GETN(gs->count, gif_lzw);
            }
            else
            /* See if there are any more images in this sequence. */                
            {
                /* Flush the image data unconditionally, so that we can
                   notify observers that the current frame has completed. */
                if(ic->imgdcb){
                     ic->imgdcb->ImgDCBFlushImage();
                     ic->imgdcb->ImgDCBHaveImageFrame();
                }
        
                gs->images_decoded++;                

                /* Clear state from this image */
                gs->control_extension = PR_FALSE;
                gs->is_transparent = PR_FALSE;

                if(ic->animate_request == eImageAnimation_None){
                    /* This is not really an error, but a mechanism
                    to stop decoding of subsequent frames. Only the
                    first frame is displayed for eImageAnimation_None.
                    */
                    gs->state = gif_stop_animating;   
                    break;
                }

                /* An image can specify a delay time before which to display
                   subsequent images.  Block until the appointed time. */
                if(gs->delay_time < MINIMUM_DELAY_TIME )
                    gs->delay_time = MINIMUM_DELAY_TIME;

                if (gs->delay_time){
                    if(ic->imgdcb){
                          gs->delay_timeout = (void *)
                                ic->imgdcb->ImgDCBSetTimeout(gif_delay_time_callback, gs->ic, gs->delay_time);
                     }
                    /* Essentially, tell the decoder state machine to wait
                       forever.  The timeout callback routine will wake up the
                       state machine and force it to decode the next image. */
                    GETN(1L<<30, gif_image_start);
                    gs->state = gif_delay;
                } else {
                    GETN(1, gif_image_start);
                }

            }
        }
        break;

        case gif_done:                
            return 0;
            break;

        case gif_delay:
        case gif_gather:    
        {
            int32 gather_remaining;
            int32 request_size = gs->gather_request_size;
            
            {
                gather_remaining = request_size - gs->gathered;

                /* Do we already have enough data in the accumulation
                   buffer to satisfy the request ?  (This can happen
                   after we transition from the gif_delay state.) */
                if (gather_remaining <= 0)
                {
                    gs->gathered -= request_size;
                    q = gs->gather_head;
                    gs->gather_head += request_size;
                    gs->state = gs->post_gather_state;
                    break;
                }

                /* Shift remaining data to the head of the buffer */
                if (gs->gathered && (gs->gather_head != gs->hold)) {
                    nsCRT::memmove(gs->hold, gs->gather_head, gs->gathered);
                    gs->gather_head = gs->hold;
                }
                 
                /* If we add the data just handed to us by the netlib
                   to what we've already gathered, is there enough to satisfy 
                   the current request ? */
                if ((ep - p) >= gather_remaining)
                {
                    if(gs->gathered)
                    { /* finish a prior gather */
                        char *hold = (char*)gs->hold;
                        BlockAllocCat(hold, gs->gathered, (char*)p, gather_remaining);
                        gs->hold = (PRUint8*)hold;
                        q = gs->gather_head = gs->hold;
                        gs->gathered = 0;
                    }
                    else
                    {
                        q = p;
                    }
                    p += gather_remaining;
                    gs->state = gs->post_gather_state;
                }
                else
                { 
                    char *hold = (char*)gs->hold;
                    BlockAllocCat(hold, gs->gathered, (char*)p, ep - p);
                    gs->hold = (PRUint8*)hold;
                    gs->gather_head = gs->hold;
                    gs->gathered += ep-p;
                    return 0;
                }
            }
        }
        break;

        case gif_oom: 
            ILTRACE(1,("il:gif: reached oom state"));
            return MK_OUT_OF_MEMORY;
            break;

        case gif_error: 
            ILTRACE(2,("il:gif: reached error state"));
            return MK_IMAGE_LOSSAGE;
            break;

        case gif_stop_animating:
            return 0;
            break;

        default: 
            ILTRACE(0,("il:gif: unknown state"));
            PR_ASSERT(0);
            break;
        }
    }

    return 0;
}

void
il_gif_complete(il_container *ic)
{
    if (ic->ds)
    {
            gif_struct *gs = (gif_struct*) ic->ds;

        /* No more data in the stream, but we may still have work to do,
           so don't actually free any of the data structures. */
        if (gs->delay_timeout) {
            /* We will free the data structures when image display completes. */
            gs->destroy_pending = PR_TRUE;
            return;
        } else if (gs->requested_buffer_fullness) {
            /* We will free the data structures when image display completes. */
            gs->destroy_pending = PR_TRUE;
            process_buffered_gif_input_data(gs);
            return;
        }
        if(!(ic->is_multipart))
            il_gif_abort(ic);
    }
    
    if (ic->imgdcb)
       ic->imgdcb->ImgDCBHaveImageAll();
    else
       il_gif_abort(ic);
       
}

/* Free up all the data structures associated with decoding a GIF */
void
il_gif_abort(il_container *ic)
{
    if (ic->ds) 
    {
        gif_struct *gs = (gif_struct*) ic->ds;

        /* Clear any pending timeouts */
        if (gs->delay_timeout) {
            ic->imgdcb->ImgDCBClearTimeout(gs->delay_timeout);
            gs->delay_timeout = NULL;
        }
    
        PR_FREEIF(gs->rowbuf);
        PR_FREEIF(gs->rgbrow);
        PR_FREEIF(gs->prefix);
        PR_FREEIF(gs->suffix);
        PR_FREEIF(gs->stack);

        PR_FREEIF(gs->hold);

        /* Free the colormap that is not in use.  The other one, if
         * present, will be freed when the image container is
         * destroyed.
         */

        if (gs->is_local_colormap_defined) {
            if (gs->local_colormap) {
                PR_FREEIF(gs->local_colormap);
                gs->local_colormap = NULL;
                ic->src_header->color_space->cmap.map = NULL;
            }
        }
        if (gs->global_colormap) {
                PR_FREEIF(gs->global_colormap);
                gs->global_colormap = NULL;
                ic->src_header->color_space->cmap.map = NULL;
        }

       
        PR_FREEIF(gs);
        ic->ds = 0;
    }
}

