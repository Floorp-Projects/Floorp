/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

PR_IMPLEMENT(void)
    _MD_init_segs (void)
{
}

PR_IMPLEMENT(PRStatus)
    _MD_alloc_segment (PRSegment *seg, PRUint32 size, void *vaddr)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PR_IMPLEMENT(void)
    _MD_free_segment (PRSegment *seg)
{
}
