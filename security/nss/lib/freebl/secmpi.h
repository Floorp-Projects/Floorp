/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "mpi.h"

 /* XXX to be replaced by define in blapit.h */
#define NSS_FREEBL_DEFAULT_CHUNKSIZE 2048

#define CHECK_SEC_OK(func) if (SECSuccess != (rv = func)) goto cleanup

#define CHECK_MPI_OK(func) if (MP_OKAY > (err = func)) goto cleanup

#define OCTETS_TO_MPINT(oc, mp, len) \
    CHECK_MPI_OK(mp_read_unsigned_octets((mp), oc, len))

#define SECITEM_TO_MPINT(it, mp) \
    CHECK_MPI_OK(mp_read_unsigned_octets((mp), (it).data, (it).len))

#define MPINT_TO_SECITEM(mp, it, arena)                         \
    SECITEM_AllocItem(arena, (it), mp_unsigned_octet_size(mp)); \
    err = mp_to_unsigned_octets(mp, (it)->data, (it)->len);     \
    if (err < 0) goto cleanup; else err = MP_OKAY;

#define MP_TO_SEC_ERROR(err)                                          \
    switch (err) {                                                    \
    case MP_MEM:    PORT_SetError(SEC_ERROR_NO_MEMORY);       break;  \
    case MP_RANGE:  PORT_SetError(SEC_ERROR_BAD_DATA);        break;  \
    case MP_BADARG: PORT_SetError(SEC_ERROR_INVALID_ARGS);    break;  \
    default:        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE); break;  \
    }
