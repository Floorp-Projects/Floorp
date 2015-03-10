/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc_abort.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/Assertions.h"

static mozalloc_oom_abort_handler gAbortHandler;

#define OOM_MSG_LEADER "out of memory: 0x"
#define OOM_MSG_DIGITS "0000000000000000" // large enough for 2^64
#define OOM_MSG_TRAILER " bytes requested"
#define OOM_MSG_FIRST_DIGIT_OFFSET sizeof(OOM_MSG_LEADER) - 1
#define OOM_MSG_LAST_DIGIT_OFFSET sizeof(OOM_MSG_LEADER) + \
                                  sizeof(OOM_MSG_DIGITS) - 3

static const char *hex = "0123456789ABCDEF";

void
mozalloc_handle_oom(size_t size)
{
    char oomMsg[] = OOM_MSG_LEADER OOM_MSG_DIGITS OOM_MSG_TRAILER;
    size_t i;

    // NB: this is handle_oom() stage 1, which simply aborts on OOM.
    // we might proceed to a stage 2 in which an attempt is made to
    // reclaim memory

    if (gAbortHandler)
        gAbortHandler(size);

    static_assert(OOM_MSG_FIRST_DIGIT_OFFSET > 0,
                  "Loop below will never terminate (i can't go below 0)");

    // Insert size into the diagnostic message using only primitive operations
    for (i = OOM_MSG_LAST_DIGIT_OFFSET;
         size && i >= OOM_MSG_FIRST_DIGIT_OFFSET; i--) {
      oomMsg[i] = hex[size % 16];
      size /= 16;
    }

    mozalloc_abort(oomMsg);
}

void
mozalloc_set_oom_abort_handler(mozalloc_oom_abort_handler handler)
{
    gAbortHandler = handler;
}
