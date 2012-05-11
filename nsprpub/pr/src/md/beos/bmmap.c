/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

PR_EXTERN(PRStatus)
_PR_MD_CREATE_FILE_MAP(PRFileMap *fmap, PRInt64 size)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return PR_FAILURE;
}

PR_EXTERN(PRInt32)
_PR_MD_GET_MEM_MAP_ALIGNMENT(void)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return -1;
}

PR_EXTERN(void *)
_PR_MD_MEM_MAP(PRFileMap *fmap, PRInt64 offset, PRUint32 len)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return 0;
}

PR_EXTERN(PRStatus)
_PR_MD_MEM_UNMAP(void *addr, PRUint32 size)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return PR_FAILURE;
}

PR_EXTERN(PRStatus)
_PR_MD_CLOSE_FILE_MAP(PRFileMap *fmap)
{
    PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
    return PR_FAILURE;
}
