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
