/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
