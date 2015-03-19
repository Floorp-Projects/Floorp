/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"


/******************************************************************/
/*  Required Platform Routines                                    */
/*                                                                */
/*     These routines are called from the common SDP code.        */
/*     They must be provided for each platform.                   */
/*                                                                */
/******************************************************************/

/*
 * sdp_dump_buffer
 *
 * Utility to send _size_bytes of data from the string
 * pointed to by _ptr to the buginf function. This may make
 * multiple buginf calls if the buffer is too large for buginf.
 */
void sdp_dump_buffer (char * _ptr, int _size_bytes)
{
    CSFLogDebug("sdp", _ptr);
}

/******************************************************************/
/*                                                                */
/*  Platform Specific Routines                                    */
/*                                                                */
/*    These routines are only used in this particular platform.   */
/*    They are called from the required platform specific         */
/*    routines provided below, not from the common SDP code.      */
/*                                                                */
/******************************************************************/

/* There are currently no platform specific routines required. */
