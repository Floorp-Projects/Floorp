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

/* ipng.h */


typedef enum {
    PNG_ERROR,
    PNG_INIT,
    PNG_CONTINUE,
    PNG_DELAY,
    PNG_FINISH,
    PNG_DONE           
} png_state;


typedef struct ipng_str {
    png_state state;		/* Decoder FSM state */
 
/*  int rows_per_chunk;		NOT USED (similar variable in jpeg_struct) */
    void *delay_timeout;
    PRUint32 delay_time;

    png_structp pngs_p;
    png_infop info_p;
    jmp_buf jmpbuf;		/* use ours, not libpng's, for consistency */
    PRUint32 width;
    PRUint32 height;
    int channels;               /* color channels (3 or 4) */
    PRUint8 *rgbrow;              /* RGB row buffer (3*width bytes) */
    PRUint8 *alpharow;            /* alpha row buffer (width bytes) */
    PRUint8 *interlacebuf;        /* interlace buffer */

    /* One scanline's worth of post-processed sample data */

    il_container *ic;

} ipng_struct, *ipng_structp;

