/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** uxshm.c -- Unix Implementations NSPR Named Shared Memory
**
**
** lth. Jul-1999.
**
*/
#include <string.h>
#include <prshm.h>
#include <prerr.h>
#include <prmem.h>
#include "primpl.h"       
#include <fcntl.h>

extern PRLogModuleInfo *_pr_shm_lm;


#define NSPR_IPC_SHM_KEY 'b'
/*
** Implementation for System V
*/
#if defined PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>

#define _MD_OPEN_SHARED_MEMORY _MD_OpenSharedMemory
#define _MD_ATTACH_SHARED_MEMORY _MD_AttachSharedMemory
#define _MD_DETACH_SHARED_MEMORY _MD_DetachSharedMemory
#define _MD_CLOSE_SHARED_MEMORY _MD_CloseSharedMemory
#define _MD_DELETE_SHARED_MEMORY  _MD_DeleteSharedMemory

extern PRSharedMemory * _MD_OpenSharedMemory( 
    const char *name,
    PRSize      size,
    PRIntn      flags,
    PRIntn      mode
)
{
    PRStatus rc = PR_SUCCESS;
    key_t   key;
    PRSharedMemory *shm;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return( NULL );
    }

    shm = PR_NEWZAP( PRSharedMemory );
    if ( NULL == shm ) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New PRSharedMemory out of memory")); 
        return( NULL );
    }

    shm->ipcname = (char*)PR_MALLOC( strlen( ipcname ) + 1 );
    if ( NULL == shm->ipcname )
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New shm->ipcname out of memory")); 
        PR_DELETE( shm );
        return( NULL );
    }

    /* copy args to struct */
    strcpy( shm->ipcname, ipcname );
    shm->size = size;
    shm->mode = mode;
    shm->flags = flags;
    shm->ident = _PR_SHM_IDENT;

    /* create the file first */
    if ( flags & PR_SHM_CREATE )  {
        int osfd = open( shm->ipcname, (O_RDWR | O_CREAT), shm->mode );
        if ( -1 == osfd ) {
            _PR_MD_MAP_OPEN_ERROR( errno );
            PR_FREEIF( shm->ipcname );
            PR_DELETE( shm );
            return( NULL );
        } 
        if ( close(osfd) == -1 ) {
            _PR_MD_MAP_CLOSE_ERROR( errno );
            PR_FREEIF( shm->ipcname );
            PR_DELETE( shm );
            return( NULL );
        }
    }

    /* hash the shm.name to an ID */
    key = ftok( shm->ipcname, NSPR_IPC_SHM_KEY );
    if ( -1 == key )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): ftok() failed on name: %s", shm->ipcname));
        PR_FREEIF( shm->ipcname );
        PR_DELETE( shm );
        return( NULL );
    }

    /* get the shared memory */
    if ( flags & PR_SHM_CREATE )  {
        shm->id = shmget( key, shm->size, ( shm->mode | IPC_CREAT|IPC_EXCL));
        if ( shm->id >= 0 ) {
            return( shm );
        }
        if ((errno == EEXIST) && (flags & PR_SHM_EXCL)) {
            PR_SetError( PR_FILE_EXISTS_ERROR, errno );
            PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
                ("_MD_OpenSharedMemory(): shmget() exclusive failed, errno: %d", errno));
            PR_FREEIF(shm->ipcname);
            PR_DELETE(shm);
            return(NULL);
        }
    } 

    shm->id = shmget( key, shm->size, shm->mode );
    if ( -1 == shm->id ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): shmget() failed, errno: %d", errno));
        PR_FREEIF(shm->ipcname);
        PR_DELETE(shm);
        return(NULL);
    }

    return( shm );
} /* end _MD_OpenSharedMemory() */

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    void        *addr;
    PRUint32    aFlags = shm->mode;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    aFlags |= (flags & PR_SHM_READONLY )? SHM_RDONLY : 0;

    addr = shmat( shm->id, NULL, aFlags );
    if ( (void*)-1 == addr )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): shmat() failed on name: %s, OsError: %d", 
                shm->ipcname, PR_GetOSError() ));
        addr = NULL;
    }

    return addr;
}    

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PRStatus rc = PR_SUCCESS;
    PRIntn   urc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = shmdt( addr );
    if ( -1 == urc )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DetachSharedMemory(): shmdt() failed on name: %s", shm->ipcname ));
    }

    return rc;
}    

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    PR_FREEIF(shm->ipcname);
    PR_DELETE(shm);

    return PR_SUCCESS;
}    

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    PRStatus rc = PR_SUCCESS;
    key_t   key;
    int     id;
    PRIntn  urc;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return(PR_FAILURE);
    }

    /* create the file first */ 
    {
        int osfd = open( ipcname, (O_RDWR | O_CREAT), 0666 );
        if ( -1 == osfd ) {
            _PR_MD_MAP_OPEN_ERROR( errno );
            return( PR_FAILURE );
        } 
        if ( close(osfd) == -1 ) {
            _PR_MD_MAP_CLOSE_ERROR( errno );
            return( PR_FAILURE );
        }
    }

    /* hash the shm.name to an ID */
    key = ftok( ipcname, NSPR_IPC_SHM_KEY );
    if ( -1 == key )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): ftok() failed on name: %s", ipcname));
    }

#ifdef SYMBIAN
    /* In Symbian OS the system imposed minimum is 1 byte, instead of ZERO */
    id = shmget( key, 1, 0 );
#else
    id = shmget( key, 0, 0 );
#endif
    if ( -1 == id ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shmget() failed, errno: %d", errno));
        return(PR_FAILURE);
    }

    urc = shmctl( id, IPC_RMID, NULL );
    if ( -1 == urc )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shmctl() failed on name: %s", ipcname ));
        return(PR_FAILURE);
    }

    urc = unlink( ipcname );
    if ( -1 == urc ) {
        _PR_MD_MAP_UNLINK_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): unlink() failed: %s", ipcname ));
        return(PR_FAILURE);
    }

    return rc;
}  /* end _MD_DeleteSharedMemory() */

/*
** Implementation for Posix
*/
#elif defined PR_HAVE_POSIX_NAMED_SHARED_MEMORY
#include <sys/mman.h>

#define _MD_OPEN_SHARED_MEMORY _MD_OpenSharedMemory
#define _MD_ATTACH_SHARED_MEMORY _MD_AttachSharedMemory
#define _MD_DETACH_SHARED_MEMORY _MD_DetachSharedMemory
#define _MD_CLOSE_SHARED_MEMORY _MD_CloseSharedMemory
#define _MD_DELETE_SHARED_MEMORY  _MD_DeleteSharedMemory

struct _MDSharedMemory {
    int     handle;
};

extern PRSharedMemory * _MD_OpenSharedMemory( 
    const char *name,
    PRSize      size,
    PRIntn      flags,
    PRIntn      mode
)
{
    PRStatus    rc = PR_SUCCESS;
    PRInt32     end;
    PRSharedMemory *shm;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return( NULL );
    }

    shm = PR_NEWZAP( PRSharedMemory );
    if ( NULL == shm ) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New PRSharedMemory out of memory")); 
        return( NULL );
    }

    shm->ipcname = PR_MALLOC( strlen( ipcname ) + 1 );
    if ( NULL == shm->ipcname )
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New shm->ipcname out of memory")); 
        return( NULL );
    }

    /* copy args to struct */
    strcpy( shm->ipcname, ipcname );
    shm->size = size; 
    shm->mode = mode;
    shm->flags = flags;
    shm->ident = _PR_SHM_IDENT;

    /*
    ** Create the shared memory
    */
    if ( flags & PR_SHM_CREATE )  {
        int oflag = (O_CREAT | O_RDWR);
        
        if ( flags & PR_SHM_EXCL )
            oflag |= O_EXCL;
        shm->id = shm_open( shm->ipcname, oflag, shm->mode );
    } else {
        shm->id = shm_open( shm->ipcname, O_RDWR, shm->mode );
    }

    if ( -1 == shm->id )  {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): shm_open failed: %s, OSError: %d",
                shm->ipcname, PR_GetOSError())); 
        PR_DELETE( shm->ipcname );
        PR_DELETE( shm );
        return(NULL);
    }

    end = ftruncate( shm->id, shm->size );
    if ( -1 == end ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): ftruncate failed, OSError: %d",
                PR_GetOSError()));
        PR_DELETE( shm->ipcname );
        PR_DELETE( shm );
        return(NULL);
    }

    return(shm);
} /* end _MD_OpenSharedMemory() */

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    void        *addr;
    PRIntn      prot = (PROT_READ | PROT_WRITE);

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    if ( PR_SHM_READONLY == flags)
        prot ^= PROT_WRITE;

    addr = mmap( (void*)0, shm->size, prot, MAP_SHARED, shm->id, 0 );
    if ((void*)-1 == addr )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): mmap failed: %s, errno: %d",
                shm->ipcname, PR_GetOSError()));
        addr = NULL;
    } else {
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): name: %s, attached at: %p", shm->ipcname, addr));
    }
    
    return addr;
}    

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PRStatus    rc = PR_SUCCESS;
    PRIntn      urc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = munmap( addr, shm->size );
    if ( -1 == urc )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DetachSharedMemory(): munmap failed: %s, errno: %d", 
                shm->ipcname, PR_GetOSError()));
    }
    return rc;
}    

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    int urc;
    
    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = close( shm->id );
    if ( -1 == urc ) {
        _PR_MD_MAP_CLOSE_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_CloseSharedMemory(): close() failed, error: %d", PR_GetOSError()));
        return(PR_FAILURE);
    }
    PR_DELETE( shm->ipcname );
    PR_DELETE( shm );
    return PR_SUCCESS;
}    

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    PRStatus    rc = PR_SUCCESS;
    PRUintn     urc;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return rc;
    }

    urc = shm_unlink( ipcname );
    if ( -1 == urc ) {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shm_unlink failed: %s, errno: %d", 
                ipcname, PR_GetOSError()));
    } else {
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): %s, success", ipcname));
    }

    return rc;
} /* end _MD_DeleteSharedMemory() */
#endif



/*
** Unix implementation for anonymous memory (file) mapping
*/
extern PRLogModuleInfo *_pr_shma_lm;

#include <unistd.h>

extern PRFileMap* _md_OpenAnonFileMap( 
    const char *dirName,
    PRSize      size,
    PRFileMapProtect prot
)
{
    PRFileMap   *fm = NULL;
    PRFileDesc  *fd;
    int         osfd;
    PRIntn      urc;
    PRIntn      mode = 0600;
    char        *genName;
    pid_t       pid = getpid(); /* for generating filename */
    PRThread    *tid = PR_GetCurrentThread(); /* for generating filename */
    int         incr; /* for generating filename */
    const int   maxTries = 20; /* maximum # attempts at a unique filename */
    PRInt64     size64; /* 64-bit version of 'size' */

    /*
    ** generate a filename from input and runtime environment
    ** open the file, unlink the file.
    ** make maxTries number of attempts at uniqueness in the filename
    */
    for ( incr = 0; incr < maxTries ; incr++ ) {
#if defined(SYMBIAN)
#define NSPR_AFM_FILENAME "%s\\NSPR-AFM-%d-%p.%d"
#else
#define NSPR_AFM_FILENAME "%s/.NSPR-AFM-%d-%p.%d"
#endif
        genName = PR_smprintf( NSPR_AFM_FILENAME,
            dirName, (int) pid, tid, incr );
        if ( NULL == genName ) {
            PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
                ("_md_OpenAnonFileMap(): PR_snprintf(): failed, generating filename"));
            goto Finished;
        }

        /* create the file */
        osfd = open(genName, (O_CREAT | O_EXCL | O_RDWR), mode);
        if (-1 == osfd) {
          if (EEXIST == errno) {
            PR_smprintf_free(genName);
            continue; /* name exists, try again */
          }
          _PR_MD_MAP_OPEN_ERROR(errno);
          PR_LOG(
            _pr_shma_lm,
            PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): open(): failed, filename: %s, errno: %d",
             genName,
             PR_GetOSError()));
          PR_smprintf_free(genName);
          goto Finished;
        }
        break; /* name generation and open successful, break; */
    } /* end for() */

    if (incr == maxTries) {
      PR_ASSERT(-1 == osfd);
      PR_ASSERT(EEXIST == errno);
      _PR_MD_MAP_OPEN_ERROR(errno);
      goto Finished;
    }

    urc = unlink( genName );
#if defined(SYMBIAN) && defined(__WINS__)
    /* If it is being used by the system or another process, Symbian OS 
     * Emulator(WINS) considers this an error. */
    if ( -1 == urc && EACCES != errno ) {
#else
    if ( -1 == urc ) {
#endif
        _PR_MD_MAP_UNLINK_ERROR( errno );
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): failed on unlink(), errno: %d", errno));
        PR_smprintf_free( genName );
        close( osfd );
        goto Finished;        
    }
    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_OpenAnonFileMap(): unlink(): %s", genName ));

    PR_smprintf_free( genName );

    fd = PR_ImportFile( osfd );
    if ( NULL == fd ) {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): PR_ImportFile(): failed"));
        goto Finished;        
    }
    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_OpenAnonFileMap(): fd: %p", fd ));

    urc = ftruncate( fd->secret->md.osfd, size );
    if ( -1 == urc ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_OpenAnonFileMap(): failed on ftruncate(), errno: %d", errno));
        PR_Close( fd );
        goto Finished;        
    }
    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_OpenAnonFileMap(): ftruncate(): size: %d", size ));

    LL_UI2L(size64, size);  /* PRSize (size_t) is unsigned */
    fm = PR_CreateFileMap( fd, size64, prot );
    if ( NULL == fm )  {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("PR_OpenAnonFileMap(): failed"));
        PR_Close( fd );
        goto Finished;        
    }
    fm->md.isAnonFM = PR_TRUE; /* set fd close */

    PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
        ("_md_OpenAnonFileMap(): PR_CreateFileMap(): fm: %p", fm ));

Finished:    
    return(fm);
} /* end md_OpenAnonFileMap() */

/*
** _md_ExportFileMapAsString()
**
**
*/
extern PRStatus _md_ExportFileMapAsString(
    PRFileMap *fm,
    PRSize    bufSize,
    char      *buf
)
{
    PRIntn  written;
    PRIntn  prot = (PRIntn)fm->prot;
    
    written = PR_snprintf( buf, bufSize, "%ld:%d",
        fm->fd->secret->md.osfd, prot );
        
    return((written == -1)? PR_FAILURE : PR_SUCCESS);
} /* end _md_ExportFileMapAsString() */


extern PRFileMap * _md_ImportFileMapFromString(
    const char *fmstring
)
{
    PRStatus    rc;
    PRInt32     osfd;
    PRIntn      prot; /* really: a PRFileMapProtect */
    PRFileDesc  *fd;
    PRFileMap   *fm = NULL; /* default return value */
    PRFileInfo64 info;

    PR_sscanf( fmstring, "%ld:%d", &osfd, &prot );

    /* import the os file descriptor */
    fd = PR_ImportFile( osfd );
    if ( NULL == fd ) {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_ImportFileMapFromString(): PR_ImportFile() failed"));
        goto Finished;
    }

    rc = PR_GetOpenFileInfo64( fd, &info );
    if ( PR_FAILURE == rc )  {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_ImportFileMapFromString(): PR_GetOpenFileInfo64() failed"));    
        goto Finished;
    }

    fm = PR_CreateFileMap( fd, info.size, (PRFileMapProtect)prot );
    if ( NULL == fm ) {
        PR_LOG( _pr_shma_lm, PR_LOG_DEBUG,
            ("_md_ImportFileMapFromString(): PR_CreateFileMap() failed"));    
    }

Finished:
    return(fm);
} /* end _md_ImportFileMapFromString() */
