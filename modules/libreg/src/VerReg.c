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
 * VerReg.c
 * XP Version Registry functions (prototype)
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * Install 'Navigator' produces a tree of:
 *
 *		/Components/Netscape/Web/Navigator/
 *				...Path="c:\netscape\program\netscape.exe"
 *				...Version=4.0.0.0
 *
 * --------------------------------------------------------------------
 */
#include <fcntl.h>
#include <errno.h>

#if defined(XP_WIN)
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "reg.h"
#include "NSReg.h"
#include "VerReg.h"

/* -------- local defines ---------------
*/
#define MAXREGVERLEN 32	/* Version=12345.12345.12345.12345 */

#define VERSTR	"Version"
#define CHKSTR	"Check"
#define PATHSTR	"Path"
#define DIRSTR  "Directory"
#define NAVHOME "Navigator Home"

#define VERSION_NAME            "Communicator"
#define NAVIGATOR_NODE          "/Netscape"
#define CURRENT_VER             "Current Navigator"

#define PATH_ROOT(p)   ( ((p) && *(p)==PATHDEL) ? ROOTKEY_VERSIONS : curver )
#define UNIX_ROOT(p)   ( ((p) && *(p)==PATHDEL) ? ROOTKEY_VERSIONS : unixver )


/* ---------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------
 */
static int isInited = 0;
static RKEY curver = 0;

static HREG vreg = 0;

#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
/* Extra Unix variables to deal with two registries
 *   "vreg" is always the writable registry.
 *   If "vreg" is the local registry then "unixreg" will
 *   be the global registry read-only (unless we couldn't
 *   open it).
 */
static HREG unixreg = 0;
static RKEY unixver = 0;
XP_Bool bGlobalRegistry = FALSE;
#endif

#ifndef STANDALONE_REGISTRY
PRMonitor *vr_monitor = NULL;
#endif

static char *app_dir = NULL;

/* ---------------------------------------------------------------------
 * local functions
 * ---------------------------------------------------------------------
 */
static REGERR vr_Init(void);
static REGERR vr_BuildVersion(VERSION *pVer, char *buf);
static Bool   vr_CompareDirs( char *dir1, char *dir2 );
static REGERR vr_SetCurrentNav( char *product, char *programPath, char *versionStr);
static REGERR vr_ParseVersion(char *verstr, VERSION *result);

#ifdef USE_CHECKSUM
static REGERR vr_GetCheck(char *path, int32 *check);
#endif

static REGERR vr_SetPathname(HREG reg, RKEY key, char *entry, char *dir);
static REGERR vr_GetPathname(HREG reg, RKEY key, char *entry, char *buf, uint32 sizebuf);

static REGERR vr_FindKey(char *name, HREG *hreg, RKEY *key);

#ifdef XP_MAC
static void vr_MacAliasFromPath(const char * fileName, void ** alias, int32 * length);
static char * vr_PathFromMacAlias(const void * alias, uint32 aliasLength);
#endif

/* --------------------------------------------------------------------- */

static REGERR vr_Init(void)
{

    REGERR  err = REGERR_OK;
    char    *regname = "";
    char    curstr[MAXREGNAMELEN];
    RKEY    navKey;
#ifndef STANDALONE_REGISTRY
    char    *curPath = NULL;
#ifdef XP_UNIX
    char    *regbuf = NULL;
#endif

    PR_EnterMonitor(vr_monitor);
#endif

    if (!isInited)
    {
#if !defined(STANDALONE_REGISTRY)
        curPath = app_dir;
        if (curPath == NULL) {
          /* can't use Version Registry interface until "BROKEN" works */
          err = REGERR_FAIL;
          goto done;
        }

#ifdef XP_UNIX
        if (curPath != NULL) {
            regbuf = (char*)XP_ALLOC( 10 + XP_STRLEN(curPath) );
            if (regbuf != NULL ) {
                XP_STRCPY( regbuf, curPath );
                XP_STRCAT( regbuf, "registry" );
            }
        }

        if ( curPath == NULL || regbuf == NULL ) {
            err = REGERR_MEMORY;
            goto done;
        }

        if (bGlobalRegistry)
            regname = regbuf;
#endif
#endif

        /* Open version registry */
		err = NR_RegOpen( regname, &vreg );

#ifndef STANDALONE_REGISTRY
        if (err == REGERR_NOFILE) {
            /* create it if not found */
            err = VR_CreateRegistry( VERSION_NAME, curPath, "" );
        }
        else if (err == REGERR_OK) {
            /* otherwise find/set the current nav node */
            err = vr_SetCurrentNav( VERSION_NAME, curPath, NULL );
        }

#ifdef XP_UNIX
        /* try to open shared Unix registry, but not an error if you can't */
        unixreg = NULL;
        if (!bGlobalRegistry && err == REGERR_OK ) {
            unixver = 0;
            if (NR_RegOpen( regbuf, &unixreg ) == REGERR_OK) {
                if (NR_RegGetKey( unixreg, ROOTKEY_VERSIONS, NAVIGATOR_NODE,
                    &navKey) == REGERR_OK)
                {
                    if (NR_RegGetEntryString( unixreg, navKey, CURRENT_VER,
                        curstr, sizeof(curstr)) == REGERR_OK )
                    {
                        NR_RegGetKey( unixreg, navKey, curstr, &unixver );
                    }
                }
            }
        }
#endif

		if (err == REGERR_OK) {
            /* successfully opened! */
            isInited = 1;
        }
        goto done;
#else
		if (err != REGERR_OK)
			goto done;

		/* Determine 'curver' key and ensure correct structure by adding */

        /* ...find top-level "Navigator" node (add if missing) */
		err = NR_RegAddKey( vreg, ROOTKEY_VERSIONS, NAVIGATOR_NODE, &navKey );
		if (err != REGERR_OK)
            goto done;

        /* ...look for "Current Version" entry */
        err = NR_RegGetEntryString( vreg, navKey, CURRENT_VER, curstr,
                                    sizeof(curstr) );
        if ( err == REGERR_NOFIND ) {
            /* If not found create one with the built-in version */
            err = NR_RegSetEntryString( vreg, navKey, CURRENT_VER, VERSION_NAME );
            XP_STRCPY( curstr, VERSION_NAME );
        }
        if ( err != REGERR_OK )
            goto done;

        /* ...look for "curstr" child key of the navigator node */
        err = NR_RegAddKey( vreg, navKey, curstr, &curver );

		if (err == REGERR_OK) {
            /* successfully opened! */
            isInited = 1;
        }
#endif
	}

done:
#ifndef STANDALONE_REGISTRY
    PR_ExitMonitor(vr_monitor);
#ifdef XP_UNIX
    XP_FREEIF(regbuf);
#endif
#endif
    return err;

}	/* Init */



#ifdef XP_PC
#define VR_FILE_SEP '\\'
#endif
#ifdef XP_MAC
#define VR_FILE_SEP ':'
#endif
#ifdef XP_UNIX
#define VR_FILE_SEP '/'
#endif
static Bool vr_CompareDirs( char *dir1, char *dir2 )
{
    int len1,len2;
    len1 = XP_STRLEN( dir1 );
    len2 = XP_STRLEN( dir2 );

    if ( dir1[len1-1] == VR_FILE_SEP )
        len1--;
    if ( dir2[len2-1] == VR_FILE_SEP )
        len2--;

    if ( len1 != len2 )
        return FALSE;

#ifdef XP_UNIX
    return ( XP_STRNCMP(dir1, dir2, len1) == 0 );
#else
    return ( XP_STRNCASECMP(dir1, dir2, len1) == 0 );
#endif
}


REGERR vr_ParseVersion(char *verstr, VERSION *result)
{

	result->major = result->minor = result->release = result->build = 0;
	result->major = atoi(verstr);
	while (*verstr && *verstr != '.')
		verstr++;
	if (*verstr)
	{
		verstr++;
		result->minor = atoi(verstr);
		while (*verstr && *verstr != '.')
			verstr++;
		if (*verstr)
		{
			verstr++;
			result->release = atoi(verstr);
			while (*verstr && *verstr != '.')
				verstr++;
			if (*verstr)
			{
				verstr++;
				result->build = atoi(verstr);
				while (*verstr && *verstr != '.')
					verstr++;
			}
		}
	}

	return REGERR_OK;

}	/* ParseVersion */



#ifdef USE_CHECKSUM
#define BLKSIZ 16384

static REGERR vr_GetCheck(char *path, int32 *check)
{

	int fh;
	char *buf;
	int actual;
	char *p;
	int i;
	int chk;

	XP_ASSERT(path);
	XP_ASSERT(check);
	
	*check = chk = 0;

#ifdef NEED_XP_FIXES
	/* open file for read */
	fh = open(path, O_RDONLY| O_BINARY);
	if (fh < 0)
	{
		switch (errno)
		{ 
		case ENOENT:	/* file not found */
			return REGERR_NOFILE;

		case EACCES:	/* file in use */
		case EMFILE:	/* too many files open */
		default:
			return REGERR_FAIL;
		}
	}

	buf = malloc(BLKSIZ);
	if (!buf)
	{
		close(fh);
		return REGERR_MEMORY;
	}

	do
	{
		/* read a block */
		actual = read(fh, buf, BLKSIZ);
		/* add to checksum */
		for (p=buf, i=0; i<actual; i++, p++)
			chk += *p;

		/* if the block was partial, we're done, else loop */
	} while (actual == BLKSIZ);

	/* close file */
	close(fh);
	free(buf);
#endif

	/* return calculated checksum */
	*check = chk;

	return REGERR_OK;

}	/* GetCheck */

#endif /* USE_CHECKSUM */



static REGERR vr_SetPathname(HREG reg, RKEY key, char *entry, char *dir)
{
    /* save in platform/OS charset, so use BYTES. */
    REGERR err;
    void * alias = dir;
    int32  datalen = XP_STRLEN(dir)+1; /* include '\0' */
#ifdef XP_MAC
	alias = NULL;
	vr_MacAliasFromPath(dir, &alias, &datalen);
#endif
    err = NR_RegSetEntry( reg, key, entry, REGTYPE_ENTRY_BYTES, alias, datalen);
#ifdef XP_MAC
	XP_FREEIF(alias);
#endif
    return err;
}



static REGERR vr_GetPathname(HREG reg, RKEY key, char *entry, char *buf, uint32 sizebuf)
{
    /* stored as BYTES since it's platform OS charset */
#ifndef XP_MAC
    REGERR err;
    err = NR_RegGetEntry( reg, key, entry, (void*)buf, &sizebuf );
    return err;
#else
    REGERR err;
	REGINFO info;

	info.size = sizeof(REGINFO);

	err = NR_RegGetEntryInfo( reg, key, entry,&info );
	if (err != REGERR_OK)
		return err;

	if (info.entryType == REGTYPE_ENTRY_STRING_UTF)	/* Assume that it is a file name */
		err = NR_RegGetEntry( reg, key, entry, (void*)buf, &sizebuf );
 	else
 	{
#define MAC_ALIAS_BUFFER_SIZE 4000
		char stackBuf[MAC_ALIAS_BUFFER_SIZE];
		uint32 stackBufSize = MAC_ALIAS_BUFFER_SIZE;
		char * tempBuf;

	    err = NR_RegGetEntry( reg, key, entry, (void*)stackBuf, &stackBufSize );
		if (err != REGERR_OK)
			return err;

		tempBuf = vr_PathFromMacAlias(stackBuf, stackBufSize);

		if (tempBuf == NULL)
			err = REGERR_FAIL;
		else
			if (XP_STRLEN(tempBuf) > sizebuf)
				err = REGERR_BUFTOOSMALL;
			else
				XP_STRCPY(buf, tempBuf);

		XP_FREEIF(tempBuf);
	}
	return err;
#endif
}



/* create default tree with 'installation' under Navigator */
/* set Current to the installation string */
static REGERR vr_SetCurrentNav( char *installation, char *programPath, char *versionStr)
{
    REGERR      err;
    REGENUM     state;
	RKEY        navKey;
    int         bFound;
    int         nCopy;
    char        regname[MAXREGNAMELEN];
    char        curstr[MAXREGNAMELEN];
    char        dirbuf[MAXREGNAMELEN];

	err = NR_RegAddKey( vreg, ROOTKEY_VERSIONS, NAVIGATOR_NODE, &navKey );
	if (err != REGERR_OK)
		goto done;

    /* ...look for "Current Version" entry */
    err = NR_RegGetEntryString( vreg, navKey, CURRENT_VER, curstr, sizeof(curstr));
    if ( err == REGERR_NOFIND )
    {
        /* No current installation, we can simply add a new one  */
    	err = NR_RegAddKey( vreg, navKey, installation, &curver );

        /* ... add Path and Version properties */
        if ( err == REGERR_OK ) {
            err = vr_SetPathname( vreg, curver, NAVHOME, programPath );
            if ( REGERR_OK == err && versionStr != NULL) {
                err = NR_RegSetEntryString( vreg, curver, VERSTR, versionStr );
            }
        }

        if ( REGERR_OK == err ) {
            /* successfully added, make it the current version */
            err = NR_RegSetEntryString(vreg, navKey, CURRENT_VER, installation);
        }

    	if (err != REGERR_OK)
    		goto done;
    }
    else if ( REGERR_OK == err )
    {
        /* found one: if we're lucky we got the right one */
        bFound = FALSE;
        err = NR_RegGetKey( vreg, navKey, curstr, &curver );
        if ( REGERR_OK == err ) {
            err = vr_GetPathname( vreg, curver, NAVHOME, dirbuf, sizeof(dirbuf) );
            if ( REGERR_OK == err )
                bFound = vr_CompareDirs(dirbuf, programPath);
            else if ( REGERR_NOFIND == err ) {
                /* assume this is the right one since it's 'Current' */
                err = vr_SetPathname( vreg, curver, NAVHOME, programPath );
                bFound = TRUE;
            }
        }

        /* Look for an existing installation if not found */
        state = 0;
        while (!bFound && err == REGERR_OK ) {
            err = NR_RegEnumSubkeys( vreg, navKey, &state, curstr,
                    sizeof(curstr), REGENUM_NORMAL );

            if (REGERR_OK == err ) {
                err = vr_GetPathname( vreg, state, NAVHOME, dirbuf, sizeof(dirbuf) );
                if (REGERR_OK == err ) {
                    if (vr_CompareDirs( dirbuf, programPath )) {
                        bFound = TRUE;
                        curver = (RKEY)state;
                    }
                }
                else if ( err == REGERR_NOFIND ) {
                    /* wasn't a navigator node */
                    err = REGERR_OK;
                }
            }
        }

        /* found the right one, make it current */
        if (bFound) {
            err = NR_RegSetEntryString( vreg, navKey, CURRENT_VER, curstr );
            /* update version (curver already set) */
            if ( REGERR_OK == err && versionStr != NULL ) {
                err = NR_RegSetEntryString( vreg, curver, VERSTR, versionStr );
            }
        }
        /* otherwise if no current installation matches */
        else if ( err == REGERR_NOMORE )
        {
            /* look for an empty slot to put new installation */
            nCopy = 1;
            XP_STRCPY( regname, installation );
            do {
                err = NR_RegGetKey( vreg, navKey, regname, &curver );
                if (err == REGERR_OK) {
                    nCopy++;
                    sprintf( regname, "%s #%d", installation, nCopy );
                }
            } while (err==REGERR_OK);

            if (err != REGERR_NOFIND)
                goto done;  /* real error, bail */

            /* found an unused name -- add it */
            err = NR_RegAddKey( vreg, navKey, regname, &curver );
            if ( err != REGERR_OK )
                goto done;

            /* add path and version properties */
            err = vr_SetPathname( vreg, curver, NAVHOME, programPath );
            if ( REGERR_OK == err && versionStr != NULL ) {
                err = NR_RegSetEntryString( vreg, curver, VERSTR, versionStr );
            }

            if ( REGERR_OK == err ) {
                /* everything's OK, make it current */
                err = NR_RegSetEntryString(vreg,navKey,CURRENT_VER,regname);
            }
        }
    }
done:
    return err;
}


#ifdef XP_MAC

#include <Aliases.h>
#include <TextUtils.h>
#include <Memory.h>
#include "FullPath.h"

/* returns an alias as a malloc'd pointer.
 * On failure, *alias is NULL
 */
static void vr_MacAliasFromPath(const char * fileName, void ** alias, int32 * length)
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
static char * vr_PathFromMacAlias(const void * alias, uint32 aliasLength)
{
	OSErr err;
	AliasHandle h = NULL;
	Handle fullPath = NULL;
	short fullPathLength;
	char * cpath = NULL;
	FSSpec fs;
	Boolean ignore;	/* Change flag, it would be nice to change the alias on disk
						if the file location changed */

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
	{
		short vRefNum;
		long dirID;
		err = FindFolder(fs.vRefNum, kTrashFolderType, false, &vRefNum, &dirID);
		if (err == noErr)
			if (dirID == fs.parID)	/* File is inside the trash */
				goto fail;
	}
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

/* assumes registries are open (only use after vr_Init() returns OK).
 * For UNIX look first in the global, then in the local if not found
 * -- returns both hreg and key of the named node (if found)
 */
static REGERR vr_FindKey(char *component_path, HREG *hreg, RKEY *key)
{
    REGERR err;
    RKEY rootkey;

#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    if (unixreg != NULL) {
        *hreg = unixreg;
        rootkey = UNIX_ROOT(component_path);
        err = NR_RegGetKey( *hreg, rootkey, component_path, key );
    }
    if (unixreg == NULL || err == REGERR_NOFIND )
#endif
    {
        *hreg = vreg;
    	rootkey = PATH_ROOT(component_path);
        err = NR_RegGetKey( *hreg, rootkey, component_path, key );
    }

    return err;
}


/* ---------------------------------------------------------------------
 * Interface
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) VR_PackRegistry(void)
{
    REGERR err;

    /* make sure vreg (src) is open */
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    err = NR_RegPack( vreg );

    return err;

}   /* PackRegistry */



VR_INTERFACE(REGERR) VR_CreateRegistry( char *installation, char *programPath, char *versionStr )
{
	FILEHANDLE  fh;
	REGERR      err;
	XP_StatStruct st;
    char *      regname = "";
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    char *      regbuf = NULL;
#endif

	if ( installation == NULL || *installation == '\0' )
		return REGERR_PARAM;

#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    if (bGlobalRegistry)
    {
        regbuf = (char*)XP_ALLOC( 10 + XP_STRLEN(programPath) );
        if (regbuf == NULL)
            return REGERR_MEMORY;

        XP_STRCPY( regbuf, programPath );
        XP_STRCAT( regbuf, "registry" );
        regname = regbuf;
    }
#endif

#ifdef STANDALONE_REGISTRY
    /* standalone registry automatically creates it if not found */
    fh = XP_FileOpen( regname, xpRegistry, XP_FILE_UPDATE_BIN );
#else
	if ( ( XP_Stat ( regname, &st, xpRegistry ) == 0) )
		fh = XP_FileOpen( regname, xpRegistry, XP_FILE_UPDATE_BIN );
	else
		/* create a new empty registry */
		fh = XP_FileOpen( regname, xpRegistry, XP_FILE_WRITE_BIN );
#endif

	if (fh == NULL) {
        err = REGERR_FAIL;
        goto done;
    }
	XP_FileClose(fh);

	err = NR_RegOpen( regname, &vreg );
	if (err != REGERR_OK)
		goto done;

	/* create default tree with 'installation' under Navigator */
	/* set Current to the installation string */

    err = vr_SetCurrentNav( installation, programPath, versionStr );

    if ( REGERR_OK == err )
    	isInited = 1;

done:
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    XP_FREEIF( regbuf );
#endif
	return err;

}	/* CreateRegistry */


#ifdef XP_MAC
#pragma export on
#endif

VR_INTERFACE(REGERR) VR_Close(void)
{
	REGERR err = REGERR_OK;

#ifndef STANDALONE_REGISTRY
    PR_EnterMonitor(vr_monitor);
#endif

    if (isInited) {
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
        if ( unixreg != NULL )
            NR_RegClose( unixreg );
#endif
        err = NR_RegClose( vreg );
        isInited = 0;
    }

#ifndef STANDALONE_REGISTRY
    PR_ExitMonitor(vr_monitor);
#endif

    return err;
}	/* Close */

VR_INTERFACE(REGERR) VR_GetVersion(char *component_path, VERSION *result)
{
	REGERR  err;
	RKEY    rootkey;
    RKEY    key;
    HREG    hreg;
	VERSION ver;
	char    buf[MAXREGNAMELEN];

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	hreg = vreg;
	
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    err = vr_FindKey( component_path, &hreg, &key );
#else
    rootkey = PATH_ROOT(component_path);
    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
#endif
	if (err != REGERR_OK)
		return err;

    err = NR_RegGetEntryString( hreg, key, VERSTR, buf, sizeof(buf) );
	if (err != REGERR_OK)
		return err;

	vr_ParseVersion(buf, &ver);

	memcpy(result, &ver, sizeof(VERSION));

	return REGERR_OK;

}	/* GetVersion */

VR_INTERFACE(REGERR) VR_GetPath(char *component_path, uint32 sizebuf, char *buf)
{
	REGERR err;
	RKEY rootkey;
	RKEY key;
    HREG hreg;

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	hreg = vreg;
	
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    err = vr_FindKey( component_path, &hreg, &key );
#else
    rootkey = PATH_ROOT(component_path);
    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
#endif
	if (err != REGERR_OK)
		return err;
    
    err = vr_GetPathname( hreg, key, PATHSTR, buf, sizebuf );

	return err;

}	/* GetPath */

#ifdef XP_MAC
#pragma export reset
#endif


VR_INTERFACE(REGERR) VR_SetDefaultDirectory(char *component_path, char *directory)
{
	REGERR err;
	RKEY rootkey;
	RKEY key;

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	rootkey = PATH_ROOT(component_path);

    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
	if (err != REGERR_OK)
		return err;
    
/*    err = NR_RegSetEntryString( vreg, key, DIRSTR, directory ); */
    err = vr_SetPathname( vreg, key, DIRSTR, directory );

	return err;
}



VR_INTERFACE(REGERR) VR_GetDefaultDirectory(char *component_path, uint32 sizebuf, char *buf)
{
	REGERR err;
	RKEY rootkey;
	RKEY key;
    HREG hreg;

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	hreg = vreg;
	
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    err = vr_FindKey( component_path, &hreg, &key );
#else
    rootkey = PATH_ROOT(component_path);
    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
#endif
	if (err != REGERR_OK)
		return err;
    
/*    err = NR_RegGetEntryString( vreg, key, DIRSTR, buf, sizebuf ); */
    err = vr_GetPathname( hreg, key, DIRSTR, buf, sizebuf );

	return err;
}



VR_INTERFACE(REGERR) VR_Install(char *component_path, char *filepath, char *version, int bDirectory)
{
	REGERR err;
	RKEY rootKey;
	RKEY key;

	/* Initialize the registry in case this is first call */
	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	/* Use curver if path is relative */
	rootKey = PATH_ROOT(component_path);

	/* Make sure path components (keys) exist by calling Add */
    /* (special "" component must always exist, and Add fails) */
    if ( component_path != NULL && *component_path == '\0' ) {
        err = NR_RegGetKey( vreg, rootKey, component_path, &key );
    }
    else {
	    err = NR_RegAddKey( vreg, rootKey, component_path, &key );
    }
	if (err != REGERR_OK)
		return err;

	if ( version != NULL && *version != '\0' ) {
        /* Add "Version" entry with values like "4.0.0.0" */
	    err = NR_RegSetEntryString( vreg, key, VERSTR, version );
	    if (err != REGERR_OK)
    		goto abort;
    }

    if ( filepath != NULL && *filepath != '\0' ) {
        /* add "Path" entry */
        err = vr_SetPathname( vreg, key, (bDirectory)?DIRSTR:PATHSTR, filepath );

	    if (err != REGERR_OK)
    		goto abort;
    }

	return REGERR_OK;

abort:
	NR_RegDeleteKey( vreg, rootKey, component_path );
	return err;

}	/* Install */



VR_INTERFACE(REGERR) VR_Remove(char *component_path)
{
	REGERR err;
	RKEY rootkey;

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

	rootkey = PATH_ROOT(component_path);

	return NR_RegDeleteKey( vreg, rootkey, component_path );

}	/* Remove */



VR_INTERFACE(REGERR) VR_Enum(REGENUM *state, char *buffer, uint32 buflen)
{
	REGERR err;

	err = vr_Init();
    if (err != REGERR_OK)
        return err;

    return NR_RegEnumSubkeys( vreg, ROOTKEY_VERSIONS, state, buffer, buflen, REGENUM_DESCEND);
}

#ifdef XP_MAC
#pragma export on
#endif

VR_INTERFACE(REGERR) VR_InRegistry(char *component_path)
{
	REGERR err;
	RKEY rootkey;
	RKEY key;
    HREG hreg;

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    return vr_FindKey( component_path, &hreg, &key );
#else
    rootkey = PATH_ROOT(component_path);
    return NR_RegGetKey( vreg, rootkey, component_path, &key );
#endif
}	/* InRegistry */


VR_INTERFACE(REGERR) VR_ValidateComponent(char *component_path)
{
	REGERR err;
	RKEY rootkey;
	RKEY key;
    HREG hreg;
	char path[MAXREGPATHLEN];
    char *url = NULL;
    char *urlPath;
	XP_StatStruct s;


#ifdef USE_CHECKSUM
	char buf[MAXREGNAMELEN];
	long calculatedCheck;
	int storedCheck;
#endif

	err = vr_Init();
	if (err != REGERR_OK)
		return err;

#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
    err = vr_FindKey( component_path, &hreg, &key );
#else
    rootkey = PATH_ROOT(component_path);
    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
#endif
    if ( err != REGERR_OK )
        return err;

    err = VR_GetPath( component_path, sizeof(path), path );
    if ( err != REGERR_OK ) {
        if ( err == REGERR_NOFIND ) {
            err = REGERR_NOPATH;
        }
        return err;
    }

    urlPath = path;
#ifdef BROKEN
    url = XP_PlatformFileToURL(path);
    if ( url == NULL )
        return REGERR_MEMORY;
    urlPath = url+7;
#endif

    if ( XP_Stat( urlPath, &s, xpURL ) != 0 ) {
        err = REGERR_NOFILE;
    }
    XP_FREEIF(url);
    if (err != REGERR_OK)
        return err;


#if defined(USE_CHECKSUM) && !defined(XP_UNIX)
	err = NR_RegGetEntryString( vreg, key, CHKSTR, buf, sizeof(buf) );
	if (err != REGERR_OK)
		return err;

	storedCheck = atoi(buf);

	err = vr_GetCheck(filepath, &calculatedCheck);
    if (err != REGERR_OK)
        return err;

	if (storedCheck != calculatedCheck)
	{
		/* printf("File %s checksum was %d, not %d as expected.\n", 
			filepath, calculatedCheck, storedCheck);
        */
		return REGERR_BADCHECK;
	}
#endif /* USE_CHECKSUM */

	return REGERR_OK;

}	/* CheckEntry */

#ifdef XP_MAC
#pragma export reset
#endif

VR_INTERFACE(REGERR) VR_SetRegDirectory(const char *path)
{
  char *tmp = XP_STRDUP(path);
  if (NULL == tmp) {
    return REGERR_MEMORY;
  }

  XP_FREEIF(app_dir);
  app_dir = tmp;

  return REGERR_OK;
}

/* EOF: VerReg.c */
