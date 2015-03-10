/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_oom_h
#define mozilla_mozalloc_oom_h

#include "mozalloc.h"

/**
 * Called when memory is critically low.  Returns iff it was able to
 * remedy the critical memory situation; if not, it will abort().
 */
MFBT_API void mozalloc_handle_oom(size_t requestedSize);

/**
 * Called by embedders (specifically Mozilla breakpad) which wants to be
 * notified of an intentional abort, to annotate any crash report with
 * the size of the allocation on which we aborted.
 */
typedef void (*mozalloc_oom_abort_handler)(size_t size);
MFBT_API void mozalloc_set_oom_abort_handler(mozalloc_oom_abort_handler handler);

/* TODO: functions to query system memory usage and register
 * critical-memory handlers. */


#endif /* ifndef mozilla_mozalloc_oom_h */
