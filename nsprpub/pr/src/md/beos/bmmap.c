/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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
