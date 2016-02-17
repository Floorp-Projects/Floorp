/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Library identity and versioning */

#include "nssckbi.h"

#if defined(DEBUG)
#define _DEBUG_STRING " (debug)"
#else
#define _DEBUG_STRING ""
#endif

/*
 * Version information
 */
const char __nss_builtins_version[] = "Version: NSS Builtin Trusted Root CAs "
        NSS_BUILTINS_LIBRARY_VERSION _DEBUG_STRING;
