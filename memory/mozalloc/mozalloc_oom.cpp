/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN) || defined(XP_OS2)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#include "mozilla/mozalloc_abort.h"
#include "mozilla/mozalloc_oom.h"

static mozalloc_oom_abort_handler gAbortHandler;

void
mozalloc_handle_oom(size_t size)
{
    // NB: this is handle_oom() stage 1, which simply aborts on OOM.
    // we might proceed to a stage 2 in which an attempt is made to
    // reclaim memory

    if (gAbortHandler)
        gAbortHandler(size);

    mozalloc_abort("out of memory");
}

void
mozalloc_set_oom_abort_handler(mozalloc_oom_abort_handler handler)
{
    gAbortHandler = handler;
}
