/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* ====================================================================
 * reg.c
 * XP Registry functions
 * ====================================================================
 */

/* TODO:
 *	- Replace 'malloc' in NR_RegPack with the Netscape XP equivalent
 *	- Solve DOS 'errno' problem mentioned below
 *	- Solve rename across volume problem described in VR_PackRegistry
 */

/* Preprocessor Defines
 *  STANDALONE_REGISTRY - define if not linking with Navigator
 *  NOCACHE_HDR			- define if multi-threaded access to registry
 *  SELF_REPAIR 		- define to skip header update on open
 *  VERIFY_READ         - define TRUE to double-check short reads
 */
#define NOCACHE_HDR     1
#define SELF_REPAIR     1
#ifdef DEBUG
#define VERIFY_READ     1
#endif

#ifdef STANDALONE_REGISTRY
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(XP_MAC)
  #include <Errors.h>
#endif

#else

#include "prerror.h"

#endif /*STANDALONE_REGISTRY*/

#if defined(SUNOS4)
#include <unistd.h>  /* for SEEK_SET */
#endif /* SUNOS4 */

#include "reg.h"
#include "NSReg.h"

#if !defined(STANDALONE_REGISTRY) && defined(SEEK_SET)
    /* Undo the damage caused by xp_core.h, which is included by NSReg.h */
    #undef SEEK_SET
    #undef SEEK_CUR
    #undef SEEK_END
    #define SEEK_SET PR_SEEK_SET
    #define SEEK_CUR PR_SEEK_CUR
    #define SEEK_END PR_SEEK_END
#endif

#if defined(XP_UNIX)
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#elif defined(WIN32)
#include <windef.h>  /* for MAX_PATH */
#elif defined(XP_MAC)
#define MAX_PATH 512
#endif


/* NOTE! It is EXREMELY important that node names be in UTF-8; otherwise
 * backwards path search for delim char will fail for multi-byte/Unicode names
 */

/* ====================================================================
 * Overview
 * --------------------------------------------------------------------
 *
 *  Layers:
 *      Interface
 *          Path Parsing
 *              Key/Entry Management
 *                  Block I/O
 *                      Virtual I/O
 *
 * The functions in this file search and add to a binary Registry file
 * quite efficiently.  So efficiently, squeezing out space left by
 * deleted and updated objects requires a separate "pack" operation.
 *
 * Terms:
 * As used here, a 'key' is a node in the tree. The root of the tree
 * exists in an otherwise empty Registry as is itself a key.  Every key
 * has 0 or more sub-keys. Every key also has 0 or more 'entry's. Both
 * entries and keys have names. Entries also have values associated.
 * Names and values are simply strings of characters. These strings
 * may be quoted so that they can include path delimiter and equals
 * sign characters which are otherwise reserved.
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * Module Global Data
 *
 * use of this data must be protected by the reglist lock
 * --------------------------------------------------------------------
 */

#if !defined(STANDALONE_REGISTRY)
static PRLock   *reglist_lock = NULL;
#endif

static REGFILE  *RegList = NULL;
static int32    regStartCount = 0;
char            *globalRegName = NULL;
static char     *user_name = NULL;




#ifdef XP_MAC

void nr_MacAliasFromPath(const char * fileName, void ** alias, int32 * length);
char * nr_PathFromMacAlias(const void * alias, uint32 aliasLength);

#include <Aliases.h>
#include <TextUtils.h>
#include <Memory.h>
#include <Folders.h>
#include "FullPath.h"

/* returns an alias as a malloc'd pointer.
 * On failure, *alias is NULL
 */
void nr_MacAliasFromPath(const char * fileName, void ** alias, int32 * length)
{
	OSErr err;
	FSSpec fs;
	AliasHandle macAlias;
	*alias = NULL;
	*length = 0;
	c2pstr((char*)fileName);
	err = FSMakeFSSpec(0, 0, (unsigned char *)fileName, &fs);
	p2cstr((unsigned char *)fileName);
	if ( err != noErr )
		return;
	err = NewAlias(NULL, &fs, &macAlias);
	if ( (err != noErr) || ( macAlias == NULL ))
		return;
	*length = GetHandleSize( (Handle) macAlias );
	*alias = XP_ALLOC( *length );
	if ( *alias == NULL )
	{
		DisposeHandle((Handle)macAlias);
		return;
	}
	HLock( (Handle) macAlias );
	XP_MEMCPY(*alias, *macAlias , *length);
	HUnlock( (Handle) macAlias );
	DisposeHandle( (Handle) macAlias);
	return;
}

/* resolves an alias, and returns a full path to the Mac file
 * If the alias changed, it would be nice to update our alias pointers
 */
char * nr_PathFromMacAlias(const void * alias, uint32 aliasLength)
{
	OSErr 			err;
	short 			vRefNum;
	long 			dirID;
	AliasHandle 	h 			= NULL;
	Handle 			fullPath 	= NULL;
	short 			fullPathLength;
	char * 			cpath 		= NULL;
	FSSpec 			fs;
	Boolean 		ignore;	/* Change flag, it would be nice to change the alias on disk 
						if the file location changed */
	
	
	XP_MEMSET( &fs, '\0', sizeof(FSSpec) );
	
	
	/* Copy the alias to a handle and resolve it */
	h = (AliasHandle) NewHandle(aliasLength);
	if ( h == NULL)
		goto fail;
		
		
	HLock( (Handle) h);
	XP_MEMCPY( *h, alias, aliasLength );
	HUnlock( (Handle) h);
	
	
	err = ResolveAlias(NULL, h, &fs, &ignore);
	if (err != noErr)
		goto fail;
	
	/* if the file is inside the trash, assume that user has deleted
	 	it and that we do not want to look at it */
	err = FindFolder(fs.vRefNum, kTrashFolderType, false, &vRefNum, &dirID);
	if (err == noErr)
		if (dirID == fs.parID)	/* File is inside the trash */
			goto fail;
	
	/* Get the full path and create a char * out of it */

	err = GetFullPath(fs.vRefNum, fs.parID,fs.name, &fullPathLength, &fullPath);
	if ( (err != noErr) || (fullPath == NULL) )
		goto fail;
	
	cpath = (char*) XP_ALLOC(fullPathLength + 1);
	if ( cpath == NULL)
		goto fail;
	
	HLock( fullPath );
	XP_MEMCPY(cpath, *fullPath, fullPathLength);
	cpath[fullPathLength] = 0;
	HUnlock( fullPath );
	
	/* Drop through */
fail:
	if (h != NULL)
		DisposeHandle( (Handle) h);
	if (fullPath != NULL)
		DisposeHandle( fullPath);
	return cpath;
}

#endif


/* --------------------------------------------------------------------
 * Registry List management
 * --------------------------------------------------------------------
 */
static void nr_AddNode(REGFILE* pReg);
static void nr_DeleteNode(REGFILE *pReg);
static REGFILE* vr_findRegFile(char *filename);

/* -------------------------------------------------------------------- */

static void nr_AddNode(REGFILE* pReg)
{
    /* add node to head of list */
    pReg->next = RegList;
    pReg->prev = NULL;

    RegList = pReg;

    if ( pReg->next != NULL ) {
        pReg->next->prev = pReg;
    }
}

static void nr_DeleteNode(REGFILE* pReg)
{
    /* if at head of list... */
    if ( pReg->prev == NULL ) {
        RegList = pReg->next;
    }
    else {
        pReg->prev->next = pReg->next;
    }

    if ( pReg->next != NULL ) {
        pReg->next->prev = pReg->prev;
    }

    /* free memory */
#ifndef STANDALONE_REGISTRY
    if ( pReg->lock != NULL )
        PR_DestroyLock( pReg->lock );
#endif
    XP_FREEIF( pReg->filename );
    XP_FREE( pReg );
}

static REGFILE* vr_findRegFile(char *filename)
{
    REGFILE *pReg;

    pReg = RegList;
    while( pReg != NULL ) {
#ifdef XP_UNIX
        if ( 0 == XP_STRCMP( filename, pReg->filename ) ) {
#else
        if ( 0 == XP_STRCASECMP( filename, pReg->filename ) ) {
#endif
            break;
        }
        pReg = pReg->next;
    }

    return pReg;
}


/* --------------------------------------------------------------------
 * Virtual I/O
 *  Platform-specifics go in this section
 * --------------------------------------------------------------------
 */
static REGERR nr_OpenFile(char *path, FILEHANDLE *fh);
static REGERR nr_CloseFile(FILEHANDLE *fh);	/* Note: fh is a pointer */
static REGERR nr_ReadFile(FILEHANDLE fh, REGOFF offset, int32 len, void *buffer);
static REGERR nr_WriteFile(FILEHANDLE fh, REGOFF offset, int32 len, void *buffer);
static REGERR nr_LockRange(FILEHANDLE fh, REGOFF offset, int32 len);
static REGERR nr_UnlockRange(FILEHANDLE fh, REGOFF offset, int32 len);
static int32  nr_GetFileLength(FILEHANDLE fh);
/* -------------------------------------------------------------------- */

#ifdef STANDALONE_REGISTRY
static REGERR nr_OpenFile(char *path, FILEHANDLE *fh)
{
    XP_ASSERT( path != NULL );
    XP_ASSERT( fh != NULL );

    /* Open the file for exclusive random read/write */
    (*fh) = vr_fileOpen(path, XP_FILE_UPDATE_BIN);
    if ( !VALID_FILEHANDLE(*fh) )
    {
        switch (errno)
        {
#ifdef XP_MAC
        case fnfErr:
#else
        case ENOENT:	/* file not found */
#endif
            return REGERR_NOFILE;

#ifdef XP_MAC
        case opWrErr:
#else
        case EACCES:	/* file in use */
#endif
            /* DVNOTE: should we try read only? */
            (*fh) = vr_fileOpen(path, XP_FILE_READ_BIN);
            if ( VALID_FILEHANDLE(*fh) )
                return REGERR_READONLY;
            else
                return REGERR_FAIL;
#ifdef EMFILE   /* Mac Does not have EMFILE  */               
        case EMFILE:	/* too many files open */
#endif
        default:
            return REGERR_FAIL;
        }
    }

    return REGERR_OK;

}   /* OpenFile */
#else
static REGERR nr_OpenFile(char *path, FILEHANDLE *fh)
{
	PR_ASSERT( path != NULL );
    PR_ASSERT( fh != NULL );

	/* Open the file for exclusive random read/write */
	(*fh) = PR_Open(path, PR_RDWR|PR_CREATE_FILE, 00700);
	if ( !VALID_FILEHANDLE(*fh) )
	{
		switch (PR_GetError())
		{
		case PR_FILE_NOT_FOUND_ERROR:	/* file not found */
			return REGERR_NOFILE;

		case PR_FILE_IS_BUSY_ERROR:	/* file in use */
		case PR_FILE_IS_LOCKED_ERROR:
		case PR_ILLEGAL_ACCESS_ERROR:
        case PR_NO_ACCESS_RIGHTS_ERROR:
            /* DVNOTE: should we try read only? */
			(*fh) = PR_Open(path, PR_RDONLY, 00700);
	        if ( VALID_FILEHANDLE(*fh) )
                return REGERR_READONLY;
            else
                return REGERR_FAIL;
		default:
			return REGERR_FAIL;
		}
	}

	return REGERR_OK;

}	/* OpenFile */
#endif


static REGERR nr_CloseFile(FILEHANDLE *fh)
{
	/* NOTE: 'fh' is a pointer, unlike other Close functions
	 *		 This is necessary so that nr_CloseFile can set it to NULL
	 */

    XP_ASSERT( fh != NULL );
	if ( VALID_FILEHANDLE(*fh) )
		XP_FileClose(*fh);
	(*fh) = NULL;
	return REGERR_OK;

}	/* CloseFile */



static REGERR nr_ReadFile(FILEHANDLE fh, REGOFF offset, int32 len, void *buffer)
{
#if VERIFY_READ
    #define        FILLCHAR  0xCC
    unsigned char* p;
    unsigned char* dbgend = (unsigned char*)buffer+len;
#endif

    int32 readlen;
	REGERR err = REGERR_OK;

	XP_ASSERT(len > 0);
	XP_ASSERT(buffer != NULL);
	XP_ASSERT(fh != NULL);

#if VERIFY_READ
    memset(buffer, FILLCHAR, len);
#endif

	if (XP_FileSeek(fh, offset, SEEK_SET) != 0 ) {
        err = REGERR_FAIL;
    }
    else {
        readlen = XP_FileRead(buffer, len, fh );
        /* PR_READ() returns an unreliable length, check EOF separately */
	    if (readlen < 0) {
#if !defined(STANDALONE_REGISTRY) || !defined(XP_MAC)
	#if defined(STANDALONE_REGISTRY)
    		if (errno == EBADF)	/* bad file handle, not open for read, etc. */
	#else
   			if (PR_GetError() == PR_BAD_DESCRIPTOR_ERROR)
	#endif
			    err = REGERR_FAIL;
		    else
#endif
    			err = REGERR_BADREAD;
    	}
        else if (readlen < len) {
#if VERIFY_READ
            /* PR_READ() says we hit EOF but return length is unreliable. */
            /* If buffer has new data beyond what PR_READ() says it got */
            /* we'll assume the read was OK--this is a gamble but */
            /* missing errors will cause fewer problems than too many. */
            p = (unsigned char*)buffer+readlen;
            while ( (*p == (unsigned char)FILLCHAR) && (p < dbgend) ) {
                p++;
            }

            /* really was EOF if it's all FILLCHAR's */
            if ( p == dbgend ) {
                err = REGERR_BADREAD;
            }
#else
            err = REGERR_BADREAD;
#endif
        }
    }

    return err;

}   /* ReadFile */



static REGERR nr_WriteFile(FILEHANDLE fh, REGOFF offset, int32 len, void *buffer)
{

    /* Note: 'offset' will commonly be the end of the file, in which
     * case this function extends the file to 'offset'+'len'. This may
     * be a two-step operation on some platforms.
     */
    XP_ASSERT(len > 0);
    XP_ASSERT(buffer);
    XP_ASSERT(fh != NULL);

    if (XP_FileSeek(fh, offset, SEEK_SET) != 0)
        return REGERR_FAIL;

    if ((int32)XP_FileWrite(buffer, len, fh) != len)
    {
        /* disk full or some other catastrophic error */
        return REGERR_FAIL;
    }

    return REGERR_OK;

}   /* WriteFile */



static REGERR nr_LockRange(FILEHANDLE fh, REGOFF offset, int32 len)
{
    /* TODO: Implement XP lock function with built-in retry. */

    return REGERR_OK;

}   /* LockRange */



static REGERR nr_UnlockRange(FILEHANDLE fh, REGOFF offset, int32 len)
{
    /* TODO: Implement XP unlock function with built-in retry. */

    XP_FileFlush( fh );
    return REGERR_OK;

}   /* UnlockRange */



#if SELF_REPAIR
static int32 nr_GetFileLength(FILEHANDLE fh)
{
    int32 length;
    int32 curpos;

    curpos = XP_FileTell(fh);
    XP_FileSeek(fh, 0, SEEK_END);
    length = XP_FileTell(fh);
    XP_FileSeek(fh, curpos, SEEK_SET);
    return length;

}   /* GetFileLength */
#endif



/* --------------------------------------------------------------------
 * Numeric converters
 * --------------------------------------------------------------------
 * The converters read and write integers in a common format so we
 * can transport registries without worrying about endian problems.
 *
 * The buffers *MUST* be the appropriate size!
 * --------------------------------------------------------------------
 */
static uint32 nr_ReadLong(char *buffer);
static uint16 nr_ReadShort(char *buffer);
static void   nr_WriteLong(uint32 num, char *buffer);
static void   nr_WriteShort(uint16 num, char *buffer);
/* -------------------------------------------------------------------- */



static uint16 nr_ReadShort(char *buffer)
{
    uint16 val;
    uint8 *p = (uint8*)buffer;
 
    val = *p + (uint16)( *(p+1) * 0x100 );

    return val;
}



static uint32 nr_ReadLong(char *buffer)
{
    uint32 val;
    uint8 *p = (uint8*)buffer;

    val = *p
        + (uint32)(*(p+1) * 0x100L)
        + (uint32)(*(p+2) * 0x10000L )
        + (uint32)(*(p+3) * 0x1000000L );

    return val;
}



static void  nr_WriteLong(uint32 num, char *buffer)
{
    uint8 *p = (uint8*)buffer;
    *p++ = (uint8)(num & 0x000000FF);
    num /= 0x100;
    *p++ = (uint8)(num & 0x000000FF);
    num /= 0x100;
    *p++ = (uint8)(num & 0x000000FF);
    num /= 0x100;
    *p   = (uint8)(num & 0x000000FF);
}



static void  nr_WriteShort(uint16 num, char *buffer)
{
    uint8 *p = (uint8*)buffer;

    *p = (uint8)(num & 0x00FF);
    *(p+1) = (uint8)(num / 0x100);
}



/* --------------------------------------------------------------------
 * Block I/O
 * --------------------------------------------------------------------
 */
static REGERR nr_ReadHdr(REGFILE *reg);	/* Reads the file header, creates file if empty */
static REGERR nr_WriteHdr(REGFILE *reg);	/* Writes the file header */
static REGERR nr_CreateRoot(REGFILE *reg);

static REGERR nr_Lock(REGFILE *reg);
static REGERR nr_Unlock(REGFILE *reg);

static REGERR nr_ReadDesc(REGFILE *reg, REGOFF offset, REGDESC *desc);		/* reads a desc */
static REGERR nr_ReadName(REGFILE *reg, REGDESC *desc, uint32 buflen, char *buf);
static REGERR nr_ReadData(REGFILE *reg, REGDESC *desc, uint32 buflen, char *buf);

static REGERR nr_WriteDesc(REGFILE *reg, REGDESC *desc);					/* writes a desc */
static REGERR nr_WriteString(REGFILE *reg, char *string, REGDESC *desc);	/* writes a string */
static REGERR nr_WriteData(REGFILE *reg, char *string, uint32 len, REGDESC *desc);	/* writes a string */

static REGERR nr_AppendDesc(REGFILE *reg, REGDESC *desc, REGOFF *result);	/* adds a desc */
static REGERR nr_AppendName(REGFILE *reg, char *name, REGDESC *desc);	    /* adds a name */
static REGERR nr_AppendString(REGFILE *reg, char *string, REGDESC *desc);	/* adds a string */
static REGERR nr_AppendData(REGFILE *reg, char *string, uint32 len, REGDESC *desc);	/* adds a string */

static XP_Bool nr_IsValidUTF8(char *string);	/* checks if a string is UTF-8 encoded */
/* -------------------------------------------------------------------- */



static REGERR nr_ReadHdr(REGFILE *reg)
{

	int err;
	long filelength;
    char hdrBuf[sizeof(REGHDR)];

	XP_ASSERT(reg);
	reg->hdrDirty = 0;

	err = nr_ReadFile(reg->fh, 0, sizeof(REGHDR), &hdrBuf);

	switch (err)
	{
	case REGERR_BADREAD:
		/* header doesn't exist, so create one */
		err = nr_CreateRoot(reg);
		break;

	case REGERR_OK:
		/* header read successfully -- convert */
        reg->hdr.magic    = nr_ReadLong ( hdrBuf + HDR_MAGIC );
        reg->hdr.verMajor = nr_ReadShort( hdrBuf + HDR_VERMAJOR );
        reg->hdr.verMinor = nr_ReadShort( hdrBuf + HDR_VERMINOR );
        reg->hdr.avail    = nr_ReadLong ( hdrBuf + HDR_AVAIL );
        reg->hdr.root     = nr_ReadLong ( hdrBuf + HDR_ROOT );

        /* check to see if it's the right file type */
		if (reg->hdr.magic != MAGIC_NUMBER) {
			err = REGERR_BADMAGIC;
            break;
        }

        /* Check registry version
         * If the major version is bumped we're incompatible
         * (minor version just means some new features were added)
         *
         * Upgrade code will go here in the future...
         */
        if ( reg->hdr.verMajor > MAJOR_VERSION ) {
            err = REGERR_REGVERSION;
            break;
        }

#if SELF_REPAIR
        if ( reg->inInit && !(reg->readOnly) ) {
    		filelength = nr_GetFileLength(reg->fh);
		    if (reg->hdr.avail != filelength)
		    {
    			reg->hdr.avail = filelength;
			    reg->hdrDirty = 1;
#if NOCACHE_HDR
			    err = nr_WriteHdr(reg);
#endif
    		}
        }
#endif	/* SELF_REPAIR */
		break;

	default:
		/* unexpected error from nr_ReadFile()*/
        XP_ASSERT(FALSE);
		err = REGERR_FAIL;
		break;
	}	/* switch */

	return err;

}	/* ReadHdr */



static REGERR nr_WriteHdr(REGFILE *reg)
{
#ifdef XP_MAC
  #define HACK_WRITE_ENTIRE_RESERVE 1
#endif

	REGERR err;
#if HACK_WRITE_ENTIRE_RESERVE
    /* 
     * pinkerton
     * Until NSPR can be fixed to seek out past the EOF on MacOS, we need to make
     * sure that we write all 128 bytes to push out the EOF to the right location
     */
    char hdrBuf[HDRRESERVE];
    memset(hdrBuf, NULL, HDRRESERVE);
#else
    char hdrBuf[sizeof(REGHDR)];
#endif

	XP_ASSERT(reg);

    if (reg->readOnly)
        return REGERR_READONLY;

    /* convert to XP int format */
    nr_WriteLong ( reg->hdr.magic,    hdrBuf + HDR_MAGIC );
    nr_WriteShort( reg->hdr.verMajor, hdrBuf + HDR_VERMAJOR );
    nr_WriteShort( reg->hdr.verMinor, hdrBuf + HDR_VERMINOR );
    nr_WriteLong ( reg->hdr.avail,    hdrBuf + HDR_AVAIL );
    nr_WriteLong ( reg->hdr.root,     hdrBuf + HDR_ROOT );

	/* err = nr_WriteFile(reg->fh, 0, sizeof(REGHDR), &reg->hdr); */
	err = nr_WriteFile(reg->fh, 0, sizeof(hdrBuf), &hdrBuf);

	if (err == REGERR_OK)
		reg->hdrDirty = 0;

	return err;

}	/* WriteHdr */



static REGERR nr_CreateRoot(REGFILE *reg)
{
	/* Called when an empty file is detected by ReadHdr */
	REGERR err;
	REGDESC root;

	XP_ASSERT(reg);

	/* Create 'hdr' */
	reg->hdr.magic      = MAGIC_NUMBER;
    reg->hdr.verMajor   = MAJOR_VERSION;
    reg->hdr.verMinor   = MINOR_VERSION;
	reg->hdr.root       = 0;
	reg->hdr.avail      = HDRRESERVE;

	/* Create root descriptor */
	root.location   = 0;
	root.left       = 0;
	root.value      = 0;
	root.down       = 0;
    root.type       = REGTYPE_KEY;
    root.valuelen   = 0;
    root.valuebuf   = 0;
    root.parent     = 0;

#ifdef XP_MAC
  #define HACK_UNTIL_NSPR_DOES_SEEK_CORRECTLY 1
#endif
#if HACK_UNTIL_NSPR_DOES_SEEK_CORRECTLY
    /* 
     * pinkerton
     * The AppendName() and AppendDesc() code that follows assumes that it can just
     * seek out past the end of the file and write a name and descriptor. However,
     * mac doesn't allow that. NSPR needs to be "fixed" somehow, but in the meantime,
     * we can just write out the header first, extending the EOF to 128 bytes where
     * the name and desc can follow w/out silently failing.
     */
	nr_WriteHdr(reg);
#endif

	err = nr_AppendName(reg, ROOTKEY_STR, &root);
	if (err != REGERR_OK)
		return err;

    err = nr_AppendDesc(reg, &root, &reg->hdr.root);
	if (err != REGERR_OK)
		return err;

	return nr_WriteHdr(reg);	/* actually commit to disk */

    /* Create standard top-level nodes */

}	/* CreateRoot */



static REGERR nr_Lock(REGFILE *reg)
{
    REGERR status;

    /* lock file */
    status = nr_LockRange(reg->fh, 0, sizeof(REGHDR));

    if (status == REGERR_OK)
    {
        /* lock the object */
        PR_Lock( reg->lock );

        /* try to refresh header info */
        status = nr_ReadHdr(reg);
        if ( status != REGERR_OK ) {
            PR_Unlock( reg->lock );
        }
    }

    return status;
}   /* Lock */



static REGERR nr_Unlock(REGFILE *reg)
{
    PR_Unlock( reg->lock );

    return nr_UnlockRange(reg->fh, 0, sizeof(REGHDR));
}   /* Unlock */



static REGERR nr_ReadDesc(REGFILE *reg, REGOFF offset, REGDESC *desc)
{

    REGERR err;
    char descBuf[ DESC_SIZE ];

    XP_ASSERT(reg);
    XP_ASSERT(offset >= HDRRESERVE);
    XP_ASSERT(offset < reg->hdr.avail);
    XP_ASSERT(desc);

    err = nr_ReadFile(reg->fh, offset, DESC_SIZE, &descBuf);
    if (err == REGERR_OK)
    {
        desc->location  = nr_ReadLong ( descBuf + DESC_LOCATION );
        desc->name      = nr_ReadLong ( descBuf + DESC_NAME );
        desc->namelen   = nr_ReadShort( descBuf + DESC_NAMELEN );
        desc->type      = nr_ReadShort( descBuf + DESC_TYPE );
        desc->left      = nr_ReadLong ( descBuf + DESC_LEFT );
        desc->value     = nr_ReadLong ( descBuf + DESC_VALUE );
        desc->valuelen  = nr_ReadLong ( descBuf + DESC_VALUELEN );
        desc->parent    = nr_ReadLong ( descBuf + DESC_PARENT );

        if ( TYPE_IS_ENTRY(desc->type) ) {
            desc->down = 0;
            desc->valuebuf  = nr_ReadLong( descBuf + DESC_VALUEBUF );
        }
        else {  /* TYPE is KEY */
            desc->down      = nr_ReadLong( descBuf + DESC_DOWN );
            desc->valuebuf  = 0;
        }

        if (desc->location != offset)
            err = REGERR_BADLOCN;
        else if ( desc->type & REGTYPE_DELETED )
            err = REGERR_DELETED;
    }

    return err;

}   /* ReadDesc */



static REGERR nr_ReadName(REGFILE *reg, REGDESC *desc, uint32 buflen, char *buf)
{

	REGERR err;

    XP_ASSERT(reg);
	XP_ASSERT(desc->name > 0);
	XP_ASSERT(desc->name < reg->hdr.avail);
	XP_ASSERT(buflen > 0);
	XP_ASSERT(buf);

    if ( desc->namelen > buflen )
        return REGERR_BUFTOOSMALL;

	err = nr_ReadFile(reg->fh, desc->name, desc->namelen, buf);

	buf[buflen-1] = '\0';	/* avoid runaways */

	return err;

}	/* ReadName */



static REGERR nr_ReadData(REGFILE *reg, REGDESC *desc, uint32 buflen, char *buf)
{

	REGERR err;

    XP_ASSERT(reg);
	XP_ASSERT(desc->value > 0);
	XP_ASSERT(desc->value < reg->hdr.avail);
	XP_ASSERT(buflen > 0);
	XP_ASSERT(buf);

    if ( desc->valuelen > buflen )
        return REGERR_BUFTOOSMALL;

  	err = nr_ReadFile(reg->fh, desc->value, desc->valuelen, buf);

	return err;

}	/* nr_ReadData */



static REGERR nr_WriteDesc(REGFILE *reg, REGDESC *desc)
{
    char descBuf[ DESC_SIZE ];

    XP_ASSERT(reg);
	XP_ASSERT(desc);
    XP_ASSERT( desc->location >= HDRRESERVE );
    XP_ASSERT( desc->location < reg->hdr.avail );

    if (reg->readOnly)
        return REGERR_READONLY;

    /* convert to XP int format */
    nr_WriteLong ( desc->location,  descBuf + DESC_LOCATION );
    nr_WriteLong ( desc->name,      descBuf + DESC_NAME );
    nr_WriteShort( desc->namelen,   descBuf + DESC_NAMELEN );
    nr_WriteShort( desc->type,      descBuf + DESC_TYPE );
    nr_WriteLong ( desc->left,      descBuf + DESC_LEFT );
    nr_WriteLong ( desc->value,     descBuf + DESC_VALUE );
    nr_WriteLong ( desc->valuelen,  descBuf + DESC_VALUELEN );
    nr_WriteLong ( desc->parent,    descBuf + DESC_PARENT );

    if ( TYPE_IS_ENTRY(desc->type) ) {
        XP_ASSERT( 0 == desc->down );
        nr_WriteLong( desc->valuebuf,  descBuf + DESC_VALUEBUF );
    }
    else {  /* TYPE is KEY */
        XP_ASSERT( 0 == desc->valuebuf );
        nr_WriteLong( desc->down,      descBuf + DESC_DOWN );
    }

	return nr_WriteFile(reg->fh, desc->location, DESC_SIZE, descBuf);
}   /* nr_WriteDesc */



static REGERR nr_AppendDesc(REGFILE *reg, REGDESC *desc, REGOFF *result)
{

	REGERR err;
    char descBuf[ DESC_SIZE ];

	XP_ASSERT(reg);
	XP_ASSERT(desc);
	XP_ASSERT(result);

	*result = 0;

    if (reg->readOnly)
        return REGERR_READONLY;

	desc->location = reg->hdr.avail;

    /* convert to XP int format */
    nr_WriteLong ( desc->location,  descBuf + DESC_LOCATION );
    nr_WriteLong ( desc->name,      descBuf + DESC_NAME );
    nr_WriteShort( desc->namelen,   descBuf + DESC_NAMELEN );
    nr_WriteShort( desc->type,      descBuf + DESC_TYPE );
    nr_WriteLong ( desc->left,      descBuf + DESC_LEFT );
    nr_WriteLong ( desc->value,     descBuf + DESC_VALUE );
    nr_WriteLong ( desc->valuelen,  descBuf + DESC_VALUELEN );
    nr_WriteLong ( desc->parent,    descBuf + DESC_PARENT );

    if ( TYPE_IS_ENTRY(desc->type) ) {
        XP_ASSERT( 0 == desc->down );
        nr_WriteLong( desc->valuebuf,  descBuf + DESC_VALUEBUF );
    }
    else {  /* TYPE is KEY */
        XP_ASSERT( 0 == desc->valuebuf );
        nr_WriteLong( desc->down,      descBuf + DESC_DOWN );
    }

	err = nr_WriteFile(reg->fh, reg->hdr.avail, DESC_SIZE, descBuf);

    if (err == REGERR_OK)
	{
		*result = reg->hdr.avail;
		reg->hdr.avail += DESC_SIZE;
		reg->hdrDirty = 1;
#if NOCACHE_HDR
		err = nr_WriteHdr(reg);
#endif
	}

	return err;

}	/* AppendDesc */



static REGERR nr_AppendName(REGFILE *reg, char *name, REGDESC *desc)
{
	REGERR err;
	int len;
    char *p;

	XP_ASSERT(reg);
	XP_ASSERT(name);
	XP_ASSERT(desc);

    if (!nr_IsValidUTF8(name))
        return REGERR_BADUTF8;
    if (reg->readOnly)
        return REGERR_READONLY;

    len = XP_STRLEN(name) + 1;

    /* check for valid name parameter */
    if ( len == 1 )
        return REGERR_PARAM;

    if ( len > MAXREGNAMELEN )
        return REGERR_NAMETOOLONG;

    for ( p = name; (*p != 0); p++ ) {
        if ( INVALID_NAME_CHAR(*p) )
            return REGERR_BADNAME;
    }

    /* save the name */
    err = nr_WriteFile(reg->fh, reg->hdr.avail, len, name);

    /* if write successful update the desc and hdr */
    if (err == REGERR_OK)
    {
        desc->namelen = (uint16)len;
        desc->name = reg->hdr.avail;
        reg->hdr.avail += len;
        reg->hdrDirty = 1;
#if NOCACHE_HDR
        err = nr_WriteHdr(reg);
#endif
    }

    return err;

}   /* nr_AppendName */



static REGERR nr_WriteString(REGFILE *reg, char *string, REGDESC *desc)
{
    uint32 len;

    XP_ASSERT(string);
    if (!nr_IsValidUTF8(string))
        return REGERR_BADUTF8;
    if (reg->readOnly)
        return REGERR_READONLY;
    len = XP_STRLEN(string) + 1;

    return nr_WriteData( reg, string, len, desc );

}   /* nr_WriteString */



static REGERR nr_WriteData(REGFILE *reg, char *string, uint32 len, REGDESC *desc)
{
    REGERR err;

    XP_ASSERT(reg);
    XP_ASSERT(string);
    XP_ASSERT(desc);

    if (reg->readOnly)
        return REGERR_READONLY;

    if ( len == 0 )
        return REGERR_PARAM;

    if ( len > MAXREGVALUELEN )
        return REGERR_NAMETOOLONG;

    /* save the data in the same place if it fits */
    if ( len <= desc->valuebuf ) {
        err = nr_WriteFile( reg->fh, desc->value, len, string );
        if ( err == REGERR_OK ) {
            desc->valuelen = len;
        }
    }
    else {
        /* otherwise append new data */
        err = nr_AppendData( reg, string, len, desc );
    }

    return err;

}   /* nr_WriteData */



static REGERR nr_AppendString(REGFILE *reg, char *string, REGDESC *desc)
{
    uint32 len;

    XP_ASSERT(string);
    if (!nr_IsValidUTF8(string))
        return REGERR_BADUTF8;
    if (reg->readOnly)
        return REGERR_READONLY;
    len = XP_STRLEN(string) + 1;

    return nr_AppendData( reg, string, len, desc );

}   /* nr_AppendString */



static REGERR nr_AppendData(REGFILE *reg, char *string, uint32 len, REGDESC *desc)
{
	REGERR err;

	XP_ASSERT(reg);
	XP_ASSERT(string);
	XP_ASSERT(desc);

    if (reg->readOnly)
        return REGERR_READONLY;

    if ( len == 0 )
        return REGERR_PARAM;

    if ( len > MAXREGVALUELEN )
        return REGERR_NAMETOOLONG;

    /* save the string */
	err = nr_WriteFile(reg->fh, reg->hdr.avail, len, string);
	if (err == REGERR_OK)
	{
        desc->value     = reg->hdr.avail;
        desc->valuelen  = len;
        desc->valuebuf  = len;

		reg->hdr.avail += len;
		reg->hdrDirty   = 1;
#if NOCACHE_HDR
		err = nr_WriteHdr(reg);
#endif
	}

	return err;

}	/* nr_AppendData */

static XP_Bool nr_IsValidUTF8(char *string)
{
	int follow = 0;
    char *c;
    unsigned char ch;

	XP_ASSERT(string);
    if ( !string )
        return FALSE;

    for ( c = string; *c != '\0'; c++ )
    {
        ch = (unsigned char)*c;
        if( follow == 0 )
        {
            /* expecting an initial byte */
            if ( ch <= 0x7F )
            {
                /* standard byte -- do nothing */
            }
            else if ((0xC0 & ch) == 0x80)
            {
                /* follow byte illegal here */
                return FALSE;
            }
            else if ((0xE0 & ch) == 0xC0)
            {
                follow = 1;
            }
            else if ((0xF0 & ch) == 0xE0)
            {
                follow = 2;
            }
            else
            { 
                /* unexpected (unsupported) initial byte */
                return FALSE;
            }
        }
        else 
        {
            XP_ASSERT( follow > 0 );
            if ((0xC0 & ch) == 0x80)
            {
                /* expecting follow byte and found one */
                follow--;
            }
            else 
            {
                /* invalid state */
                return FALSE;
            }
        }
    } /* for */

    if ( follow != 0 )
    {
        /* invalid state -- interrupted character */
        return FALSE;
    }
    
    return TRUE;
}   /* checks if a string is UTF-8 encoded */

/* --------------------------------------------------------------------
 * Path Parsing
 * --------------------------------------------------------------------
 */
static REGERR nr_NextName(char *pPath, char *buf, uint32 bufsize, char **newPath);
static REGERR nr_RemoveName(char *path);
static REGERR nr_CatName(REGFILE *reg, REGOFF node, char *path, uint32 bufsize,
                    REGDESC *desc);
static REGERR nr_ReplaceName(REGFILE *reg, REGOFF node, char *path,
                    uint32 bufsize, REGDESC *desc);

#if 0 /* old interface */
static char * nr_LastDelim(char *pPath);
static Bool   nr_IsQualifiedEntry(char *pPath);
static REGERR nr_SplitEntry(char *pPath, char **name, char **value);
static REGERR nr_CatNameValue(char *path, int bufsize, REGDESC *desc);
static REGERR nr_ReplaceNameValue(char *path, int bufsize, REGDESC *desc);
#endif
/* -------------------------------------------------------------------- */


/* Scans path at 'pPath' and copies next name segment into 'buf'.
 * Also sets 'newPath' to point at the next segment of pPath.
 */
static REGERR nr_NextName(char *pPath, char *buf, uint32 bufsize, char **newPath)
{
    uint32 len = 0;
    REGERR err = REGERR_OK;

    /* initialization and validation */
	XP_ASSERT(buf);

    *newPath = NULL;
    *buf = '\0';

	if ( pPath==NULL || *pPath=='\0' )
		return REGERR_NOMORE;

    /* ... skip an initial path delimiter */
	if ( *pPath == PATHDEL ) {
		pPath++;

    	if ( *pPath == '\0' )
    		return REGERR_NOMORE;
    }

    /* ... missing name segment or initial blank are errors*/
    if ( *pPath == PATHDEL || *pPath == ' ' )
        return REGERR_BADNAME;

    /* copy first path segment into return buf */
	while ( *pPath != '\0' && *pPath != PATHDEL )
	{
        if ( len == bufsize ) {
            err = REGERR_NAMETOOLONG;
            break;
        }
        if ( *pPath < ' ' && *pPath > 0 )
            return REGERR_BADNAME;

		*buf++ = *pPath++;
        len++;
	}
	*buf = '\0';

    /* ... name segment can't end with blanks, either */
    if ( ' ' == *(buf-1) )
        return REGERR_BADNAME;

    /* return a pointer to the start of the next segment */
    *newPath = pPath;

	return err;

}	/* nr_NextName */




static REGERR nr_CatName(REGFILE *reg, REGOFF node, char *path, uint32 bufsize, REGDESC *desc)
{
    REGERR err = REGERR_OK;

	char   *p;
	uint32 len = XP_STRLEN(path);

	if (len > 0)
	{
		p = &path[len-1];
		if (*p != PATHDEL)
		{
            if ( len < bufsize ) {
			    p++;
			    *p = PATHDEL;
                len++;
            }
            else
                err = REGERR_BUFTOOSMALL;
		}
		p++;	/* point one past PATHDEL */
	}
	else
		p = path;

    if ( err == REGERR_OK ) {
        err = nr_ReadDesc( reg, node, desc );
        if ( err == REGERR_OK ) {
        	err = nr_ReadName( reg, desc, bufsize-len, p );
        }
    }

    return err;

}	/* CatName */



static REGERR nr_ReplaceName(REGFILE *reg, REGOFF node, char *path, uint32 bufsize, REGDESC *desc)
{
    /* NOTE! It is EXREMELY important that names be in UTF-8; otherwise
     * the backwards path search will fail for multi-byte/Unicode names
     */

	char   *p;
    uint32 len;
	REGERR err;

    XP_ASSERT(path);

	len = XP_STRLEN(path);
    if ( len > bufsize )
        return REGERR_PARAM;

    if ( len > 0 ) {
        p = &path[len-1];

        while ((p > path) && (*p != PATHDEL)) {
            --p;
            --len;
        }
        if ( *p == PATHDEL ) {
            p++; 
            len++;
        }
    }
    else
        p = path;


    err = nr_ReadDesc( reg, node, desc );
    if ( err == REGERR_OK ) {
    	err = nr_ReadName( reg, desc, bufsize-len, p );
    }

    return err;

}	/* ReplaceName */


static REGERR nr_RemoveName(char *path)
{
	/* Typical inputs:
	 * path = "/Machine/4.0/"	output = "/Machine"
	 * path = "/Machine"		output = ""
	 * path = ""				output = REGERR_NOMORE
     *
     * NOTE! It is EXREMELY important that names be in UTF-8; otherwise
     * the backwards path search will fail for multi-byte/Unicode names
	 */

	int len = XP_STRLEN(path);
	char *p;
	if (len < 1)
		return REGERR_NOMORE;

	p = &path[len-1];
	/* if last char is '/', ignore it */
	if (*p == PATHDEL)
		p--;

	while ((p > path) && (*p != PATHDEL))
		p--;

/*	if (*p != PATHDEL)
		return REGERR_NOMORE;
*/

	*p = '\0';
	return REGERR_OK;

}	/* RemoveName */



/* --------------------------------------------------------------------
 * Key/Entry Management
 * --------------------------------------------------------------------
 */
static REGERR nr_Find(REGFILE *reg, REGOFF offParent, char *pPath,
    REGDESC *pDesc,	REGOFF *pPrev, REGOFF *pParent, Bool raw);

static REGERR nr_FindAtLevel(REGFILE *reg, REGOFF offFirst, char *pName,
	REGDESC *pDesc, REGOFF *pOffPrev);

static REGERR nr_CreateSubKey(REGFILE *reg, REGDESC *pParent, char *name);
static REGERR nr_CreateEntryString(REGFILE *reg, REGDESC *pParent, 
    char *name, char *value);
static REGERR nr_CreateEntry(REGFILE *reg, REGDESC *pParent, char *name,
    uint16 type, char *buffer, uint32 length);
/* -------------------------------------------------------------------- */



static REGERR nr_Find(REGFILE *reg,
            REGOFF offParent,
			char *pPath,
			REGDESC *pDesc,
			REGOFF *pPrev,
			REGOFF *pParent,
			Bool raw)
{

    REGERR  err;
    REGDESC desc;
    REGOFF  offPrev = 0;
    char    namebuf[MAXREGNAMELEN];
    char    *p;

    XP_ASSERT( pPath != NULL );
    XP_ASSERT( offParent >= HDRRESERVE );
    XP_ASSERT( VALID_FILEHANDLE( reg->fh ) );

    if (pPrev)
        *pPrev = 0;
    if (pParent)
        *pParent = 0;

    /* read starting desc */
    err = nr_ReadDesc( reg, offParent, &desc);

    if (raw == TRUE) {
        /* save current location as parent of next segment */
        offParent = desc.location;
        /* look for name at next level down */
        err = nr_FindAtLevel(reg, desc.down, pPath, &desc, &offPrev);
    }
    else {
        /* Walk 'path', reading keys into 'desc' */
        p = pPath;
        while ( err == REGERR_OK ) 
        {
            err = nr_NextName(p, namebuf, sizeof(namebuf), &p);

            if ( err == REGERR_OK ) {
                /* save current location as parent of next segment */
                offParent = desc.location;
                /* look for name at next level down */
                err = nr_FindAtLevel(reg, desc.down, namebuf, &desc, &offPrev);
            }
        }
    }

    if ( (raw == FALSE && err == REGERR_NOMORE) ||
            (raw == TRUE && err == REGERR_OK) ) {
        /* we found all the segments of the path--success! */
        err = REGERR_OK;

        if (pDesc) {
            COPYDESC(pDesc, &desc);
        }
    	if (pPrev) {
    		*pPrev = offPrev;
        }
    	if (pParent) {
    		*pParent = offParent;
        }
    }
    
    return err;

}   /* nr_Find */




/* nr_FindAtLevel -- looks for a node matching "pName" on the level starting
 *                   with "offset".  Returns REGERR_OK if found, REGERR_NOFIND
 *                   if not (plus other error conditions).
 *
 *                   If pDesc and pOffPrev are valid pointers *AND* the name is
 *                   found then pDesc will point at the REGDESC of the node and
 *                   pOffPrev will be the offset of the desc for the previous
 *                   node at the same level.  These values aren't touched
 *                   if the node is not found.
 */
static REGERR nr_FindAtLevel(REGFILE *reg,
							 REGOFF offset,
							 char *pName,
							 REGDESC *pDesc,
							 REGOFF *pOffPrev)
{
	char    namebuf[MAXREGNAMELEN];
	REGDESC desc;
	REGERR  err;
    REGOFF  prev = 0;

	/* Note: offset=0 when there's no 'down' or 'left' */
	XP_ASSERT(reg);
	XP_ASSERT(offset < reg->hdr.avail);
	XP_ASSERT(pName);
	XP_ASSERT(*pName);

	while ( offset != 0 )
    {
        /* get name of next node */
    	err = nr_ReadDesc(reg, offset, &desc);
		if (err != REGERR_OK)
			return err;

		err = nr_ReadName(reg, &desc, sizeof(namebuf), namebuf);
		if (err != REGERR_OK)
			return err;

        /* check to see if it's the one we want */
		if (XP_STRCASECMP(namebuf, pName) == 0) {
            /* Found it! */
            if ( pDesc != NULL ) {
                COPYDESC( pDesc, &desc );
            }
            if ( pOffPrev != NULL ) {
                *pOffPrev = prev;
            }
            return REGERR_OK;
        }

        /* advance to the next node */
        prev = offset;
		offset = desc.left;
	}

	return REGERR_NOFIND;
}	/* FindAtLevel */



static REGERR nr_CreateSubKey(REGFILE *reg, REGDESC *pParent, char *name)
{
    /* nr_CreateSubKey does NO error checking--callers *MUST*
     * ensure that there are no duplicates
     */
    REGDESC desc;
    REGERR err;

    XP_ASSERT(reg);
    XP_ASSERT(pParent);
    XP_ASSERT(name);

    err = nr_AppendName(reg, name, &desc);
    if (err != REGERR_OK)
        return err;

    desc.type = REGTYPE_KEY;
    desc.left = pParent->down;
    desc.down = 0;
    desc.value = 0;
    desc.valuelen = 0;
    desc.valuebuf = 0;
    desc.parent   = pParent->location;

    err = nr_AppendDesc(reg, &desc, &pParent->down);
    if (err != REGERR_OK)
        return err;

    /* printf("nr_CreateSubKey: %s @0x%lx\n", name, pParent->down); */

    err = nr_WriteDesc(reg, pParent);
    COPYDESC(pParent, &desc);

    return err;

}   /* nr_CreateSubKey */



static REGERR nr_CreateEntryString(REGFILE *reg, REGDESC *pParent, char *name, char *value)
{
    REGDESC desc;
    REGERR  err;

    XP_ASSERT(reg);
    XP_ASSERT(pParent);
    XP_ASSERT(name);
    XP_ASSERT(value);

    memset( &desc, 0, sizeof(REGDESC) );

    err = nr_AppendName(reg, name, &desc);
    if (err != REGERR_OK)
        return err;

    err = nr_AppendString(reg, value, &desc);
    if (err != REGERR_OK)
        return err;

    desc.type = REGTYPE_ENTRY_STRING_UTF;
    desc.left = pParent->value;
    desc.down = 0;
    desc.parent = pParent->location;

    err = nr_AppendDesc(reg, &desc, &pParent->value);
    if (err != REGERR_OK)
        return err;

    /* printf("nr_AddEntry: %s=%s @0x%lx\n", name, value, pParent->value); */

    return nr_WriteDesc(reg, pParent);

}   /* nr_CreateEntryString */



static REGERR nr_CreateEntry(REGFILE *reg, REGDESC *pParent, char *name,
    uint16 type, char *value, uint32 length)
{
	REGDESC desc;
	REGERR  err;

	XP_ASSERT(reg);
	XP_ASSERT(pParent);
	XP_ASSERT(name);
	XP_ASSERT(value);

    memset( &desc, 0, sizeof(REGDESC) );

	err = nr_AppendName(reg, name, &desc);
    if (err != REGERR_OK)
		return err;

	err = nr_AppendData(reg, value, length, &desc);
    if (err != REGERR_OK)
		return err;

    desc.type = type;
	desc.left = pParent->value;
	desc.down = 0;
    desc.parent = pParent->location;

	err = nr_AppendDesc(reg, &desc, &pParent->value);
    if (err != REGERR_OK)
		return err;

	/* printf("nr_AddEntry: %s=%s @0x%lx\n", name, value, pParent->value); */

	return nr_WriteDesc(reg, pParent);

}   /* nr_CreateEntry */



#if 0 /* old interface */
VR_INTERFACE(REGERR) NR_RegRename(RKEY key, char *path, char *newname)
{

	REGDESC desc;
	REGERR err;

	if ( !VALID_FILEHANDLE(gReg.fh) )
		return REGERR_FAIL;

	/* Okay to Update to "", but names must not be null */
	if (!newname || !*newname)
		return REGERR_PARAM;

	err = nr_Lock(&gReg);
	if (err != REGERR_OK)
		return err;

	/* Find the key or entry to rename (and validate the
		incoming parameters) */
	err = nr_Find((REGOFF)key, path, &desc, 0, 0, FALSE);
	if (err != REGERR_OK)
		goto cleanup;

	/* Write the new name string to the file and update
		the key or entry's pointer */
	err = nr_AppendName(&gReg, newname, &desc);
	if (err != REGERR_OK)
		goto cleanup;

	/* Write the key or entry back to the disk and return */
	err = nr_WriteDesc(&gReg, &desc);

cleanup:
	nr_Unlock(&gReg);
	return err;

}	/* RegRename */



VR_INTERFACE(REGERR) NR_RegPack(char *newfilename)
{

	REGFILE dstReg;
	char *path = NULL;
	int err = REGERR_OK;

	dstReg.fh = NULL;

	if (!newfilename || !*newfilename)
		return REGERR_PARAM;
	if ( !VALID_FILEHANDLE(gReg.fh) )
		return REGERR_FAIL;

	/* open it & create a header by trying to read */
	err = nr_OpenFile(newfilename, &dstReg.fh);
	if (err != REGERR_OK)
		goto cleanup;
	err = nr_ReadHdr(&dstReg);
	if (err != REGERR_OK)
		goto cleanup;

	/* read records from the current registry file and
		add them to 'dstReg' */
	path = XP_ALLOC(PACKBUFFERSIZE);
	if (path == NULL)
	{
		err = REGERR_FAIL;
		goto cleanup;
	}

	XP_STRCPY(path, "/");

	err = nr_Lock(&gReg);
	if (err != REGERR_OK)
		goto cleanup;

	while (NR_RegNext( 0, PACKBUFFERSIZE, path ) == REGERR_OK)
	{
		err = nr_RegGenericAdd(&dstReg, 0, path);
		if (err != REGERR_OK)
			break;
	}

	nr_Unlock(&gReg);

cleanup:
    XP_FREEIF(path);
	if ( VALID_FILEHANDLE(dstReg.fh) )
    {
        /* even if not caching headers it could be dirty due to an error */
	    if (dstReg.hdrDirty) {
		    nr_WriteHdr(&dstReg);
        }
		nr_CloseFile(&dstReg.fh);
	}

	return err;

}	/* RegPack */

#endif /* old interface */



/* ---------------------------------------------------------------------
 * Intermediate API
 * ---------------------------------------------------------------------
 */
static REGOFF nr_TranslateKey( REGFILE *reg, RKEY key );
static void   nr_InitStdRkeys( REGFILE *reg );
static Bool   nr_ProtectedNode( REGFILE *reg, REGOFF key );
static REGERR nr_RegAddKey( REGFILE *reg, RKEY key, char *path, RKEY *newKey, Bool raw );
static REGERR nr_RegDeleteKey( REGFILE *reg, RKEY key, char *path, Bool raw );
static REGERR nr_RegOpen( char *filename, HREG *hReg );
static REGERR nr_RegClose( HREG hReg );
static char*  nr_GetUsername();
static char*  nr_GetRegName (char *name);

/* --------------------------------------------------------------------- */


static REGOFF nr_TranslateKey( REGFILE *reg, RKEY key )
{
    REGOFF retKey = 0;

    /* if it's a special key  */
    if ( key < HDRRESERVE )  {
        /* ...translate it */
        switch (key)
        {
            case ROOTKEY:
                retKey = reg->hdr.root;
                break;

            case ROOTKEY_VERSIONS:
                retKey = reg->rkeys.versions;
                break;

            case ROOTKEY_USERS:
                retKey = reg->rkeys.users;
                break;

            case ROOTKEY_COMMON:
                retKey = reg->rkeys.common;
                break;

#ifndef STANDALONE_REGISTRY
            case ROOTKEY_CURRENT_USER:
                if ( reg->rkeys.current_user == 0 ) {
                    /* not initialized--find the current user key */
                    RKEY    userkey = 0;
                    REGERR  err;
                    char*   profName;

                    profName = nr_GetUsername();
                    if ( NULL != profName ) {
                        /* Don't assign a slot for missing or magic profile */
                        if ( '\0' == *profName ||
                            0 == XP_STRCMP(ASW_MAGIC_PROFILE_NAME, profName)) 
                        {
                            err = REGERR_FAIL;
                        } else {
                            err = nr_RegAddKey( reg, reg->rkeys.users, profName, &userkey, FALSE );
                        }
                        XP_FREE(profName);
                    }
                    else {
                        err = nr_RegAddKey( reg, reg->rkeys.users, "default", &userkey, FALSE );
                    }

                    if ( err == REGERR_OK ) {
                        reg->rkeys.current_user = userkey;
                    }
                }
                retKey = reg->rkeys.current_user;
                break;
#endif /* !STANDALONE_REGISTRY */

            case ROOTKEY_PRIVATE:
                retKey = reg->rkeys.privarea;
                break;

            default:
                /* not a valid key */
                retKey = 0;
                break;
        }
    }
    else {
        /* ...otherwise it's fine as-is */
        retKey = (REGOFF)key;
    }
    return ( retKey );
}  /* nr_TranslateKey */



static void   nr_InitStdRkeys( REGFILE *reg )
{
    REGERR      err;
    RKEY        key;

    XP_ASSERT( reg != NULL );

    /* initialize to invalid key values */
    memset( &reg->rkeys, 0, sizeof(STDNODES) );

    /* Add each key before looking it up.  Adding an already
     * existing key is harmless, and these MUST exist.
     */

    /* ROOTKEY_USERS */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_USERS_STR, &key, FALSE );
    if ( err == REGERR_OK ) {
        reg->rkeys.users = key;
    }

    /* ROOTKEY_COMMON */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_COMMON_STR, &key, FALSE );
    if ( err == REGERR_OK ) {
        reg->rkeys.common = key;
    }

    /* ROOTKEY_VERSIONS */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_VERSIONS_STR, &key, FALSE );
    if ( err == REGERR_OK ) {
        reg->rkeys.versions = key;
    }

    /* ROOTKEY_CURRENT_USER */
	/* delay until first use -- see nr_TranslateKey */

    /* ROOTKEY_PRIVATE */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_PRIVATE_STR, &key, FALSE );
    if ( err == REGERR_OK ) {
        reg->rkeys.privarea = key;
    }
}   /* nr_InitStdRkeys */



static Bool nr_ProtectedNode( REGFILE *reg, REGOFF key )
{
    if ( (key == reg->hdr.root) ||
         (key == reg->rkeys.users) ||
         (key == reg->rkeys.versions) ||
         (key == reg->rkeys.common) ||
         (key == reg->rkeys.current_user) )
    {
        return TRUE;
    }
    else
        return FALSE;
}



static REGERR nr_RegAddKey( REGFILE *reg, RKEY key, char *path, RKEY *newKey, Bool raw )
{
    REGERR      err;
    REGDESC     desc;
    REGOFF      start;
    char        namebuf[MAXREGNAMELEN];
    char        *p;

    XP_ASSERT( regStartCount > 0 );
    XP_ASSERT( reg != NULL );
    XP_ASSERT( path != NULL );
    XP_ASSERT( *path != '\0' );
    XP_ASSERT( VALID_FILEHANDLE( reg->fh ) );

    /* have to translate again in case this is an internal call */
    start = nr_TranslateKey( reg, key );

    /* Get starting desc */
    err = nr_ReadDesc( reg, start, &desc );

    if (raw == TRUE) {
        /* look for name at next level down */
        err = nr_FindAtLevel(reg, desc.down, path, &desc, 0);

        /* if key is not found */
        if ( err == REGERR_NOFIND ) {
            /* add it as a sub-key to the last found key */
            err = nr_CreateSubKey(reg, &desc, path);
        }
    }
    else {
        /* Walk 'path', reading keys into 'desc' */
        p = path;
        while ( err == REGERR_OK ) {

            /* get next name on the path */
            err = nr_NextName(p, namebuf, sizeof(namebuf), &p);
            if ( err == REGERR_OK ) {

                /* look for name at next level down */
                err = nr_FindAtLevel(reg, desc.down, namebuf, &desc, 0);

                /* if key is not found */
                if ( err == REGERR_NOFIND ) {
                    /* add it as a sub-key to the last found key */
                    err = nr_CreateSubKey(reg, &desc, namebuf);
                }
            }
        }
    }

    /* it's good to have processed the whole path */
    if ( (raw == FALSE && err == REGERR_NOMORE) ||
         (raw == TRUE && err == REGERR_OK) ) 
    {
        err = REGERR_OK;

        /* return new key if the caller wants it */
        if ( newKey != NULL ) {
            *newKey = desc.location;
        }
    }

    return err;

}   /* nr_RegAddKey */




static REGERR nr_RegDeleteKey( REGFILE *reg, RKEY key, char *path, Bool raw )
{
    REGERR      err;
    REGOFF      start;
    REGDESC     desc;
    REGDESC     predecessor;
    REGOFF      offPrev;
    REGOFF      offParent;
    REGOFF*     link;

    XP_ASSERT( regStartCount > 0 );
    XP_ASSERT( reg != NULL );
    XP_ASSERT( path != NULL );
    XP_ASSERT( *path != '\0' );
    XP_ASSERT( VALID_FILEHANDLE( reg->fh ) );

    start = nr_TranslateKey( reg, key );
    if ( path == NULL || *path == '\0' || start == 0 )
        return REGERR_PARAM;

    /* find the specified key */
    err = nr_Find( reg, start, path, &desc, &offPrev, &offParent, raw );
    if ( err == REGERR_OK ) {

        XP_ASSERT( !TYPE_IS_ENTRY( desc.type ) );

        /* make sure it's childless and not a top-level key */
        if ( (desc.down == 0) && !nr_ProtectedNode( reg, desc.location ) ) {

            /* Are we the first on our level? */
            if ( offPrev == 0 ) {
                /* Yes: link to parent's "down" pointer */
                err = nr_ReadDesc( reg, offParent, &predecessor );
                link = &(predecessor.down);
            }
            else {
                /* No: link using predecessor's "left" pointer */
                err = nr_ReadDesc( reg, offPrev, &predecessor );
                link = &(predecessor.left);
            }

            /* If we read the predecessor desc OK */
            if (err == REGERR_OK) {
                XP_ASSERT( *link == desc.location );

                /* link predecessor to next, removing current node from chain */
                *link = desc.left;

                /* Write the updated predecessor */
                err = nr_WriteDesc( reg, &predecessor );
                if ( err == REGERR_OK ) {
                    /* Mark key deleted to prevent bogus use by anyone
                     * who is holding an RKEY for that node
                     */
                    desc.type |= REGTYPE_DELETED;
                    err = nr_WriteDesc( reg, &desc );
                }
            }
        }
        else {
            /* specified node is protected from deletion */
            err = REGERR_FAIL;
        }
    }

    return err;

}   /* nr_RegDeleteKey */




static REGERR nr_RegOpen( char *filename, HREG *hReg )
{
    REGERR    status = REGERR_OK;
    REGFILE   *pReg;
    REGHANDLE *pHandle;

    XP_ASSERT( regStartCount > 0 );

    /* initialize output handle in case of error */
    if ( hReg == NULL ) {
        return REGERR_PARAM;
    }
    *hReg = NULL;
    
    /* Look for named file in list of open registries */
    filename = nr_GetRegName( filename );
    if (filename == NULL) {
        filename = "";
    }
    pReg = vr_findRegFile( filename );

    /* if registry not already open */
    if (pReg == NULL) {

        /* ...then open it */
        pReg = (REGFILE*)XP_ALLOC( sizeof(REGFILE) );
        if ( pReg == NULL ) {
            status = REGERR_MEMORY;
            goto bail;
        }
        XP_MEMSET(pReg, 0, sizeof(REGFILE));

        pReg->inInit = TRUE;
        pReg->filename = XP_STRDUP(filename);
        if (pReg->filename == NULL) {
            XP_FREE( pReg );
            status = REGERR_MEMORY;
            goto bail;
        }

        status = nr_OpenFile( filename, &(pReg->fh) );
        if (status == REGERR_READONLY) {
            /* Open, but read only */
            pReg->readOnly = TRUE;
            status = REGERR_OK;
        }
        if ( status != REGERR_OK ) {
            XP_FREE( pReg->filename );
            XP_FREE( pReg );

            goto bail;
        }

        /* ...read and validate the header */
        status = nr_ReadHdr( pReg );
        if ( status != REGERR_OK ) {
            nr_CloseFile( &(pReg->fh) );
            XP_FREE( pReg->filename );
            XP_FREE( pReg );
            goto bail;
        }

        /* ...other misc initialization */
        pReg->refCount = 0;

#ifndef STANDALONE_REGISTRY
        pReg->lock = PR_NewLock();
#endif
        /* now done with everything that needs to protect the header */
        pReg->inInit = FALSE;

        nr_InitStdRkeys( pReg );

        /* ...and add it to the list */
        nr_AddNode( pReg );
    }

    /* create a new handle to the regfile */
    pHandle = (REGHANDLE*)XP_ALLOC( sizeof(REGHANDLE) );
    if ( pHandle == NULL ) {
        /* we can't create the handle */
        if ( pReg->refCount == 0 ) {
            /* we've just opened it so close it and remove node */
            nr_CloseFile( &(pReg->fh) );
            nr_DeleteNode( pReg );
        }

        status = REGERR_MEMORY;
        goto bail;
    }

    pHandle->magic   = MAGIC_NUMBER;
    pHandle->pReg    = pReg;

    /* success: bump the reference count and return the handle */
    pReg->refCount++;
    *hReg = (void*)pHandle;

bail:
    return status;

}   /* nr_RegOpen */



static REGERR nr_RegClose( HREG hReg )
{
    REGERR      err = REGERR_OK;
    REGHANDLE*  reghnd = (REGHANDLE*)hReg;

    XP_ASSERT( regStartCount > 0 );

    /* verify handle */
    err = VERIFY_HREG( hReg );
    if ( err == REGERR_OK )
    {
        XP_ASSERT( VALID_FILEHANDLE(reghnd->pReg->fh) );

        /* lower REGFILE user count */
        reghnd->pReg->refCount--;

        /* if registry is no longer in use */
        if ( reghnd->pReg->refCount < 1 ) {
            /* ...then close the file */
            if ( reghnd->pReg->hdrDirty ) {
                nr_WriteHdr( reghnd->pReg );
            }
            nr_CloseFile( &(reghnd->pReg->fh) );
            /* ...and delete REGFILE node from list */
            nr_DeleteNode( reghnd->pReg );
        }

        reghnd->magic = 0;    /* prevent accidental re-use */  
        XP_FREE( reghnd );
    }

    return err;

}   /* nr_RegClose */



static char *nr_GetUsername()
{
  if (NULL == user_name) {
    return "default";
  } else {
    return user_name;
  }
}

static char* nr_GetRegName (char *name)
{
    if (name == NULL || *name == '\0') {
        XP_ASSERT( globalRegName != NULL );
        return globalRegName;
    } else {
        return name;
    }
}




/* ---------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */


/* ---------------------------------------------------------------------
 * NR_RegGetUsername - Gets a copy of the current username
 *
 * Parameters:
 *   A variable which, on exit will contain an alloc'ed string which is a
 *   copy of the current username.
 *
 * Output: 
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) NR_RegGetUsername(char **name)
{
    /* XXX: does this need locking? */

    if ( name == NULL )
        return REGERR_PARAM;

    *name = XP_STRDUP(nr_GetUsername());

    if ( NULL == *name )
        return REGERR_MEMORY;

    return REGERR_OK;
}




/* ---------------------------------------------------------------------
 * NR_RegSetUsername - Set the current username
 *
 * Parameters:
 *     name     - name of the current user
 *
 * Output:
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) NR_RegSetUsername(const char *name)
{
    char *tmp;

    if ( name == NULL || *name == '\0' )
        return REGERR_PARAM;

    tmp = XP_STRDUP(name);
    if (NULL == tmp) {
        return REGERR_MEMORY;
    }

    PR_Lock( reglist_lock );

    XP_FREEIF(user_name);
    user_name = tmp;

/* XXX: changing the username should go through and clear out the current.user
   for each open registry. */

    PR_Unlock( reglist_lock );
  
    return REGERR_OK;
}




/* ---------------------------------------------------------------------
 * NReg_Open - Open a netscape XP registry
 *
 * Parameters:
 *    filename   - registry file to open. NULL or ""  opens standard
 *                 local registry. (This parameter currently ignored)
 *    hReg       - OUT: handle to opened registry
 *
 * Output:
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegOpen( char *filename, HREG *hReg )
{
    REGERR    status = REGERR_OK;

     /* you must call NR_StartupRegistry() first */
    if ( regStartCount <= 0 )
        return REGERR_FAIL;

    PR_Lock(reglist_lock);

    status = nr_RegOpen( filename, hReg );

    PR_Unlock(reglist_lock);

    return status;

}   /* NR_RegOpen */




/* ---------------------------------------------------------------------
 * NR_RegClose - Close a netscape XP registry
 *
 * Parameters:
 *    hReg     - handle of open registry to be closed.
 *
 * After calling this routine the handle is no longer valid
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegClose( HREG hReg )
{
    REGERR      err = REGERR_OK;

    PR_Lock( reglist_lock );

    err = nr_RegClose( hReg );

    PR_Unlock(reglist_lock);

    return err;

}   /* NR_RegClose */




/* ---------------------------------------------------------------------
 * NR_RegAddKey - Add a key node to the registry
 *
 *      This routine is simply a wrapper to perform user input
 *      validation and translation from HREG and standard key
 *      values into the internal format
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - registry key obtained from NR_RegGetKey(),
 *               or one of the standard top-level keys
 *    path     - relative path of key to be added.  Intermediate
 *               nodes will also be added if necessary.
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegAddKey( HREG hReg, RKEY key, char *path, RKEY *newKey )
{
    REGERR      err;
    REGOFF      start;
    REGFILE*    reg;

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    if ( path == NULL || *path == '\0' || reg == NULL )
        return REGERR_PARAM;

    /* lock the registry file */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        /* ... don't allow additional children of ROOTKEY */
        start = nr_TranslateKey( reg, key );
        if ( start != 0 && start != reg->hdr.root )
        {
            err = nr_RegAddKey( reg, start, path, newKey, FALSE );
        }
        else
            err = REGERR_PARAM;

        /* unlock the registry */
        nr_Unlock( reg );
    }

    return err;
}   /* NR_RegAddKey */




/* ---------------------------------------------------------------------
 * NR_RegAddKeyRaw - Add a key node to the registry
 *
 *      This routine is simply a wrapper to perform user input
 *      validation and translation from HREG and standard key
 *      values into the internal format. It is different from
 *		NR_RegAddKey() in that it takes a keyname rather than
 *		a path.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - registry key obtained from NR_RegGetKey(),
 *               or one of the standard top-level keys
 *    keyname  - name of key to be added. No parsing of this
 *				 name happens.
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegAddKeyRaw( HREG hReg, RKEY key, char *keyname, RKEY *newKey )
{
    REGERR      err;
    REGOFF      start;
    REGFILE*    reg;

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    if ( keyname == NULL || *keyname == '\0' || reg == NULL )
        return REGERR_PARAM;

    /* lock the registry file */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        /* ... don't allow additional children of ROOTKEY */
        start = nr_TranslateKey( reg, key );
        if ( start != 0 && start != reg->hdr.root ) 
        {
            err = nr_RegAddKey( reg, start, keyname, newKey, TRUE );
        }
        else
            err = REGERR_PARAM;

        /* unlock the registry */
        nr_Unlock( reg );
    }

    return err;
}   /* NR_RegAddKeyRaw */




/* ---------------------------------------------------------------------
 * NR_RegDeleteKey - Delete the specified key
 *
 * Note that delete simply orphans blocks and makes no attempt
 * to reclaim space in the file. Use NR_RegPack()
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    path     - relative path of key to delete
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteKey( HREG hReg, RKEY key, char *path )
{
    REGERR      err;
    REGFILE*    reg;

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        err = nr_RegDeleteKey( reg, key, path, FALSE );
        nr_Unlock( reg );
    }

    return err;
}   /* NR_RegDeleteKey */




/* ---------------------------------------------------------------------
 * NR_RegDeleteKeyRaw - Delete the specified raw key
 *
 * Note that delete simply orphans blocks and makes no attempt
 * to reclaim space in the file. Use NR_RegPack()
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY or parent to the raw key you wish to delete
 *    keyname  - name of child key to delete
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteKeyRaw( HREG hReg, RKEY key, char *keyname )
{
    REGERR      err;
    REGFILE*    reg;

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        err = nr_RegDeleteKey( reg, key, keyname, TRUE );
        nr_Unlock( reg );
    }

    return err;
}   /* NR_RegDeleteKeyRaw */




/* ---------------------------------------------------------------------
 * NR_RegGetKey - Get the RKEY value of a node from its path
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    path     - relative path of key to find.  (a blank path just gives you
 *               the starting key--useful for verification, VersionRegistry)
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetKey( HREG hReg, RKEY key, char *path, RKEY *result )
{
    REGERR      err;
    REGOFF      start;
    REGFILE*    reg;
    REGDESC     desc;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( path == NULL || result == NULL )
        return REGERR_PARAM;

    *result = (RKEY)0;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        start = nr_TranslateKey( reg, key );
        if ( start != 0 )
        {
            /* find the specified key ( if it's valid )*/
            err = nr_Find( reg, start, path, &desc, 0, 0, FALSE );
            if ( err == REGERR_OK ) {
                *result = (RKEY)desc.location;
            }
        }
        else {
            err = REGERR_PARAM;
        }

        nr_Unlock( reg );
    }

    return err;

}   /* NR_RegGetKey */




/* ---------------------------------------------------------------------
 * NR_RegGetKeyRaw - Get the RKEY value of a node from its keyname
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    keyname  - keyname of key to find.  (a blank keyname just gives you
 *               the starting key--useful for verification, VersionRegistry)
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetKeyRaw( HREG hReg, RKEY key, char *keyname, RKEY *result )
{
    REGERR      err;
    REGOFF      start;
    REGFILE*    reg;
    REGDESC     desc;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( keyname == NULL || result == NULL )
        return REGERR_PARAM;

    *result = (RKEY)0;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        start = nr_TranslateKey( reg, key );
        if ( start != 0 )
        {
            /* find the specified key ( if it's valid )*/
            err = nr_Find( reg, start, keyname, &desc, 0, 0, TRUE );
            if ( err == REGERR_OK ) {
                *result = (RKEY)desc.location;
            }
        }
        else {
            err = REGERR_PARAM;
        }

        nr_Unlock( reg );
    }

    return err;

}   /* NR_RegGetKeyRaw */




/* ---------------------------------------------------------------------
 * NR_RegGetEntryInfo - Get some basic info about the entry data
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    info     - return: Entry info object
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntryInfo( HREG hReg, RKEY key, char *name, 
                            REGINFO *info )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;
    
    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || info == NULL )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        /* read starting desc */
        err = nr_ReadDesc( reg, key, &desc);
        if ( err == REGERR_OK ) 
        {
            /* if the named entry exists */
            err = nr_FindAtLevel( reg, desc.value, name, &desc, NULL );
            if ( err == REGERR_OK ) 
            {
                /* ... return the values */
                if ( info->size == sizeof(REGINFO) )
                {
                    info->entryType   = desc.type;
                    info->entryLength = desc.valuelen;
                }
                else
                {
                    /* uninitialized (maybe invalid) REGINFO structure */
                    err = REGERR_PARAM;
                }
            }
        }

        nr_Unlock( reg );
    }

    return err;

}   /* NR_RegGetEntryInfo */



       
/* ---------------------------------------------------------------------
 * NR_RegGetEntryString - Get the UTF string value associated with the
 *                       named entry of the specified key.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - destination for string
 *    bufsize  - size of buffer
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntryString( HREG  hReg, RKEY  key, char  *name,
                            char  *buffer, uint32 bufsize)
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL || bufsize == 0 )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        /* read starting desc */
        err = nr_ReadDesc( reg, key, &desc);
        if ( err == REGERR_OK ) 
        {
            /* if the named entry exists */
            err = nr_FindAtLevel( reg, desc.value, name, &desc, NULL );
            if ( err == REGERR_OK ) 
            {
                /* read the string */
                if ( desc.type == REGTYPE_ENTRY_STRING_UTF ) 
                {
                    err = nr_ReadData( reg, &desc, bufsize, buffer );
                    /* prevent run-away strings */
                    buffer[bufsize-1] = '\0';
                }
                else {
                    err = REGERR_BADTYPE;
                }
            }
        }

        nr_Unlock( reg );
    }

    return err;

}   /* NR_RegGetEntryString */




/* ---------------------------------------------------------------------
 * NR_RegGetEntry - Get the value data associated with the
 *                  named entry of the specified key.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - destination for data
 *    size     - in:  size of buffer
 *               out: size of actual data (incl. \0 term. for strings)
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntry( HREG hReg, RKEY key, char *name,
    void *buffer, uint32 *size )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;
    char        *tmpbuf = NULL;  /* malloc a tmp buffer to convert XP int arrays */
    uint32      nInt;
    uint32      *pISrc;
    uint32      *pIDest;
    XP_Bool     needFree = FALSE;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL || size == NULL )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    err = nr_Lock( reg );
    if ( err == REGERR_OK )
    {
        /* read starting desc */
        err = nr_ReadDesc( reg, key, &desc);
        if ( err == REGERR_OK )
        {
            /* if the named entry exists */
            err = nr_FindAtLevel( reg, desc.value, name, &desc, NULL );
            if ( err == REGERR_OK )
            {
                if ( desc.valuelen > *size ) {
                    err = REGERR_BUFTOOSMALL;
                }
                else if ( desc.valuelen == 0 ) {
                    err = REGERR_FAIL;
                }
                else switch (desc.type)
                {
                /* platform independent array of 32-bit integers */
                case REGTYPE_ENTRY_INT32_ARRAY:
                    tmpbuf = (char*)XP_ALLOC( desc.valuelen );
                    if ( tmpbuf != NULL ) 
                    {
                        needFree = TRUE;
                        err = nr_ReadData( reg, &desc, desc.valuelen, tmpbuf );
                        if ( REGERR_OK == err )
                        {
                            /* convert int array */
                            nInt = (desc.valuelen / INTSIZE);
                            pISrc = (uint32*)tmpbuf;
                            pIDest = (uint32*)buffer;
                            for(; nInt > 0; nInt--, pISrc++, pIDest++) {
                                *pIDest = nr_ReadLong((char*)pISrc);
                            }
                        }
                    }
                    else
                        err = REGERR_MEMORY;
                    break;

                case REGTYPE_ENTRY_STRING_UTF:
                    tmpbuf = (char*)buffer;
                    err = nr_ReadData( reg, &desc, *size, tmpbuf );
                    /* prevent run-away strings */
                    tmpbuf[(*size)-1] = '\0';
                    break;

                case REGTYPE_ENTRY_FILE:

                    err = nr_ReadData( reg, &desc, *size, (char*)buffer );
#ifdef XP_MAC
                    if (err == 0)
                    {
                        tmpbuf = nr_PathFromMacAlias(buffer, *size);
                        if (tmpbuf == NULL) 
                        {
                            buffer = NULL;
                            err = REGERR_NOFIND;
                        }
                        else 
                        {
                            needFree = TRUE;

                            if (XP_STRLEN(tmpbuf) < *size) /* leave room for \0 */
                                XP_STRCPY(buffer, tmpbuf);
                            else 
                                err = REGERR_BUFTOOSMALL;
                        }
                    }
#endif
                    break;
                
                case REGTYPE_ENTRY_BYTES:
                default:              /* return raw data for unknown types */
                    err = nr_ReadData( reg, &desc, *size, (char*)buffer );
                    break;
                }

                /* return the actual data size */
                *size = desc.valuelen;
            }
        }

        nr_Unlock( reg );
    }

    if (needFree)
        XP_FREE(tmpbuf);

    return err;

}   /* NR_RegGetEntry */




/* ---------------------------------------------------------------------
 * NR_RegSetEntryString - Store a UTF string value associated with the
 *                       named entry of the specified key.  Used for
 *                       both creation and update.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - UTF String to store
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegSetEntryString( HREG hReg, RKEY key, char *name,
                                     char *buffer )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;
    REGDESC     parent;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err != REGERR_OK )
        return err;

    /* read starting desc */
    err = nr_ReadDesc( reg, key, &parent);
    if ( err == REGERR_OK ) {

        /* if the named entry already exists */
        err = nr_FindAtLevel( reg, parent.value, name, &desc, NULL );
        if ( err == REGERR_OK ) {
            /* then update the existing one */
            err = nr_WriteString( reg, buffer, &desc );
            if ( err == REGERR_OK ) {
                desc.type = REGTYPE_ENTRY_STRING_UTF;
                err = nr_WriteDesc( reg, &desc );
            }
        }
        else if ( err == REGERR_NOFIND ) {
            /* otherwise create a new entry */
            err = nr_CreateEntryString( reg, &parent, name, buffer );
        }
        /* other errors fall through */
    }

    /* unlock registry */
    nr_Unlock( reg );

    return err;

}   /* NR_RegSetEntryString */




/* ---------------------------------------------------------------------
 * NR_RegSetEntry - Store value data associated with the named entry
 *                  of the specified key.  Used for both creation and update.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    type     - type of data to be stored
 *    buffer   - data to store
 *    size     - length of data to store in bytes
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegSetEntry( HREG hReg, RKEY key, char *name, uint16 type,
    void *buffer, uint32 size )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;
    REGDESC     parent;
    char        *data = NULL;
    uint32      nInt;
    uint32      *pIDest;
    uint32      *pISrc;
    XP_Bool     needFree = FALSE;
    int32       datalen = size;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL || size == 0 )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* validate type and convert numerics to XP format */
    switch (type)
    {
        case REGTYPE_ENTRY_BYTES:
            data = (char*)buffer;
            break;

        case REGTYPE_ENTRY_FILE:

#ifdef XP_MAC
            nr_MacAliasFromPath(buffer, &data, &datalen);
            if (data)
                needFree = TRUE;
#else
            data = (char*)buffer;	
#endif
            break;


        case REGTYPE_ENTRY_STRING_UTF:
            data = (char*)buffer;
            /* string must be null terminated */
            if ( data[size-1] != '\0' )
                return REGERR_PARAM;
            break;


        case REGTYPE_ENTRY_INT32_ARRAY:
            /* verify no partial integers */
            if ( (size % INTSIZE) != 0 )
                return REGERR_PARAM;

            /* get a conversion buffer */
            data = (char*)XP_ALLOC(size);
            if ( data == NULL )
                return REGERR_MEMORY;
            else
                needFree = TRUE;

            /* convert array to XP format */
            nInt = ( size / INTSIZE );
            pIDest = (uint32*)data;
            pISrc  = (uint32*)buffer;

            for( ; nInt > 0; nInt--, pIDest++, pISrc++) {
                nr_WriteLong( *pISrc, (char*)pIDest );
            }
            break;


        default:
            return REGERR_BADTYPE;
    }

    /* lock registry */
    err = nr_Lock( reg );
    if ( REGERR_OK == err )
    {
        /* read starting desc */
        err = nr_ReadDesc( reg, key, &parent);
        if ( err == REGERR_OK ) 
        {
            /* if the named entry already exists */
            err = nr_FindAtLevel( reg, parent.value, name, &desc, NULL );
            if ( err == REGERR_OK ) 
            {
                /* then update the existing one */
                err = nr_WriteData( reg, data, datalen, &desc );
                if ( err == REGERR_OK ) 
                {
                    desc.type = type;
                    err = nr_WriteDesc( reg, &desc );
                }
            }
            else if ( err == REGERR_NOFIND ) 
            {
                /* otherwise create a new entry */
                err = nr_CreateEntry( reg, &parent, name, type, data, datalen );
            }
            else {
                /* other errors fall through */
            }
        }

        /* unlock registry */
        nr_Unlock( reg );
    }

    if (needFree)
        XP_FREE(data);

    return err;

}   /* NR_RegSetEntry */




/* ---------------------------------------------------------------------
 * NR_RegDeleteEntry - Delete the named entry
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteEntry( HREG hReg, RKEY key, char *name )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;
    REGDESC     parent;
    REGOFF      offPrev;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err != REGERR_OK )
    	return err;

    /* read starting desc */
    err = nr_ReadDesc( reg, key, &parent);
    if ( err == REGERR_OK ) {

        /* look up the named entry */
        err = nr_FindAtLevel( reg, parent.value, name, &desc, &offPrev );
        if ( err == REGERR_OK ) {

            XP_ASSERT( TYPE_IS_ENTRY( desc.type ) );

            /* if entry is the head of a chain */
            if ( offPrev == 0 ) {
                /* hook parent key to next entry */
                XP_ASSERT( parent.value == desc.location );
                parent.value = desc.left;
            }
            else {
                /* otherwise hook previous entry to next */
                err = nr_ReadDesc( reg, offPrev, &parent );
                parent.left = desc.left;
            }
            /* write out changed desc for previous node */
            if ( err == REGERR_OK ) {
                err = nr_WriteDesc( reg, &parent );
                /* zap the deleted desc because an enum state may contain a
                 * reference to a specific entry node
                 */
                if ( err == REGERR_OK ) {
                    desc.type |= REGTYPE_DELETED;
                    err = nr_WriteDesc( reg, &desc );
                }
            }
        }
    }

    /* unlock registry */
    nr_Unlock( reg );

    return err;

}   /* NR_RegDeleteEntry */




/* ---------------------------------------------------------------------
 * NR_RegEnumSubkeys - Enumerate the subkey names for the specified key
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key to enumerate--obtain with NR_RegGetKey()
 *    eState   - enumerations state, must contain NULL to start
 *    buffer   - location to store subkey names.  Once an enumeration
 *               is started user must not modify contents since values
 *               are built using the previous contents.
 *    bufsize  - size of buffer for names
 *    style    - 0 returns direct child keys only, REGENUM_DESCEND
 *               returns entire sub-tree
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegEnumSubkeys( HREG hReg, RKEY key, REGENUM *state,
                                    char *buffer, uint32 bufsize, uint32 style)
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lcok registry */
    err = nr_Lock( reg );
    if ( err != REGERR_OK )
        return err;

    key = nr_TranslateKey( reg, key );

    /* verify starting key */
    err = nr_ReadDesc( reg, key, &desc);
    if ( err == REGERR_OK )
    {
        /* if in initial state and no children return now */
        if ( *state == 0 && desc.down == 0 ) 
        {
            err = REGERR_NOMORE;
        }
        else switch ( style )
        {
          case REGENUM_CHILDREN:
            *buffer = '\0';
            if ( *state == 0 ) 
            {
                /* initial state: get first child (.down) */
                err = nr_ReplaceName( reg, desc.down, buffer, bufsize, &desc );
            }
            else 
            {
                /* get sibling (.left) of current key */
                err = nr_ReadDesc( reg, *state, &desc );
                if ( err == REGERR_OK || REGERR_DELETED == err )
                {
                    /* it's OK for the current (state) node to be deleted */
                    if ( desc.left != 0 ) 
                    {
                        err = nr_ReplaceName( reg, desc.left, 
                                    buffer, bufsize, &desc );
                    }
                    else
                        err = REGERR_NOMORE;
                }
            }
            break;


          case REGENUM_DESCEND:
            if ( *state == 0 ) 
            {
                /* initial state */
                *buffer = '\0';
                err = nr_ReplaceName( reg, desc.down, buffer, bufsize, &desc );
            }
            else 
            {
                /* get last position */
                err = nr_ReadDesc( reg, *state, &desc );
                if ( REGERR_OK != err && REGERR_DELETED != err ) 
                {
                    /* it is OK for the state node to be deleted
                     * (the *next* node MUST be "live", though).
                     * bail out on any other error */
                    break;
                }

                if ( desc.down != 0 ) {
                    /* append name of first child key */
                    err = nr_CatName( reg, desc.down, buffer, bufsize, &desc );
                }
                else if ( desc.left != 0 ) {
                    /* replace last segment with next sibling */
                    err = nr_ReplaceName( reg, desc.left, 
                                buffer, bufsize, &desc );
                }
                else {
                  /* done with level, pop up as many times as necessary */
                    while ( err == REGERR_OK ) 
                    {
                        if ( desc.parent != key && desc.parent != 0 ) 
                        {
                            err = nr_RemoveName( buffer );
                            if ( err == REGERR_OK ) 
                            {
                                err = nr_ReadDesc( reg, desc.parent, &desc );
                                if ( err == REGERR_OK && desc.left != 0 ) 
                                {
                                    err = nr_ReplaceName( reg, desc.left, 
                                                buffer, bufsize, &desc );
                                    break;  /* found a node */
                                }
                            }
                        }
                        else
                            err = REGERR_NOMORE;
                    }
                }
            }
            break;


          case REGENUM_DEPTH_FIRST:
            if ( *state == 0 ) 
            {
                /* initial state */

                *buffer = '\0';
                err = nr_ReplaceName( reg, desc.down, buffer, bufsize, &desc );
                while ( REGERR_OK == err && desc.down != 0 )
                {
                    /* start as far down the tree as possible */
                    err = nr_CatName( reg, desc.down, buffer, bufsize, &desc );
                }
            }
            else 
            {
                /* get last position */
                err = nr_ReadDesc( reg, *state, &desc );
                if ( REGERR_OK != err && REGERR_DELETED != err ) 
                {
                    /* it is OK for the state node to be deleted
                     * (the *next* node MUST be "live", though).
                     * bail out on any other error */
                    break;
                }

                if ( desc.left != 0 )
                {
                    /* get sibling, then descend as far as possible */
                    err = nr_ReplaceName(reg, desc.left, buffer,bufsize,&desc);

                    while ( REGERR_OK == err && desc.down != 0 ) 
                    {
                        err = nr_CatName(reg, desc.down, buffer,bufsize,&desc);
                    }
                }
                else 
                {
                    /* pop up to parent */
                    if ( desc.parent != key && desc.parent != 0 )
                    {
                        err = nr_RemoveName( buffer );
                        if ( REGERR_OK == err )
                        {
                            /* validate parent key */
                            err = nr_ReadDesc( reg, desc.parent, &desc );
                        }
                    }
                    else 
                        err = REGERR_NOMORE;
                }
            }
            break;


          default:
            err = REGERR_PARAM;
            break;
        }
    }

    /* set enum state to current key */
    if ( err == REGERR_OK ) {
        *state = desc.location;
    }

    /* unlock registry */
    nr_Unlock( reg );

    return err;

}   /* NR_RegEnumSubkeys */




/* ---------------------------------------------------------------------
 * NR_RegEnumEntries - Enumerate the entry names for the specified key
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    eState   - enumerations state, must contain NULL to start
 *    buffer   - location to store entry names
 *    bufsize  - size of buffer for names
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegEnumEntries( HREG hReg, RKEY key, REGENUM *state,
                            char *buffer, uint32 bufsize, REGINFO *info )
{
    REGERR      err;
    REGFILE*    reg;
    REGDESC     desc;

    XP_ASSERT( regStartCount > 0 );

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lcok registry */
    err = nr_Lock( reg );
    if ( err != REGERR_OK )
        return err;
    
    /* verify starting key */
    err = nr_ReadDesc( reg, key, &desc);
    if ( err == REGERR_OK )
    {
        if ( *state == 0 ) 
        {
            /* initial state--get first entry */
            if ( desc.value != 0 ) 
            {
                *buffer = '\0';
                err =  nr_ReplaceName( reg, desc.value, buffer, bufsize, &desc );
            }
            else  
            { 
                /* there *are* no entries */
                err = REGERR_NOMORE;
            }
        }
        else 
        {
            /* 'state' stores previous entry */
            err = nr_ReadDesc( reg, *state, &desc );
            if ( err == REGERR_OK  || err == REGERR_DELETED ) 
            {
                /* get next entry in chain */
                if ( desc.left != 0 ) 
                {
                    *buffer = '\0';
                    err =  nr_ReplaceName( reg, desc.left, buffer, bufsize, &desc );
                }
                else 
                {
                    /* at end of chain */
                    err = REGERR_NOMORE;
                }
            }
        }

        /* if we found an entry */
        if ( err == REGERR_OK ) 
        {
            /* set enum state to current entry */
            *state = desc.location;

            /* return REGINFO if requested */
            if ( info != NULL && info->size >= sizeof(REGINFO) ) 
            {
                info->entryType   = desc.type;
                info->entryLength = desc.valuelen;
            }
        }
    }

    /* unlock registry */
    nr_Unlock( reg );

    return err;

}   /* NR_RegEnumEntries */





/* --------------------------------------------------------------------
 * Registry Packing
 * --------------------------------------------------------------------
 */
#ifndef STANDALONE_REGISTRY
#include "VerReg.h"

static REGERR nr_createTempRegName( char *filename, uint32 filesize );
static REGERR nr_addNodesToNewReg( HREG hReg, RKEY rootkey, HREG hRegNew, void *userData, nr_RegPackCallbackFunc fn );
/* -------------------------------------------------------------------- */
static REGERR nr_createTempRegName( char *filename, uint32 filesize )
{
    struct stat statbuf;
    Bool nameFound = FALSE;
    char tmpname[MAX_PATH];
    uint32 len;
    int err;

    XP_STRCPY( tmpname, filename );
    len = XP_STRLEN(tmpname);
    if (len < filesize) {
        tmpname[len-1] = '~';
        tmpname[len] = '\0';
        remove(tmpname);
        if ( stat(tmpname, &statbuf) != 0 )
            nameFound = TRUE;
    }
    len++;
    while (!nameFound && len < filesize ) {
        tmpname[len-1] = '~';
        tmpname[len] = '\0';
        remove(tmpname);
        if ( stat(tmpname, &statbuf) != 0 )
            nameFound = TRUE;
        else
            len++;
    }  
    if (nameFound) {
        XP_STRCPY(filename, tmpname);
        err = REGERR_OK;
    } else {
        err = REGERR_FAIL;
    }
   return err;
}

static REGERR nr_addNodesToNewReg( HREG hReg, RKEY rootkey, HREG hRegNew, void *userData, nr_RegPackCallbackFunc fn )
{
    char keyname[MAXREGPATHLEN+1] = {0};
    char entryname[MAXREGPATHLEN+1] = {0};
    void *buffer;
    uint32 bufsize = 2024;
    uint32 datalen;
    REGENUM state = 0;
    REGENUM entrystate = 0;
    REGINFO info;
	int err = REGERR_OK;
    int status = REGERR_OK;
    RKEY key;
    RKEY newKey;
    REGFILE* reg;
    REGFILE* regNew;
    static int32 cnt = 0;
    static int32 prevCnt = 0;

    reg = ((REGHANDLE*)hReg)->pReg;
    regNew = ((REGHANDLE*)hRegNew)->pReg;

    buffer = XP_ALLOC(bufsize);
    if ( buffer == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }

    while (err == REGERR_OK)
    {
        err = NR_RegEnumSubkeys( hReg, rootkey, &state, keyname, sizeof(keyname), REGENUM_DESCEND );
        if ( err != REGERR_OK )
    	    break;
        err = NR_RegAddKey( hRegNew, rootkey, keyname, &newKey );
        if ( err != REGERR_OK )
    	    break;
        cnt++;
        if (cnt >= prevCnt + 15) 
        {
            fn(userData, regNew->hdr.avail, reg->hdr.avail);
            prevCnt = cnt;
        }
        err = NR_RegGetKey( hReg, rootkey, keyname, &key );
        if ( err != REGERR_OK )
    	    break;
        entrystate = 0;
        status = REGERR_OK;
        while (status == REGERR_OK) {
	        info.size = sizeof(REGINFO);
            status = NR_RegEnumEntries( hReg, key, &entrystate, entryname, 
                                        sizeof(entryname), &info );
            if ( status == REGERR_OK ) {
                XP_ASSERT( bufsize >= info.entryLength );
                datalen = bufsize;
                status = NR_RegGetEntry( hReg, key, entryname, buffer, &datalen );
                XP_ASSERT( info.entryLength == datalen );
                if ( status == REGERR_OK ) {
                    /* copy entry */
                    status = NR_RegSetEntry( hRegNew, newKey, entryname, 
                                info.entryType, buffer, info.entryLength );
                }
            } 
        }
        if ( status != REGERR_NOMORE ) {
            /* pass real error to outer loop */
            err = status;
        }
    }

    if ( err == REGERR_NOMORE )
        err = REGERR_OK;

    XP_FREEIF(buffer);
    return err;

}


/* ---------------------------------------------------------------------
 * NR_RegPack    - Pack an open registry.  
 *                Registry is locked the entire time.
 *
 * Parameters:
 *    hReg     - handle of open registry to pack
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegPack( HREG hReg, void *userData, nr_RegPackCallbackFunc fn)
{
    XP_File  fh;
    REGFILE* reg;
    HREG hRegTemp;
    char tempfilename[MAX_PATH] = {0};
    char oldfilename[MAX_PATH] = {0};

    XP_Bool bCloseTempFile = FALSE;

    int err = REGERR_OK;
    RKEY key;

    XP_ASSERT( regStartCount > 0 );
    if ( regStartCount <= 0 )
        return REGERR_FAIL;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* lock registry */
    err = nr_Lock( reg );
    if ( err != REGERR_OK )
    	return err; 

    PR_Lock(reglist_lock); 
    XP_STRCPY(tempfilename, reg->filename);
    err = nr_createTempRegName(tempfilename, sizeof(tempfilename));
    if ( err != REGERR_OK )
    	goto safe_exit; 
     
    /* force file creation */
    fh = vr_fileOpen(tempfilename, XP_FILE_WRITE_BIN);
    if ( !VALID_FILEHANDLE(fh) ) {
        err = REGERR_FAIL;
        goto safe_exit;
    }
    XP_FileClose(fh);

    err = NR_RegOpen(tempfilename, &hRegTemp);
    if ( err != REGERR_OK )
    	goto safe_exit;
    bCloseTempFile = TRUE;

    /* must open temp file first or we get the same name twice */
    XP_STRCPY(oldfilename, reg->filename);
    err = nr_createTempRegName(oldfilename, sizeof(oldfilename));
    if ( err != REGERR_OK )
    	goto safe_exit; 
     
    key = ROOTKEY_PRIVATE;
    err = nr_addNodesToNewReg( hReg, key, hRegTemp, userData, fn);
    if ( err != REGERR_OK  )
    	goto safe_exit;
    key = ROOTKEY_VERSIONS;
    err = nr_addNodesToNewReg( hReg, key, hRegTemp, userData, fn);
    if ( err != REGERR_OK  )
    	goto safe_exit;
    key = ROOTKEY_COMMON;
    err = nr_addNodesToNewReg( hReg, key, hRegTemp, userData, fn);
    if ( err != REGERR_OK  )
        goto safe_exit;
    key = ROOTKEY_USERS;
    err = nr_addNodesToNewReg( hReg, key, hRegTemp, userData, fn);
    if ( err != REGERR_OK  )
    	goto safe_exit;

    err = NR_RegClose(hRegTemp);
    bCloseTempFile = FALSE;
  
    /* close current reg file so we can rename it */
    XP_FileClose(reg->fh);
   
    /* rename current reg file out of the way */
    err = nr_RenameFile(reg->filename, oldfilename);
    if ( err == -1 ) {
        /* rename failed, get rid of the new registry and reopen the old one*/
        remove(tempfilename);
        reg->fh = vr_fileOpen(reg->filename, XP_FILE_UPDATE_BIN);
        goto safe_exit;
    }

    /* rename packed registry to the correct name */
    err = nr_RenameFile(tempfilename, reg->filename);
    if ( err == -1 ) {
        /* failure, recover original registry */
        err = nr_RenameFile(oldfilename, reg->filename);
        remove(tempfilename);
        reg->fh = vr_fileOpen(reg->filename, XP_FILE_UPDATE_BIN);
        goto safe_exit;
    
    } else {
        remove(oldfilename); 
    }
    reg->fh = vr_fileOpen(reg->filename, XP_FILE_UPDATE_BIN);

safe_exit:
    if ( bCloseTempFile ) {
        NR_RegClose(hRegTemp);
    }
    PR_Unlock( reglist_lock );
    nr_Unlock(reg);
    return err;

}

#ifdef XP_MAC
#pragma export reset
#endif

#endif /* STANDALONE_REGISTRY */






/* ---------------------------------------------------------------------
 * ---------------------------------------------------------------------
 * Registry initialization and shut-down
 * ---------------------------------------------------------------------
 * ---------------------------------------------------------------------
 */

#include "VerReg.h"

#ifndef STANDALONE_REGISTRY
extern PRMonitor *vr_monitor;
#endif 



#ifdef XP_UNIX
extern XP_Bool bGlobalRegistry;
#endif

#ifdef XP_MAC
#pragma export on
#endif

VR_INTERFACE(REGERR) NR_StartupRegistry(void)
{
    REGERR status = REGERR_OK;
    HREG reg;

#ifndef STANDALONE_REGISTRY
    if ( reglist_lock == NULL ) {
        reglist_lock = PR_NewLock();
    }

    if ( reglist_lock != NULL ) {
        PR_Lock( reglist_lock );
    }
    else {
        XP_ASSERT( reglist_lock );
        status = REGERR_FAIL;
    }
#endif

    if ( status == REGERR_OK )
    {
        if ( ++regStartCount == 1 )
        {
            /* first time only initialization */

            vr_findGlobalRegName();

            /* check to see that we have a valid registry */
            /* or create one if it doesn't exist */
            if (REGERR_OK == nr_RegOpen("", &reg))
            {
                nr_RegClose(reg);
            }

            /* initialization for version registry */
#ifndef STANDALONE_REGISTRY
            vr_monitor = PR_NewMonitor();
            XP_ASSERT( vr_monitor != NULL );
#endif 

#ifdef XP_UNIX
            bGlobalRegistry = ( getenv(UNIX_GLOBAL_FLAG) != NULL );
#endif

        } /* if ( regStartCount == 1 ) */

        PR_Unlock( reglist_lock );
    }

    return status;
}   /* NR_StartupRegistry */

VR_INTERFACE(void) NR_ShutdownRegistry(void)
{
    REGFILE* pReg;

#ifndef STANDALONE_REGISTRY
    /* people should track whether NR_StartupRegistry() was successful
     * and not call this if it fails... but they won't so we'll try to
     * handle that case gracefully.
     */
    if ( reglist_lock == NULL ) 
        return;  /* was not started successfully */
#endif

    PR_Lock( reglist_lock );

    --regStartCount;
    if ( regStartCount == 0 )
    {
        /* shutdown for real. */
        /* Close version registry first */
#ifndef STANDALONE_REGISTRY
        if ( vr_monitor != NULL ) 
        {
            VR_Close();
            PR_DestroyMonitor(vr_monitor);
            vr_monitor = NULL;
        }
#endif 

        /* close any forgotten open registries */
        while ( RegList != NULL ) 
        {
            pReg = RegList;
            if ( pReg->hdrDirty ) {
                nr_WriteHdr( pReg );
            }
            nr_CloseFile( &(pReg->fh) );
            nr_DeleteNode( pReg );
        }
    
        XP_FREEIF(user_name);
        XP_FREEIF(globalRegName);
    }

    PR_Unlock( reglist_lock );

#ifndef STANDALONE_REGISTRY    
    if ( regStartCount == 0 ) 
    {
        PR_DestroyLock( reglist_lock );
        reglist_lock = NULL;
    }
#endif

}   /* NR_ShutdownRegistry */

#ifdef XP_MAC
#pragma export reset
#endif

/* EOF: reg.c */
