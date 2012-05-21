/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* We need to get a separate copy of the ELF-code into
   libunwind-ptrace since it cannot (and must not) have any ELF
   dependencies on libunwind.  */
#include "libunwind_i.h"	/* get ELFCLASS defined */
#include "../elfxx.c"
