/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

PR_EXTERN(void) _PR_MD_INIT_SEGS(void);
PR_EXTERN(PRStatus) _PR_MD_ALLOC_SEGMENT(PRSegment *seg, PRUint32 size, void *vaddr);
PR_EXTERN(void) _PR_MD_FREE_SEGMENT(PRSegment *seg);
