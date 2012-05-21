/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_oom_h
#define mozilla_mozalloc_oom_h

#include "mozalloc.h"

#if defined(MOZALLOC_EXPORT)
// do nothing: it's been defined to __declspec(dllexport) by
// mozalloc*.cpp on platforms where that's required
#elif defined(XP_WIN) || (defined(XP_OS2) && defined(__declspec))
#  define MOZALLOC_EXPORT __declspec(dllimport)
#elif defined(HAVE_VISIBILITY_ATTRIBUTE)
/* Make sure symbols are still exported even if we're wrapped in a
 * |visibility push(hidden)| blanket. */
#  define MOZALLOC_EXPORT __attribute__ ((visibility ("default")))
#else
#  define MOZALLOC_EXPORT
#endif


/**
 * Called when memory is critically low.  Returns iff it was able to 
 * remedy the critical memory situation; if not, it will abort().
 * 
 * We have to re-#define MOZALLOC_EXPORT because this header can be
 * used indepedently of mozalloc.h.
 */
MOZALLOC_EXPORT void mozalloc_handle_oom(size_t requestedSize);

/**
 * Called by embedders (specifically Mozilla breakpad) which wants to be
 * notified of an intentional abort, to annotate any crash report with
 * the size of the allocation on which we aborted.
 */
typedef void (*mozalloc_oom_abort_handler)(size_t size);
MOZALLOC_EXPORT void mozalloc_set_oom_abort_handler(mozalloc_oom_abort_handler handler);

/* TODO: functions to query system memory usage and register
 * critical-memory handlers. */


#endif /* ifndef mozilla_mozalloc_oom_h */
