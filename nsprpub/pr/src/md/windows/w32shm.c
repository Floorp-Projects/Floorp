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

    shm->ipcname = PR_MALLOC( strlen( ipcname ) + 1 );
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
        /* XXX: Not 64bit safe. Fix when WinNT goes 64bit. */
        dwHi = 0;
        dwLo = shm->size;

        if (_PR_NT_MakeSecurityDescriptorACL(mode, filemapAccessTable,
                &pSD, &pACL) == PR_SUCCESS) {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = pSD;
            sa.bInheritHandle = FALSE;
            lpSA = &sa;
        }
        shm->handle = CreateFileMapping(
            (HANDLE)-1 ,
            lpSA,
            flProtect,
            dwHi,
            dwLo,
            shm->ipcname);
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
        shm->handle = OpenFileMapping( FILE_MAP_WRITE, TRUE, shm->ipcname );
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

    written = PR_snprintf( buf, bufSize, "%d:%ld:%ld",
        (PRIntn)fm->prot, (PRInt32)fm->md.hFileMap, (PRInt32)fm->md.dwAccess );
    /* Watch out on the above snprintf(). Windows HANDLE assumes 32bits; windows calls it void* */

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
    PRInt32 hFileMap;
    PRInt32 dwAccess;
    PRFileMap *fm = NULL;

    PR_sscanf( fmstring, "%d:%ld:%ld", &prot, &hFileMap, &dwAccess  );

    fm = PR_NEWZAP(PRFileMap);
    if ( NULL == fm ) {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_ImportFileMapFromString(): PR_NEWZAP(): Failed"));
        return(fm);
    }

    fm->prot = (PRFileMapProtect)prot;
    fm->md.hFileMap = (HANDLE)hFileMap;  /* Assumes HANDLE is 32bit */
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
