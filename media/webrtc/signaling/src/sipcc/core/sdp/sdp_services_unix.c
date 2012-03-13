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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

void sdp_log_errmsg (sdp_errmsg_e errmsg, char *str)
{
    switch (errmsg) {

    case SDP_ERR_INVALID_CONF_PTR:
        SDP_ERROR("\nSDP: Invalid Config pointer (%s).", str);
        break;

    case SDP_ERR_INVALID_SDP_PTR:
        SDP_ERROR("\nSDP: Invalid SDP pointer (%s).", str);
        break;

    case SDP_ERR_INTERNAL:
        SDP_ERROR("\nSDP: Internal error (%s).", str);
        break;

    default:
        break;
    }
}

/*
 * sdp_dump_buffer
 *
 * Utility to send _size_bytes of data from the string
 * pointed to by _ptr to the buginf function. This may make
 * multiple buginf calls if the buffer is too large for buginf.
 */
void sdp_dump_buffer (char * _ptr, int _size_bytes)
{
    buginf_msg(_ptr);
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
