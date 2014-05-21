/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef libmalloc_h___
#define libmalloc_h___

#include <sys/types.h>
#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

typedef unsigned long u_long;

// For me->flags
#define JP_FIRST_AFTER_PAUSE 1

// Format of a jprof log entry. This is what's written out to the
// "jprof-log" file.
// It's called malloc_log_entry because the history of jprof is that
// it's a modified version of tracemalloc.
struct malloc_log_entry {
  u_long delTime;
  u_long numpcs;
  unsigned int flags;
  int thread;
  char* pcs[MAX_STACK_CRAWL];
};

// Format of a malloc map entry; after this struct is nameLen+1 bytes of
// name data.
struct malloc_map_entry {
  u_long nameLen;
  u_long address;		// base address
};

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* libmalloc_h___ */
