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
	png_state state;				/* Decoder FSM state */
 
    int rows_per_chunk;
    void *delay_timeout;
    uint32 delay_time;
    png_struct *pngs_p;
    png_infop  info_p;

	/* One scanline's worth of post-processed sample data */

	il_container *ic;
} ipng_struct, *ipng_structp;

