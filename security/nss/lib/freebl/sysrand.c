/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "seccomon.h"

#if defined(XP_UNIX) && defined(SEED_ONLY_DEV_URANDOM)
#include "unix_urandom.c"
#elif defined(XP_UNIX)
#include "unix_rand.c"
#endif
#ifdef XP_WIN
#include "win_rand.c"
#endif
