/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jprof_h___
#define jprof_h___
#include "nscore.h"

#ifdef _IMPL_JPPROF_API
#define JPROF_API(type) NS_EXPORT_(type)
#else
#define JPROF_API(type) NS_IMPORT_(type)
#endif

JPROF_API(void) setupProfilingStuff(void);

#endif /* jprof_h___ */
