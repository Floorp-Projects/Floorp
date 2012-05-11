/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

PR_EXTERN(void) _PR_MD_INIT_SEGS(void);
PR_EXTERN(PRStatus) _PR_MD_ALLOC_SEGMENT(PRSegment *seg, PRUint32 size, void *vaddr);
PR_EXTERN(void) _PR_MD_FREE_SEGMENT(PRSegment *seg);
