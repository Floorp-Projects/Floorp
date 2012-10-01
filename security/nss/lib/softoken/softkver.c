/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Library identity and versioning */

#include "softkver.h"

#if defined(DEBUG)
#define _DEBUG_STRING " (debug)"
#else
#define _DEBUG_STRING ""
#endif

/*
 * Version information for the 'ident' and 'what commands
 *
 * NOTE: the first component of the concatenated rcsid string
 * must not end in a '$' to prevent rcs keyword substitution.
 */
const char __nss_softokn_rcsid[] = "$Header: NSS " SOFTOKEN_VERSION _DEBUG_STRING
        "  " __DATE__ " " __TIME__ " $";
const char __nss_softokn_sccsid[] = "@(#)NSS " SOFTOKEN_VERSION _DEBUG_STRING
        "  " __DATE__ " " __TIME__;
