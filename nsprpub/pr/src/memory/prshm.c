/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** prshm.c -- NSPR Named Shared Memory
**
** lth. Jul-1999.
*/
#include <string.h>
#include "primpl.h"

extern PRLogModuleInfo *_pr_shm_lm;


#if defined PR_HAVE_SYSV_NAMED_SHARED_MEMORY
/* SysV implementation is in pr/src/md/unix/uxshm.c */
#elif defined PR_HAVE_POSIX_NAMED_SHARED_MEMORY
/* Posix implementation is in pr/src/md/unix/uxshm.c */
#elif defined PR_HAVE_WIN32_NAMED_SHARED_MEMORY
/* Win32 implementation is in pr/src/md/windows/w32shm.c */
#else 
/* 
**  there is no named_shared_memory 
*/
extern PRSharedMemory*  _MD_OpenSharedMemory( const char *name, PRSize size, PRIntn flags, PRIntn mode )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}    

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}    

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}    

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}    

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}    
#endif /* HAVE_SYSV_NAMED_SHARED_MEMORY */

/*
** FUNCTION: PR_OpenSharedMemory()
**
*/
PR_IMPLEMENT( PRSharedMemory * )
    PR_OpenSharedMemory(
        const char *name,
        PRSize      size,
        PRIntn      flags,
        PRIntn      mode
)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return( _PR_MD_OPEN_SHARED_MEMORY( name, size, flags, mode ));
} /* end PR_OpenSharedMemory() */

/*
** FUNCTION: PR_AttachSharedMemory()
**
*/
PR_IMPLEMENT( void * )
    PR_AttachSharedMemory(
        PRSharedMemory *shm,
        PRIntn          flags
)
{
    return( _PR_MD_ATTACH_SHARED_MEMORY( shm, flags ));
} /* end PR_AttachSharedMemory() */

/*
** FUNCTION: PR_DetachSharedMemory()
**
*/
PR_IMPLEMENT( PRStatus )
    PR_DetachSharedMemory(
        PRSharedMemory *shm,
        void *addr
)
{
    return( _PR_MD_DETACH_SHARED_MEMORY( shm, addr ));
} /* end PR_DetachSharedMemory() */

/*
** FUNCTION: PR_CloseSharedMemory()
**
*/
PR_IMPLEMENT( PRStatus )
    PR_CloseSharedMemory(
        PRSharedMemory *shm
)
{
    return( _PR_MD_CLOSE_SHARED_MEMORY( shm ));
} /* end PR_CloseSharedMemory() */

/*
** FUNCTION: PR_DeleteSharedMemory()
**
*/
PR_EXTERN( PRStatus )
    PR_DeleteSharedMemory(
        const char *name
)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return(_PR_MD_DELETE_SHARED_MEMORY( name ));
} /* end PR_DestroySharedMemory() */
/* end prshm.c */
