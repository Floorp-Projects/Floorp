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

#ifdef XP_MAC
#include <size_t.h>
#endif
	
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef SUNOS4
  #include <unistd.h>  /* for SEEK_SET */
#endif /* SUNOS4 */

#include "reg.h"
#include "NSReg.h"


/* NOTE! It is EXREMELY important that node names be in UTF-8; otherwise
 * backwards path search for delim char will fail for multi-byte/Unicode names.
 * SmartUpdate will also break, as it relies on Java's UTF-8 -> Unicode
 * conversion, which will fail if the text is not actually valid UTF-8
 */

/* ====================================================================
 * Overview
 * --------------------------------------------------------------------
 *
 * Layers:
 *		Interface
 *			Path Parsing
 *			Key/Entry Management
 *				Block I/O
 *					Virtual I/O
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
 * --------------------------------------------------------------------
 */

static REGFILE *RegList = NULL;
static XP_Bool bRegStarted = FALSE;
#if !defined(STANDALONE_REGISTRY)
static PRMonitor *reglist_monitor;
#endif
static char *user_name = NULL;


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
    if ( pReg->monitor != NULL )
        PR_DestroyMonitor( pReg->monitor );
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
 *	Platform-specifics go in this section
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



static REGERR nr_OpenFile(char *path, FILEHANDLE *fh)
{
	XP_ASSERT( path != NULL );
    XP_ASSERT( fh != NULL );

	/* Open the file for exclusive random read/write */
	(*fh) = XP_FileOpen(path, xpRegistry, XP_FILE_UPDATE_BIN);
	if ( !VALID_FILEHANDLE(*fh) )
	{
		switch (errno)
		{
		case ENOENT:	/* file not found */
			return REGERR_NOFILE;

		case EACCES:	/* file in use */
            /* DVNOTE: should we try read only? */
    	    (*fh) = XP_FileOpen(path, xpRegistry, XP_FILE_READ_BIN);
	        if ( VALID_FILEHANDLE(*fh) )
                return REGERR_READONLY;
            else
                return REGERR_FAIL;
                
		case EMFILE:	/* too many files open */
		default:
			return REGERR_FAIL;
		}
	}

	return REGERR_OK;

}	/* OpenFile */



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
    		if (errno == EBADF)	/* bad file handle, not open for read, etc. */
			    err = REGERR_FAIL;
		    else
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

}	/* ReadFile */



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

}	/* WriteFile */



static REGERR nr_LockRange(FILEHANDLE fh, REGOFF offset, int32 len)
{
	/* TODO: Implement XP lock function with built-in retry. */

	return REGERR_OK;

}	/* LockRange */



static REGERR nr_UnlockRange(FILEHANDLE fh, REGOFF offset, int32 len)
{
	/* TODO: Implement XP unlock function with built-in retry. */

	XP_FileFlush( fh );
    return REGERR_OK;

}	/* UnlockRange */



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

}	/* GetFileLength */
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
	REGERR err;
    char hdrBuf[sizeof(REGHDR)];

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
	err = nr_WriteFile(reg->fh, 0, sizeof(REGHDR), &hdrBuf);

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

	REGERR err;

	/* lock header */
	err = nr_LockRange(reg->fh, 0, sizeof(REGHDR));
	if (err != REGERR_OK)
		return err;

#ifndef STANDALONE_REGISTRY
    PR_EnterMonitor( reg->monitor );
#endif

	return nr_ReadHdr(reg);

}	/* Lock */



static REGERR nr_Unlock(REGFILE *reg)
{
#ifndef STANDALONE_REGISTRY
    PR_ExitMonitor( reg->monitor );
#endif

	return nr_UnlockRange(reg->fh, 0, sizeof(REGHDR));
}	/* Unlock */



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
            desc->valuebuf  = nr_ReadShort( descBuf + DESC_VALUEBUF );
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

}	/* ReadDesc */



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

}	/* nr_AppendName */



static REGERR nr_WriteString(REGFILE *reg, char *string, REGDESC *desc)
{
	uint32 len;

	XP_ASSERT(string);
    if (reg->readOnly)
        return REGERR_READONLY;
	len = XP_STRLEN(string) + 1;

    return nr_WriteData( reg, string, len, desc );

}	/* nr_WriteString */



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

}	/* nr_WriteData */



static REGERR nr_AppendString(REGFILE *reg, char *string, REGDESC *desc)
{
	uint32 len;

	XP_ASSERT(string);
    if (reg->readOnly)
        return REGERR_READONLY;
	len = XP_STRLEN(string) + 1;

    return nr_AppendData( reg, string, len, desc );

}	/* nr_AppendString */



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
    REGDESC *pDesc,	REGOFF *pPrev, REGOFF *pParent);

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
			REGOFF *pParent)
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

   	/* Walk 'path', reading keys into 'desc' */
    p = pPath;
    while ( err == REGERR_OK ) {

        err = nr_NextName(p, namebuf, sizeof(namebuf), &p);

        if ( err == REGERR_OK ) {
            /* save current location as parent of next segment */
            offParent = desc.location;
            /* look for name at next level down */
            err = nr_FindAtLevel(reg, desc.down, namebuf, &desc, &offPrev);
        }
  	}

    if ( err == REGERR_NOMORE ) {
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

}	/* nr_Find */


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

}	/* nr_CreateSubKey */



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

}	/* nr_CreateEntryString */



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
	err = nr_Find((REGOFF)key, path, &desc, 0, 0);
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
static REGERR nr_RegAddKey( REGFILE *reg, RKEY key, char *path, RKEY *newKey );
static void   nr_Upgrade_1_1( REGFILE *reg );
static char*  nr_GetUsername();

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
                            err = nr_RegAddKey( reg, reg->rkeys.users, profName, &userkey );
                        }
                        XP_FREE(profName);
                    }
                    else {
                        err = nr_RegAddKey( reg, reg->rkeys.users, "default", &userkey );
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
    char        *profName;

    XP_ASSERT( reg != NULL );

    /* initialize to invalid key values */
    memset( &reg->rkeys, 0, sizeof(STDNODES) );

    /* Add each key before looking it up.  Adding an already
     * existing key is harmless, and these MUST exist.
     */

    /* ROOTKEY_USERS */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_USERS_STR, &key );
    if ( err == REGERR_OK ) {
        reg->rkeys.users = key;
    }

    /* ROOTKEY_COMMON */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_COMMON_STR, &key );
    if ( err == REGERR_OK ) {
        reg->rkeys.common = key;
    }

    /* ROOTKEY_VERSIONS */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_VERSIONS_STR, &key );
    if ( err == REGERR_OK ) {
        reg->rkeys.versions = key;
    }

    /* ROOTKEY_CURRENT_USER */
#if 0 /* delay until first use */ /*#ifndef STANDALONE_REGISTRY */
    err = PREF_CopyDefaultCharPref( "profile.name", &profName );

    if (err == PREF_NOERROR ) {
        err = nr_RegAddKey( reg, reg->rkeys.users, profName, &key );
        XP_FREE(profName);
    }
    else {
        err = nr_RegAddKey( reg, reg->rkeys.users, "default", &key );
    }
    if ( err == REGERR_OK ) {
        reg->rkeys.current_user = key;
    }
#endif

    /* ROOTKEY_PRIVATE */
    err = nr_RegAddKey( reg, reg->hdr.root, ROOTKEY_PRIVATE_STR, &key );
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



static REGERR nr_RegAddKey( REGFILE *reg, RKEY key, char *path, RKEY *newKey )
{
    REGERR      err;
    REGDESC     desc;
    char        namebuf[MAXREGNAMELEN];
    char        *p;

    XP_ASSERT( path != NULL );
    XP_ASSERT( *path != '\0' );
    XP_ASSERT( key >= HDRRESERVE );
    XP_ASSERT( VALID_FILEHANDLE( reg->fh ) );

    /* lock registry */
	err = nr_Lock( reg );
	if ( err != REGERR_OK ) {
    	return err;
    }

	/* Get starting desc */
	err = nr_ReadDesc( reg, key, &desc );

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

    XP_ASSERT( err != REGERR_OK );
    /* it's good to have processed the whole path */
    if ( err == REGERR_NOMORE ) {
        err = REGERR_OK;

        /* return new key if the caller wants it */
        if ( newKey != NULL ) {
            *newKey = desc.location;
        }
    }

    /* unlock registry */
	nr_Unlock(reg);

	return err;

}   /* nr_RegAddKey */



static void nr_Upgrade_1_1(REGFILE *reg)
{
	REGDESC desc;
    REGOFF  level;
	REGERR err;

	XP_ASSERT(reg);

    /* find offset for start of the top-level nodes */
    err = nr_ReadDesc( reg, reg->hdr.root, &desc);
    if ( err != REGERR_OK ) {
        return;
    }
    level = desc.down;

    /* ROOTKEY_VERSIONS */
    err = nr_FindAtLevel( reg, level, OLD_VERSIONS_STR, &desc, NULL );
    if ( err == REGERR_NOFIND ) {
        err = REGERR_OK;
    }
    else if ( REGERR_OK == err ) {
        err = nr_AppendName( reg, ROOTKEY_VERSIONS_STR, &desc );
        if ( REGERR_OK == err ) {
            err = nr_WriteDesc( reg, &desc );
        }
    }
    if ( err != REGERR_OK )
        return;

    /* ROOTKEY_USERS */
    err = nr_FindAtLevel( reg, level, OLD_USERS_STR, &desc, NULL );
    if ( err == REGERR_NOFIND ) {
        err = REGERR_OK;
    }
    else if ( REGERR_OK == err ) {
        err = nr_AppendName( reg, ROOTKEY_USERS_STR, &desc );
        if ( REGERR_OK == err ) {
            err = nr_WriteDesc( reg, &desc );
        }
    }
    if ( err != REGERR_OK )
        return;

    /* ROOTKEY_COMMON */
    err = nr_FindAtLevel( reg, level, OLD_COMMON_STR, &desc, NULL );
    if ( err == REGERR_NOFIND ) {
        err = REGERR_OK;
    }
    else if ( REGERR_OK == err ) {
        err = nr_AppendName( reg, ROOTKEY_COMMON_STR, &desc );
        if ( REGERR_OK == err ) {
            err = nr_WriteDesc( reg, &desc );
        }
    }
    if ( err != REGERR_OK )
        return;

    /* Mark registry with new version */
    reg->hdr.verMinor = MINOR_VERSION;
    nr_WriteHdr(reg);

}

static char *nr_GetUsername()
{
  if (NULL == user_name) {
    return "default";
  } else {
    return user_name;
  }
}

/* ---------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

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
  char *tmp = XP_STRDUP(name);
  if (NULL == tmp) {
    return REGERR_MEMORY;
  }
  
  XP_FREEIF(user_name);
  user_name = tmp;
  
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
    REGFILE   *pReg;
    REGHANDLE *pHandle;

    XP_ASSERT(bRegStarted);

    /* initialize output handle in case of error */
    *hReg = NULL;

#if !defined(STANDALONE_REGISTRY)
    PR_EnterMonitor(reglist_monitor);
#endif

    /* Look for named file in list of open registries */
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
            XP_FREE( pReg );
            goto bail;
        }

        /* ...read and validate the header */
        status = nr_ReadHdr( pReg );
        if ( status != REGERR_OK ) {
            nr_CloseFile( &(pReg->fh) );
            XP_FREE( pReg );
            goto bail;
        }

        /* ...other misc initialization */
        pReg->refCount = 0;

#ifndef STANDALONE_REGISTRY
        pReg->monitor = PR_NewMonitor();
#endif
        /* now done with everything that needs to protect the header */
        pReg->inInit = FALSE;

        if ( pReg->hdr.verMajor == 1 && pReg->hdr.verMinor <= 1 )
            nr_Upgrade_1_1(pReg);

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
#if !defined(STANDALONE_REGISTRY)
    PR_ExitMonitor(reglist_monitor);
#endif

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
    REGERR      err;
    REGHANDLE*  reghnd = (REGHANDLE*)hReg;

    XP_ASSERT(bRegStarted);

    /* verify handle */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    XP_ASSERT( VALID_FILEHANDLE(reghnd->pReg->fh) );

#if !defined(STANDALONE_REGISTRY)
    PR_EnterMonitor(reglist_monitor);
#endif
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
#if !defined(STANDALONE_REGISTRY)
    PR_ExitMonitor(reglist_monitor);
#endif

    reghnd->magic = 0;    /* prevent accidental re-use */
    XP_FREE( reghnd );

    return REGERR_OK;

}   /* NR_RegClose */



/* ---------------------------------------------------------------------
 * NR_RegPack    - Pack an open registry.  
 *                Registry is locked the entire time.
 *
 * Parameters:
 *    hReg     - handle of open registry to pack
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegPack( HREG hReg )
{
    XP_ASSERT(bRegStarted);

    return REGERR_FAIL;
}



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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    start = nr_TranslateKey( reg, key );

    /* ... don't allow additional children of ROOTKEY */
    if ( start == reg->hdr.root )
        return REGERR_PARAM;

    if ( path == NULL || *path == '\0' || start == 0 || reg == NULL )
        return REGERR_PARAM;

    /* call the real routine */
    return nr_RegAddKey( reg, start, path, newKey );
}   /* NR_RegAddKey */



/* ---------------------------------------------------------------------
 * NR_RegDeleteKey - Delete the specified key and all of its children
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
    REGOFF      start;
    REGFILE*    reg;
    REGDESC     desc;
    REGDESC     predecessor;
	REGOFF      offPrev;
	REGOFF      offParent;
    REGOFF*     link;

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    start = nr_TranslateKey( reg, key );
    if ( path == NULL || *path == '\0' || start == 0 )
        return REGERR_PARAM;

    /* lock registry */
	err = nr_Lock( reg );
	if ( err != REGERR_OK )
    	return err;

    /* find the specified key */
	err = nr_Find( reg, start, path, &desc, &offPrev, &offParent );
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

    /* unlock registry */
	nr_Unlock(reg);

	return err;

}   /* NR_RegDeleteKey */



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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;
    start = nr_TranslateKey( reg, key );

    if ( path == NULL || start == 0 || result == NULL )
        return REGERR_PARAM;

    /* find the specified key */
	err = nr_Find( reg, start, path, &desc, 0, 0 );
    if ( err == REGERR_OK ) {
    	*result = (RKEY)desc.location;
    }

	return err;

}   /* NR_RegGetKey */



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
    
    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || info == NULL )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* read starting desc */
	err = nr_ReadDesc( reg, key, &desc);
    if ( err == REGERR_OK ) {

        /* if the named entry exists */
        err = nr_FindAtLevel( reg, desc.value, name, &desc, NULL );
        if ( err == REGERR_OK ) {

            /* ... return the values */
            if ( info->size != sizeof(REGINFO) )
                err = REGERR_PARAM;
            else {
                info->entryType   = desc.type;
                info->entryLength = desc.valuelen;
            }
        }
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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL || bufsize == 0 )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    if ( name == NULL || *name == '\0' || buffer == NULL || size == NULL )
        return REGERR_PARAM;

    reg = ((REGHANDLE*)hReg)->pReg;

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


                case REGTYPE_ENTRY_BYTES:
                default:              /* return raw data for unknown types */
                    err = nr_ReadData( reg, &desc, *size, (char*)buffer );
                    break;
            }
            
            /* return the actual data size */
            *size = desc.valuelen;
        }
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

    XP_ASSERT(bRegStarted);

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
    char        *data;
    uint32      nInt;
    uint32      *pIDest;
    uint32      *pISrc;
    XP_Bool     needFree = FALSE;

    XP_ASSERT(bRegStarted);

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
                err = nr_WriteData( reg, data, size, &desc );
                if ( err == REGERR_OK ) 
                {
                    desc.type = type;
                    err = nr_WriteDesc( reg, &desc );
                }
            }
            else if ( err == REGERR_NOFIND ) 
            {
                /* otherwise create a new entry */
                err = nr_CreateEntry( reg, &parent, name, type, data, size );
            }
            else {
                /* other errors fall through */
            }
        }
    }

    /* unlock registry */
    nr_Unlock( reg );

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

    XP_ASSERT(bRegStarted);

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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    key = nr_TranslateKey( reg, key );

    /* verify starting key */
	err = nr_ReadDesc( reg, key, &desc);
    if ( err != REGERR_OK )
        return err;

    /* if in initial state */
    if ( *state == 0 ) {

        /* get first subkey */
        if ( desc.down != 0 ) {
            *buffer = '\0';
            err = nr_ReplaceName( reg, desc.down, buffer, bufsize, &desc );
        }
        else  {
            /* there *are* no child keys */
            err = REGERR_NOMORE;
        }
    }
    /* else already enumerating so get current 'state' node */
    else if ( REGERR_OK == (err = nr_ReadDesc( reg, *state, &desc )) ) {

        /* if traversing the tree */
        if ( style & REGENUM_DESCEND ) {
            if ( desc.down != 0 ) {
                /* append name of first child key */
                err = nr_CatName( reg, desc.down, buffer, bufsize, &desc );
            }
            else if ( desc.left != 0 ) {
                /* replace last segment with next sibling */
                err = nr_ReplaceName( reg, desc.left, buffer, bufsize, &desc );
            }
            else {
                /* done with level, pop up as many times as necessary */
                while ( err == REGERR_OK ) {
                    if ( desc.parent != key && desc.parent != 0 ) {
                        err = nr_RemoveName( buffer );
                        if ( err == REGERR_OK ) {
                            err = nr_ReadDesc( reg, desc.parent, &desc );
                            if ( err == REGERR_OK && desc.left != 0 ) {
                                err = nr_ReplaceName( reg, desc.left, buffer,
                                                      bufsize, &desc );
                                break;  /* found a node */
                            }
                        }
                    }
                    else {
                        err = REGERR_NOMORE;
                    }
                }
            }
        }
        /* else immediate child keys only */
        else {
            /* get next key in chain */
            if ( desc.left != 0 ) {
                *buffer = '\0';
                err =  nr_ReplaceName( reg, desc.left, buffer, bufsize, &desc );
            }
            else {
                err = REGERR_NOMORE;
            }
        }
    }

    /* set enum state to current key */
    if ( err == REGERR_OK ) {
        *state = desc.location;
    }

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

    XP_ASSERT(bRegStarted);

    /* verify parameters */
    err = VERIFY_HREG( hReg );
    if ( err != REGERR_OK )
        return err;

    reg = ((REGHANDLE*)hReg)->pReg;

    /* verify starting key */
	err = nr_ReadDesc( reg, key, &desc);
    if ( err != REGERR_OK )
        return err;

    if ( *state == 0 ) {
        /* initial state--get first entry */

        if ( desc.value != 0 ) {
            *buffer = '\0';
            err =  nr_ReplaceName( reg, desc.value, buffer, bufsize, &desc );
        }
        else  { 
            /* there *are* no entries */
            err = REGERR_NOMORE;
        }
    }
    else {
        /* 'state' stores previous entry */
        err = nr_ReadDesc( reg, *state, &desc );
        if ( err == REGERR_OK ) {

            /* get next entry in chain */
            if ( desc.left != 0 ) {
                *buffer = '\0';
                err =  nr_ReplaceName( reg, desc.left, buffer, bufsize, &desc );
            }
            else {
                /* at end of chain */
                err = REGERR_NOMORE;
            }
        }
    }

    /* if we found an entry */
    if ( err == REGERR_OK ) {

        /* set enum state to current entry */
        *state = desc.location;

        /* return REGINFO if requested */
        if ( info != NULL && info->size >= sizeof(REGINFO) ) {
            info->entryType   = desc.type;
            info->entryLength = desc.valuelen;
        }
    }

    return err;

}   /* NR_RegEnumEntries */




#ifndef STANDALONE_REGISTRY
#include "VerReg.h"

/* ---------------------------------------------------------------------
 * ---------------------------------------------------------------------
 * Registry initialization and shut-down
 * ---------------------------------------------------------------------
 * ---------------------------------------------------------------------
 */

extern PRMonitor *vr_monitor;
#ifdef XP_UNIX
extern XP_Bool bGlobalRegistry;
#endif

#ifdef XP_MAC
#pragma export on
#endif

VR_INTERFACE(void) NR_StartupRegistry(void)
{
    HREG reg;

    if (bRegStarted)
        return;

    vr_monitor = PR_NewMonitor();
    XP_ASSERT( vr_monitor != NULL );
    reglist_monitor = PR_NewMonitor();
    XP_ASSERT( reglist_monitor != NULL );
#ifdef XP_UNIX
    bGlobalRegistry = ( getenv(UNIX_GLOBAL_FLAG) != NULL );
#endif

    bRegStarted = TRUE;

    /* check to see that we have a valid registry */
    if (REGERR_OK == NR_RegOpen("", &reg)) {
        NR_RegClose(reg);
    }
    else {
        /* Couldn't open -- make a VersionRegistry call to force creation */
        /* Didn't worry about this before because Netcaster was doing it */
        VR_InRegistry("/Netscape");
        VR_Close();
    }

}

VR_INTERFACE(void) NR_ShutdownRegistry(void)
{
    REGFILE* pReg;

    if ( vr_monitor != NULL ) {
        VR_Close();
        PR_DestroyMonitor(vr_monitor);
        vr_monitor = NULL;
    }

    /* close any forgotten open registries */
    while ( RegList != NULL ) {
        pReg = RegList;
    	if ( pReg->hdrDirty ) {
    		nr_WriteHdr( pReg );
        }
        nr_CloseFile( &(pReg->fh) );
        nr_DeleteNode( pReg );
    }
    
    if ( reglist_monitor != NULL ) {
        PR_DestroyMonitor( reglist_monitor );
        reglist_monitor = NULL;
    }

    XP_FREEIF(user_name);

    bRegStarted = FALSE;
}

#ifdef XP_MAC
#pragma export reset
#endif

#endif /* STANDALONE_REGISTRY */

/* EOF: reg.c */
