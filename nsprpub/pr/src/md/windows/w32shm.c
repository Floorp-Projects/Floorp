/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <private/primpl.h>       
#include <string.h>
#include <prshm.h>
#include <prerr.h>
#include <prmem.h>

#if defined(PR_HAVE_WIN32_NAMED_SHARED_MEMORY)

extern PRLogModuleInfo *_pr_shm_lm;

/*
 * NSPR-to-NT access right mapping table for file-mapping objects.
 *
 * The OR of these three access masks must equal FILE_MAP_ALL_ACCESS.
 * This is because if a file-mapping object with the specified name
 * exists, CreateFileMapping requests full access to the existing
 * object.
 */
static DWORD filemapAccessTable[] = {
    FILE_MAP_ALL_ACCESS & ~FILE_MAP_WRITE, /* read */
    FILE_MAP_ALL_ACCESS & ~FILE_MAP_READ, /* write */ 
    0  /* execute */
};

extern PRSharedMemory * _MD_OpenSharedMemory( 
        const char *name,
        PRSize      size,
        PRIntn      flags,
        PRIntn      mode
)
{
    char        ipcname[PR_IPC_NAME_SIZE];
    PRStatus    rc = PR_SUCCESS;
    DWORD dwHi, dwLo;
    PRSharedMemory *shm;
    DWORD flProtect = ( PAGE_READWRITE );
    SECURITY_ATTRIBUTES sa;
    LPSECURITY_ATTRIBUTES lpSA = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pACL = NULL;

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError(PR_UNKNOWN_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: name is invalid")); 
        return(NULL);
    }

    shm = PR_NEWZAP( PRSharedMemory );
    if ( NULL == shm ) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New PRSharedMemory out of memory")); 
        return(NULL);
    }

    shm->ipcname = PR_MALLOC( (PRUint32) (strlen( ipcname ) + 1) );
    if ( NULL == shm->ipcname )
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New shm->ipcname out of memory")); 
        PR_DELETE(shm);
        return(NULL);
    }

    /* copy args to struct */
    strcpy( shm->ipcname, ipcname );
    shm->size = size; 
    shm->mode = mode;
    shm->flags = flags;
    shm->ident = _PR_SHM_IDENT;

    if (flags & PR_SHM_CREATE ) {
        dwHi = (DWORD) (((PRUint64) shm->size >> 32) & 0xffffffff);
        dwLo = (DWORD) (shm->size & 0xffffffff);

        if (_PR_NT_MakeSecurityDescriptorACL(mode, filemapAccessTable,
                &pSD, &pACL) == PR_SUCCESS) {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = pSD;
            sa.bInheritHandle = FALSE;
            lpSA = &sa;
        }
#ifdef WINCE
        {
            /*
             * This is assuming that the name will never be larger than
             * MAX_PATH.  Should we dynamically allocate?
             */
            PRUnichar wideIpcName[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, shm->ipcname, -1,
                                wideIpcName, MAX_PATH);
            shm->handle = CreateFileMappingW(
                (HANDLE)-1 ,
                lpSA,
                flProtect,
                dwHi,
                dwLo,
                wideIpcName);
        }
#else
        shm->handle = CreateFileMappingA(
            (HANDLE)-1 ,
            lpSA,
            flProtect,
            dwHi,
            dwLo,
            shm->ipcname);
#endif
        if (lpSA != NULL) {
            _PR_NT_FreeSecurityDescriptorACL(pSD, pACL);
        }

        if ( NULL == shm->handle ) {
            PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
                ( "PR_OpenSharedMemory: CreateFileMapping() failed: %s",
                    shm->ipcname )); 
            _PR_MD_MAP_DEFAULT_ERROR( GetLastError());
            PR_FREEIF( shm->ipcname )
            PR_DELETE( shm );
            return(NULL);
        } else {
            if (( flags & PR_SHM_EXCL) && ( GetLastError() == ERROR_ALREADY_EXISTS ))  {
                PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
                    ( "PR_OpenSharedMemory: Request exclusive & already exists",
                        shm->ipcname )); 
                PR_SetError( PR_FILE_EXISTS_ERROR, ERROR_ALREADY_EXISTS );
                CloseHandle( shm->handle );
                PR_FREEIF( shm->ipcname )
                PR_DELETE( shm );
                return(NULL);
            } else {
                PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
                    ( "PR_OpenSharedMemory: CreateFileMapping() success: %s, handle: %d",
                        shm->ipcname, shm->handle ));
                return(shm);
            }
        }
    } else {
#ifdef WINCE
        PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
        shm->handle = NULL;  /* OpenFileMapping not supported */
#else
        shm->handle = OpenFileMapping( FILE_MAP_WRITE, TRUE, shm->ipcname );
#endif
        if ( NULL == shm->handle ) {
            _PR_MD_MAP_DEFAULT_ERROR( GetLastError());
            PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
                ( "PR_OpenSharedMemory: OpenFileMapping() failed: %s, error: %d",
                    shm->ipcname, PR_GetOSError())); 
            PR_FREEIF( shm->ipcname );
            PR_DELETE( shm );
            return(NULL);
        } else {
            PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
                ( "PR_OpenSharedMemory: OpenFileMapping() success: %s, handle: %d",
                    shm->ipcname, shm->handle )); 
                return(shm);
        }
    }
    /* returns from separate paths */
}

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    PRUint32    access = FILE_MAP_WRITE;
    void        *addr;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    if ( PR_SHM_READONLY & flags )
        access = FILE_MAP_READ;

    addr = MapViewOfFile( shm->handle,
        access,
        0, 0,
        shm->size );

    if ( NULL == addr ) {
        _PR_MD_MAP_DEFAULT_ERROR( GetLastError());
        PR_LOG( _pr_shm_lm, PR_LOG_ERROR, 
            ("_MD_AttachSharedMemory: MapViewOfFile() failed. OSerror: %d", PR_GetOSError()));
    }

    return( addr );
} /* end _MD_ATTACH_SHARED_MEMORY() */


extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PRStatus rc = PR_SUCCESS;
    BOOL        wrc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    wrc = UnmapViewOfFile( addr );
    if ( FALSE == wrc ) 
    {
        _PR_MD_MAP_DEFAULT_ERROR( GetLastError());
        PR_LOG( _pr_shm_lm, PR_LOG_ERROR, 
            ("_MD_DetachSharedMemory: UnmapViewOfFile() failed. OSerror: %d", PR_GetOSError()));
        rc = PR_FAILURE;
    }

    return( rc );
}


extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    PRStatus rc = PR_SUCCESS;
    BOOL wrc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    wrc = CloseHandle( shm->handle );
    if ( FALSE == wrc )
    {
        _PR_MD_MAP_DEFAULT_ERROR( GetLastError());
        PR_LOG( _pr_shm_lm, PR_LOG_ERROR, 
            ("_MD_CloseSharedMemory: CloseHandle() failed. OSerror: %d", PR_GetOSError()));
        rc = PR_FAILURE;
    }
    PR_FREEIF( shm->ipcname );
    PR_DELETE( shm );

    return( rc );
} /* end _MD_CLOSE_SHARED_MEMORY() */

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    return( PR_SUCCESS );
}    


/*
** Windows implementation of anonymous memory (file) map
*/
extern PRLogModuleInfo *_pr_shma_lm;

extern PRFileMap* _md_OpenAnonFileMap( 
    const char *dirName,
    PRSize      size,
    PRFileMapProtect prot
)
{
    PRFileMap   *fm;
    HANDLE      hFileMap;

    fm = PR_CreateFileMap( (PRFileDesc*)-1, size, prot );
    if ( NULL == fm )  {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): PR_CreateFileMap(): failed"));
        goto Finished;
    }

    /*
    ** Make fm->md.hFileMap inheritable. We can't use
    ** GetHandleInformation and SetHandleInformation
    ** because these two functions fail with
    ** ERROR_CALL_NOT_IMPLEMENTED on Win95.
    */
    if (DuplicateHandle(GetCurrentProcess(), fm->md.hFileMap,
            GetCurrentProcess(), &hFileMap,
            0, TRUE /* inheritable */,
            DUPLICATE_SAME_ACCESS) == FALSE) {
        PR_SetError( PR_UNKNOWN_ERROR, GetLastError() );
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): DuplicateHandle(): failed"));
        PR_CloseFileMap( fm );
        fm = NULL;
        goto Finished;
    }
    CloseHandle(fm->md.hFileMap);
    fm->md.hFileMap = hFileMap;

Finished:    
    return(fm);
} /* end md_OpenAnonFileMap() */

/*
** _md_ExportFileMapAsString()
**
*/
extern PRStatus _md_ExportFileMapAsString(
    PRFileMap *fm,
    PRSize    bufSize,
    char      *buf
)
{
    PRIntn  written;

    written = PR_snprintf( buf, (PRUint32) bufSize, "%d:%" PR_PRIdOSFD ":%ld",
        (PRIntn)fm->prot, (PROsfd)fm->md.hFileMap, (PRInt32)fm->md.dwAccess );

    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_ExportFileMapAsString(): prot: %x, hFileMap: %x, dwAccess: %x",
            fm->prot, fm->md.hFileMap, fm->md.dwAccess ));
        
    return((written == -1)? PR_FAILURE : PR_SUCCESS);
} /* end _md_ExportFileMapAsString() */


/*
** _md_ImportFileMapFromString()
**
*/
extern PRFileMap * _md_ImportFileMapFromString(
    const char *fmstring
)
{
    PRIntn  prot;
    PROsfd hFileMap;
    PRInt32 dwAccess;
    PRFileMap *fm = NULL;

    PR_sscanf( fmstring, "%d:%" PR_SCNdOSFD ":%ld",
        &prot, &hFileMap, &dwAccess );

    fm = PR_NEWZAP(PRFileMap);
    if ( NULL == fm ) {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_ImportFileMapFromString(): PR_NEWZAP(): Failed"));
        return(fm);
    }

    fm->prot = (PRFileMapProtect)prot;
    fm->md.hFileMap = (HANDLE)hFileMap;
    fm->md.dwAccess = (DWORD)dwAccess;
    fm->fd = (PRFileDesc*)-1;

    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_ImportFileMapFromString(): fm: %p, prot: %d, hFileMap: %8.8x, dwAccess: %8.8x, fd: %x",
            fm, prot, fm->md.hFileMap, fm->md.dwAccess, fm->fd));
    return(fm);
} /* end _md_ImportFileMapFromString() */

#else
Error! Why is PR_HAVE_WIN32_NAMED_SHARED_MEMORY not defined? 
#endif /* PR_HAVE_WIN32_NAMED_SHARED_MEMORY */
/* --- end w32shm.c --- */
