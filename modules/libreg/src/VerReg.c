/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
/* ====================================================================
 * VerReg.c
 * XP Version Registry functions (prototype)
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * Install 'Navigator' produces a tree of:
 *
 *      /Components/Netscape/Web/Navigator/
 *              ...Path="c:\netscape\program\netscape.exe"
 *              ...Version=4.0.0.0
 *
 * --------------------------------------------------------------------
 */
#include <fcntl.h>
#include <errno.h>

#if defined(XP_WIN) 
#include <io.h>
#endif

#if defined(XP_OS2)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <string.h>

#ifdef STANDALONE_REGISTRY
#include <stdlib.h>
#include <assert.h>
#endif /*STANDALONE_REGISTRY*/

#include "reg.h"
#include "NSReg.h"
#include "VerReg.h"

#ifdef XP_MAC
#include <Folders.h>
#endif

/* -------- local defines --------------- 
*/
#define MAXREGVERLEN 32     /* Version=12345.12345.12345.12345 */

#define VERSTR          "Version"
#define CHKSTR          "Check"
#define PATHSTR         "Path"
#define DIRSTR          "Directory"
#define NAVHOME         "InstallDir"
#define REFCSTR         "RefCount"
#define SHAREDSTR       "Shared"
#define PACKAGENAMESTR  "PackageName"
#define SHAREDFILESSTR  "/Shared Files"

#define VERSION_NAME    "Mozilla"
#define NAVIGATOR_NODE  "/mozilla.org"
#define CURRENT_VER     "CurrentVersion"

#define PATH_ROOT(p)   ( ((p) && *(p)==PATHDEL) ? ROOTKEY_VERSIONS : curver )
#define UNIX_ROOT(p)   ( ((p) && *(p)==PATHDEL) ? ROOTKEY_VERSIONS : unixver )

 
/* ---------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------
 */
static int isInited = 0;
static RKEY curver = 0;
static char gCurstr[MAXREGNAMELEN];

static HREG vreg = 0;

static char *app_dir = NULL;

char *verRegName = NULL;


#if defined(XP_UNIX)
/* Extra Unix variables to deal with two registries 
 *   "vreg" is always the writable registry.
 *   If "vreg" is the local registry then "unixreg" will
 *   be the global registry read-only (unless we couldn't
 *   open it).
 */
#if !defined(STANDALONE_REGISTRY)
static HREG unixreg = 0;
static RKEY unixver = 0;
#endif
XP_Bool bGlobalRegistry = FALSE;
#endif

#ifndef STANDALONE_REGISTRY
PRLock *vr_lock = NULL;
#endif


/* ---------------------------------------------------------------------
 * local functions
 * ---------------------------------------------------------------------
 */
static REGERR vr_Init(void);
static XP_Bool vr_CompareDirs( char *dir1, char *dir2 );
static REGERR vr_SetCurrentNav( char *product, char *programPath, char *versionStr);
static REGERR vr_ParseVersion(char *verstr, VERSION *result);

#ifdef USE_CHECKSUM
static REGERR vr_GetCheck(char *path, int32 *check);
#endif

static REGERR vr_SetPathname(HREG reg, RKEY key, char *entry, char *dir);
static REGERR vr_GetPathname(HREG reg, RKEY key, char *entry, char *buf, uint32 sizebuf);

static REGERR vr_FindKey(char *name, HREG *hreg, RKEY *key);

static REGERR vr_GetUninstallItemPath(char *regPackageName, char *regbuf, uint32 regbuflen);
static REGERR vr_convertPackageName(char *regPackageName, char *convertedPackageName, uint32 convertedDataLength);
static REGERR vr_unmanglePackageName(char *mangledPackageName, char *regPackageName, uint32 regPackageLength);

#ifdef XP_MAC
static void vr_MacAliasFromPath(const char * fileName, void ** alias, int32 * length);
static char * vr_PathFromMacAlias(const void * alias, uint32 aliasLength);
#endif

/* --------------------------------------------------------------------- */

static REGERR vr_Init(void)
{

    REGERR  err = REGERR_OK;
    char    *regname = vr_findVerRegName();
#if defined(XP_UNIX) || defined(STANDALONE_REGISTRY)
    char    curstr[MAXREGNAMELEN];
    RKEY    navKey;
#endif
#ifdef XP_UNIX
    char    *regbuf = NULL;
#endif

#ifndef STANDALONE_REGISTRY
    if (vr_lock == NULL)
        return REGERR_FAIL;
#endif
    PR_Lock(vr_lock);

    if (!isInited)
    {
#ifdef XP_UNIX
        /* need browser directory to find the correct registry */
        if (app_dir != NULL) {
            regbuf = (char*)XP_ALLOC( 10 + XP_STRLEN(app_dir) );
            if (regbuf != NULL ) {
                XP_STRCPY( regbuf, app_dir );
                XP_STRCAT( regbuf, "/registry" );
            } 
            else {
                err = REGERR_MEMORY;
            }
        } 
        if ( err != REGERR_OK )
            goto done;

        if (bGlobalRegistry) 
            regname = regbuf;
#endif

        /* Open version registry */
        err = NR_RegOpen( regname, &vreg );

#ifndef STANDALONE_REGISTRY
        if (err == REGERR_OK) 
        {
            /* find/set the current nav node */
            err = vr_SetCurrentNav( VERSION_NAME, app_dir, NULL );
            if ( REGERR_OK != err ) {
                /* couldn't find or set current nav -- big problems! */
                NR_RegClose( vreg );
                goto done;
            }
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
    PR_Unlock(vr_lock);
#if defined(XP_UNIX) && !defined(STANDALONE_REGISTRY)
    XP_FREEIF(regbuf);
#endif
    return err;

}   /* Init */



#ifdef XP_PC
#define VR_FILE_SEP '\\'
#endif
#ifdef XP_MAC
#define VR_FILE_SEP ':'
#endif
#ifdef XP_BEOS
#define VR_FILE_SEP '/'
#endif
#ifdef XP_UNIX
#define VR_FILE_SEP '/'
#endif

static XP_Bool vr_CompareDirs( char *dir1, char *dir2 )
{
    int len1,len2;
   
    XP_ASSERT( dir1 && dir2 );
    if (!dir1 || !dir2) return FALSE;

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

}   /* ParseVersion */



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
        case ENOENT:    /* file not found */
            return REGERR_NOFILE;

        case EACCES:    /* file in use */
        
#ifdef EMFILE        
        case EMFILE:    /* too many files open */
#endif
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

}   /* GetCheck */

#endif /* USE_CHECKSUM */



static REGERR vr_SetPathname(HREG reg, RKEY key, char *entry, char *dir)
{
    REGERR err;
    int32  datalen = XP_STRLEN(dir)+1; /* include '\0' */

    err = NR_RegSetEntry( reg, key, entry, REGTYPE_ENTRY_FILE, dir, datalen);

    return err;
}



static REGERR vr_GetPathname(HREG reg, RKEY key, char *entry, char *buf, uint32 sizebuf)
{
    REGERR  err;
    REGINFO info;
    
    info.size = sizeof(REGINFO);
        
#ifndef XP_MAC
        err = NR_RegGetEntry( reg, key, entry, (void*)buf, &sizebuf );
        return err;
#else
    
    err = NR_RegGetEntryInfo( reg, key, entry, &info );
    
    if (err != REGERR_OK)
        return err;
    
    if (info.entryType == REGTYPE_ENTRY_FILE ||
        info.entryType == REGTYPE_ENTRY_STRING_UTF )
    {
        err = NR_RegGetEntry( reg, key, entry, (void*)buf, &sizebuf );  
    }
    else if (info.entryType == REGTYPE_ENTRY_BYTES)
    {

        extern char * nr_PathFromMacAlias(const void * alias, uint32 aliasLength);

        #define MAC_ALIAS_BUFFER_SIZE 4000
        char stackBuf[MAC_ALIAS_BUFFER_SIZE];
        uint32 stackBufSize = MAC_ALIAS_BUFFER_SIZE;
        char * tempBuf;

        err = NR_RegGetEntry( reg, key, entry, (void*)stackBuf, &stackBufSize );

        if (err != REGERR_OK)
            return err;

        tempBuf = nr_PathFromMacAlias(stackBuf, stackBufSize);

        if (tempBuf == NULL) 
        {
            /* don't change error w/out changing vr_SetCurrentNav to match */
            buf[0] = '\0';
            err = REGERR_NOFILE;
         }
        else 
        {
            if (XP_STRLEN(tempBuf) > sizebuf)
                err = REGERR_BUFTOOSMALL;
            else
                XP_STRCPY(buf, tempBuf);

            XP_FREE(tempBuf);
        }
    }
    else
    {
        /* what did we put here?? */
        err = REGERR_BADTYPE;
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
    char        dirbuf[MAXREGNAMELEN];

    XP_ASSERT( installation ); /* required */
    XP_ASSERT( programPath );  /* required */
    if ( !installation || !programPath )
        return REGERR_PARAM;

    err = NR_RegAddKey( vreg, ROOTKEY_VERSIONS, NAVIGATOR_NODE, &navKey );
    if (err != REGERR_OK)
        goto done;

    /* ...look for "Current Version" entry */
    err = NR_RegGetEntryString( vreg, navKey, CURRENT_VER, gCurstr, sizeof(gCurstr));
    if ( err == REGERR_NOFIND )
    {
        /* No current installation, we can simply add a new one  */
        err = NR_RegAddKey( vreg, navKey, installation, &curver );

        /* ... add Path and Version properties */
        if ( err == REGERR_OK ) 
        {
            err = vr_SetPathname( vreg, curver, NAVHOME, programPath );
            if ( REGERR_OK == err && versionStr != NULL && *versionStr != '\0')
            {
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
        err = NR_RegGetKey( vreg, navKey, gCurstr, &curver );
        if ( REGERR_OK == err ) {
            err = vr_GetPathname( vreg, curver, NAVHOME, dirbuf, sizeof(dirbuf) );
            if ( REGERR_OK == err ) {
                bFound = vr_CompareDirs(dirbuf, programPath);
            }
            else if ( REGERR_NOFIND == err ) {
                /* assume this is the right one since it's 'Current' */
                err = vr_SetPathname( vreg, curver, NAVHOME, programPath );
                bFound = TRUE;
            }
        }
        
        /* Look for an existing installation if not found */
        state = 0;
        while (!bFound && ((err == REGERR_OK) || (err == REGERR_NOFILE)) ) {
            err = NR_RegEnumSubkeys( vreg, navKey, &state, gCurstr,
                    sizeof(gCurstr), REGENUM_NORMAL );

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
            err = NR_RegSetEntryString( vreg, navKey, CURRENT_VER, gCurstr );
            /* update version (curver already set) */
            if ( REGERR_OK == err && versionStr != NULL && *versionStr != '\0' ) {
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
            if ( REGERR_OK == err && versionStr != NULL && *versionStr != '\0' ) {
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
        if (rootkey)
            err = NR_RegGetKey( *hreg, rootkey, component_path, key );
        else
            err = REGERR_NOFIND;
    }
    if (unixreg == NULL || err == REGERR_NOFIND ) 
#endif
    {
        *hreg = vreg;
        rootkey = PATH_ROOT(component_path);
        if (rootkey)
            err = NR_RegGetKey( *hreg, rootkey, component_path, key );
        else
            err = REGERR_NOFIND;
    }

    return err;
}



/* ---------------------------------------------------------------------
 * Interface
 * ---------------------------------------------------------------------
 */

#ifdef XP_MAC
#pragma export on
#endif

#ifndef STANDALONE_REGISTRY
VR_INTERFACE(REGERR) VR_PackRegistry(void *userData, nr_RegPackCallbackFunc fn)
{
    REGERR err;

    /* make sure vreg (src) is open */
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    err = NR_RegPack( vreg, userData, fn );

    return err;

}   /* PackRegistry */
#endif /* STANDALONE_REGISTRY */


VR_INTERFACE(REGERR) VR_CreateRegistry( char *installation, char *programPath, char *versionStr )
{
    REGERR      err;
    char *      regname = vr_findVerRegName();
#ifdef XP_UNIX
    char *      regbuf = NULL;
#endif

    if ( installation == NULL || *installation == '\0' )
        return REGERR_PARAM;

#ifdef XP_UNIX
#ifndef STANDALONE_REGISTRY
    if (bGlobalRegistry)
#endif 
    {
        regbuf = (char*)XP_ALLOC( 10 + XP_STRLEN(programPath) );
        if (regbuf == NULL) 
            return REGERR_MEMORY;

        XP_STRCPY( regbuf, programPath );
        XP_STRCAT( regbuf, "registry" );
        regname = regbuf;
    }
#endif /* XP_UNIX */

    PR_Lock(vr_lock);

    /* automatically creates it if not found */
    err = NR_RegOpen( regname, &vreg );
    if (err == REGERR_OK) 
    {
        /* create default tree with 'installation' under Navigator */
        /* set Current to the installation string */

        err = vr_SetCurrentNav( installation, programPath, versionStr );

        if ( REGERR_OK == err )
            isInited = 1;
        else
            NR_RegClose( vreg );
    }

    PR_Unlock(vr_lock);

#if defined(XP_UNIX)
    XP_FREEIF( regbuf );
#endif
    return err;

}   /* CreateRegistry */


VR_INTERFACE(REGERR) VR_Close(void)
{
    REGERR err = REGERR_OK;

#ifndef STANDALONE_REGISTRY
    if (vr_lock == NULL)
        return REGERR_FAIL;
#endif

    PR_Lock(vr_lock);

    if (isInited) {
#if !defined(STANDALONE_REGISTRY) && defined(XP_UNIX)
        if ( unixreg != NULL )
            NR_RegClose( unixreg );
#endif
        err = NR_RegClose( vreg );
        isInited = 0;
    }

    PR_Unlock(vr_lock);

    return err;
}   /* Close */



VR_INTERFACE(REGERR) VR_GetVersion(char *component_path, VERSION *result)
{
    REGERR  err;
    RKEY    key;
    HREG    hreg;
    VERSION ver;
    char    buf[MAXREGNAMELEN];

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    hreg = vreg;

    err = vr_FindKey( component_path, &hreg, &key );
    if (err != REGERR_OK)
        return err;

    err = NR_RegGetEntryString( hreg, key, VERSTR, buf, sizeof(buf) );
    if (err != REGERR_OK)
        return err;

    vr_ParseVersion(buf, &ver);

    memcpy(result, &ver, sizeof(VERSION));

    return REGERR_OK;

}   /* GetVersion */



VR_INTERFACE(REGERR) VR_GetPath(char *component_path, uint32 sizebuf, char *buf)
{
    REGERR err;
    RKEY key;
    HREG hreg;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    hreg = vreg;

    err = vr_FindKey( component_path, &hreg, &key );
    if (err != REGERR_OK)
        return err;
    
    err = vr_GetPathname( hreg, key, PATHSTR, buf, sizebuf );

    return err;

}   /* GetPath */



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
    
    err = vr_SetPathname( vreg, key, DIRSTR, directory );

    return err;
}



VR_INTERFACE(REGERR) VR_GetDefaultDirectory(char *component_path, uint32 sizebuf, char *buf)
{
    REGERR err;
    RKEY key;
    HREG hreg;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    hreg = vreg;

    err = vr_FindKey( component_path, &hreg, &key );
    if (err != REGERR_OK)
        return err;

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

}   /* Install */



VR_INTERFACE(REGERR) VR_Remove(char *component_path)
{
    REGERR err;
    RKEY rootkey;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    rootkey = PATH_ROOT(component_path);

    return NR_RegDeleteKey( vreg, rootkey, component_path );

}   /* Remove */

VR_INTERFACE(REGERR) VR_Enum(char *component_path, REGENUM *state, 
                                         char *buffer, uint32 buflen)
{
    REGERR  err;
    RKEY    rootkey;
    RKEY    key;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( component_path == NULL )
        rootkey = ROOTKEY_VERSIONS;
    else
        rootkey = PATH_ROOT(component_path);

    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
    if (err != REGERR_OK)
        return err;

    err = NR_RegEnumSubkeys( vreg, key, state, buffer, buflen, REGENUM_DEPTH_FIRST);

    return err;

}   /* Enum */

VR_INTERFACE(REGERR) VR_InRegistry(char *component_path)
{
    REGERR err;
    RKEY key;
    HREG hreg;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    return vr_FindKey( component_path, &hreg, &key );
}   /* InRegistry */



VR_INTERFACE(REGERR) VR_ValidateComponent(char *component_path)
{
    REGERR err;
    RKEY key;
    char path[MAXREGPATHLEN];
    HREG hreg;


#ifdef USE_CHECKSUM
    char buf[MAXREGNAMELEN];
    long calculatedCheck;
    int storedCheck;
#endif

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    err = vr_FindKey( component_path, &hreg, &key );
    if ( err != REGERR_OK )
        return err;

    err = VR_GetPath( component_path, sizeof(path), path );
    if ( err != REGERR_OK ) {
        if ( err == REGERR_NOFIND ) {
            err = REGERR_NOPATH;
        }
        return err;
    }

    {
        struct stat  statStruct;

        if ( stat ( path, &statStruct ) != 0 ) {
            err = REGERR_NOFILE;
        }
    }
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
        return REGERR_BADCHECK;
    }
#endif /* USE_CHECKSUM */

    return REGERR_OK;

}   /* CheckEntry */



VR_INTERFACE(REGERR) VR_SetRegDirectory(const char *path)
{
    char *tmp;

    tmp = XP_STRDUP(path);
    if (NULL == tmp) {
        return REGERR_MEMORY;
    }

    PR_Lock(vr_lock);

    XP_FREEIF(app_dir);
    app_dir = tmp;
    
    PR_Unlock(vr_lock);

    return REGERR_OK;
}



VR_INTERFACE(REGERR) VR_SetRefCount(char *component_path, int refcount)
{
    REGERR err;
    RKEY rootKey;
    RKEY key = 0;
    char rcstr[MAXREGNAMELEN];

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    /* Use curver if path is relative */
    rootKey = PATH_ROOT(component_path);

    /* Make sure path components (keys) exist by calling Add */
    /* (special "" component must always exist, and Add fails) */
    if ( component_path != NULL && *component_path == '\0' ) {
        err = REGERR_PARAM;
    }
    else {
        err = NR_RegAddKey( vreg, rootKey, component_path, &key );
    }
    
    if (err != REGERR_OK)
        return err;

    *rcstr = '\0';
    /* itoa(refcount, rcstr, 10); */
    XP_SPRINTF(rcstr, "%d", refcount);
    
    if ( rcstr != NULL && *rcstr != '\0' ) {
        /* Add "RefCount" */
        err = NR_RegSetEntryString( vreg, key, REFCSTR, rcstr );
    }
    
    return err;
}   /* SetRefCount */



VR_INTERFACE(REGERR) VR_GetRefCount(char *component_path, int *result)
{
    REGERR  err;
    RKEY    rootkey;
    RKEY    key;
    char    buf[MAXREGNAMELEN];

    *result = -1; 

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    /* "Uninstall" only happens in the writable registry, so no
     * need to search the shared one on Unix using vr_FindKey()
     */
    rootkey = PATH_ROOT(component_path);
    err = NR_RegGetKey( vreg, rootkey, component_path, &key );
    if (err != REGERR_OK)
        return err;

    err = NR_RegGetEntryString( vreg, key, REFCSTR, buf, sizeof(buf) );
    if (err != REGERR_OK)
        return err;

    *result = atoi( buf );

    return REGERR_OK;

}   /* GetRefCount */

#ifdef XP_MAC
#pragma export reset
#endif

static REGERR vr_GetUninstallItemPath(char *regPackageName, char *regbuf, uint32 regbuflen)
{
    XP_Bool bSharedUninstall = FALSE;
    XP_Bool bNavPackage = FALSE;
    uint32 len = 0;
    uint32 sharedstrlen = 0;
    uint32 curstrlen = 0;
    uint32 curregbuflen = 0;

    /* determine install type */
    if (*regPackageName == '\0') {
        bNavPackage = TRUE;
    }
    else if ( *regPackageName == PATHDEL) {
        bSharedUninstall = TRUE;
    }

    /* create uninstall path prefix */
    len = XP_STRLEN(REG_UNINSTALL_DIR);
    if (len < regbuflen)
    {
        XP_STRCPY( regbuf, REG_UNINSTALL_DIR );
    }
    else
    {
        return REGERR_BUFTOOSMALL;
    }
    if (bSharedUninstall)
    {
        sharedstrlen = XP_STRLEN(SHAREDSTR);
        if (sharedstrlen < (regbuflen - len))
            XP_STRCAT( regbuf, SHAREDSTR );
        else 
            return REGERR_BUFTOOSMALL;
    }
    else
    {
        curstrlen = XP_STRLEN(gCurstr);
        if (curstrlen < (regbuflen - len))
            XP_STRCAT( regbuf, gCurstr );
        else 
            return REGERR_BUFTOOSMALL;
        if (1 < (regbuflen - len - curstrlen))
            XP_STRCAT( regbuf, "/" );
        else 
            return REGERR_BUFTOOSMALL;
    }  

    /* add final uninstall node name */
    len = 0;
    curregbuflen = XP_STRLEN(regbuf);
    if ( bNavPackage ) {
        len = XP_STRLEN(UNINSTALL_NAV_STR);
        if (len < (regbuflen - curregbuflen))
            XP_STRCAT( regbuf, UNINSTALL_NAV_STR );
        else 
            return REGERR_BUFTOOSMALL;
    }
    else {
        len = XP_STRLEN(regPackageName);
        if (len < (regbuflen - curregbuflen))
            XP_STRCAT( regbuf, regPackageName );
        else 
            return REGERR_BUFTOOSMALL;
    }
    return REGERR_OK;
}


/**
 * Replaces all '/' with '_',in the given string.If an '_' already exists in the
 * given string, it is escaped by adding another '_' to it.
 */
static REGERR vr_convertPackageName(char *regPackageName, char *convertedPackageName, uint32 convertedDataLength)
{
    uint32 length = 0;
    uint32 i;
    uint32 j = 0;

    length = XP_STRLEN(regPackageName);
    
    if (convertedDataLength <= length)
        return REGERR_BUFTOOSMALL;

    for (i=0, j=0; i<length; i++, j++)
    {
        if (j < (convertedDataLength-1))
        {
            convertedPackageName[j] = regPackageName[i];
        }
        else
        {
            return REGERR_BUFTOOSMALL;
        }
        if (regPackageName[i] == '_')
        {
            if ((j+1) < (convertedDataLength-1))
            {
                convertedPackageName[j+1] = '_';
            }
            else
            {
                return REGERR_BUFTOOSMALL;
            }
            j = j + 1;
        }
    }

    if (convertedPackageName[j-1] == '/')
        convertedPackageName[j-1] = '\0';
    else
    {
        if (j < convertedDataLength)
        {
            convertedPackageName[j] = '\0';
        }
        else
        {
            return REGERR_BUFTOOSMALL;
        }
    }

    length = 0;
    length = XP_STRLEN(convertedPackageName);
    for (i=1; i<length; i++)
    {
        if (convertedPackageName[i] == '/')
            convertedPackageName[i] = '_';
    }

    return REGERR_OK;
}

static REGERR vr_unmanglePackageName(char *mangledPackageName, char *regPackageName, uint32 regPackageLength)
{
    uint32 length = 0;
    uint32 i = 0;
    uint32 j = 0;

    length = XP_STRLEN(mangledPackageName);
    
    if (regPackageLength <= length)
        return REGERR_BUFTOOSMALL;

    while (i < length)
    {
        if ((mangledPackageName[i] != '_') || (i == length-1)){
            if (j < (regPackageLength - 1))
                regPackageName[j] = mangledPackageName[i];
            else
                return REGERR_BUFTOOSMALL;
            j = j + 1;  
            i = i + 1;
        } else if (mangledPackageName[i + 1] != '_') { 
            if (j < (regPackageLength - 1))
                regPackageName[j] = '/';
            else
                return REGERR_BUFTOOSMALL;
            j = j + 1;
            i = i + 1;
        } else {
            if (j < (regPackageLength - 1))
                regPackageName[j] = '_';
            else
                return REGERR_BUFTOOSMALL;
            j = j + 1;
            i = i + 2;
        }
    }
    if (j < regPackageLength)
        regPackageName[j] = '\0';
    else
        return REGERR_BUFTOOSMALL;
    return REGERR_OK;
}

#ifdef XP_MAC
#pragma export on
#endif

VR_INTERFACE(REGERR) VR_UninstallCreateNode(char *regPackageName, char *userPackageName)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf;
    uint32 regbuflen = 0;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( regPackageName == NULL )
        err = REGERR_PARAM;
   
    if ( userPackageName == NULL)
        err = REGERR_PARAM;

    regbuflen = 256 + XP_STRLEN(regPackageName);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(regPackageName, regbuf, regbuflen);  
        if (err != REGERR_OK)
        {
            XP_FREE(regbuf);
            return err;
        }
        err = NR_RegAddKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
        XP_FREE(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }

    if (err == REGERR_OK)
        err = NR_RegSetEntryString(vreg, key, PACKAGENAMESTR, userPackageName);
  
    return err;

}   /* UninstallCreateNode */

VR_INTERFACE(REGERR) VR_GetUninstallUserName(char *regPackageName, char *outbuf, uint32 buflen)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf = NULL;
    char *convertedName = NULL;
    uint32 convertedDataLength = 0;
    uint32 regbuflen = 0;

    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( regPackageName == NULL || *regPackageName == '\0' || outbuf == NULL )
        return REGERR_PARAM;
   
    convertedDataLength = 2 * XP_STRLEN(regPackageName) + 1;
    convertedName = (char*)XP_ALLOC(convertedDataLength);
    if (convertedName == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }
  
    err = vr_convertPackageName(regPackageName, convertedName, convertedDataLength);
    if (err != REGERR_OK) {
        XP_FREE(convertedName);
        return err;
    }
    regbuflen = 256 + XP_STRLEN(convertedName);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf == NULL ) {
        err = REGERR_MEMORY;
    } else {
        err = vr_GetUninstallItemPath(convertedName, regbuf, regbuflen);
        if (err == REGERR_OK) {
            err = NR_RegGetKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
        }
        XP_FREE(regbuf);
    }

    if (err == REGERR_OK)
        err = NR_RegGetEntryString( vreg, key, PACKAGENAMESTR, outbuf, buflen );
  
    XP_FREE(convertedName);
    return err;

}   /* GetUninstallName */

VR_INTERFACE(REGERR) VR_UninstallAddFileToList(char *regPackageName, char *vrName)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf;
    uint32 regbuflen = 0;
    uint32 curregbuflen = 0;
    uint32 len = 0;
    
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( regPackageName == NULL )
        err = REGERR_PARAM;

    if ( vrName == NULL )
        err = REGERR_PARAM;

    regbuflen = 256 + XP_STRLEN(regPackageName);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(regPackageName, regbuf, regbuflen);
        if (err == REGERR_OK)
        {
            curregbuflen = XP_STRLEN(regbuf);
            len = XP_STRLEN(SHAREDFILESSTR);
            if (len < (regbuflen - curregbuflen))
            {
                XP_STRCAT(regbuf, SHAREDFILESSTR);
                err = NR_RegAddKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
            }
            else
                err = REGERR_BUFTOOSMALL;
        }
        XP_FREEIF(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }

    if (err == REGERR_OK)
        err = NR_RegSetEntryString( vreg, key, vrName, "");
  
    return err;

}   /* UninstallAddFileToList */

VR_INTERFACE(REGERR) VR_UninstallFileExistsInList(char *regPackageName, char *vrName)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf;
    char  sharedfilesstr[MAXREGNAMELEN];
    uint32 regbuflen = 0;
    uint32 curregbuflen = 0;
    uint32 len = 0;
    
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( regPackageName == NULL )
        err = REGERR_PARAM;

    if ( vrName == NULL )
        err = REGERR_PARAM;

    regbuflen = 256 + XP_STRLEN(regPackageName);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(regPackageName, regbuf, regbuflen);
        if (err == REGERR_OK)
        {
            curregbuflen = XP_STRLEN(regbuf);
            len = XP_STRLEN(SHAREDFILESSTR);
            if (len < (regbuflen - curregbuflen))
            {
                XP_STRCAT(regbuf, SHAREDFILESSTR);
                err = NR_RegGetKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
            }
            else
                err = REGERR_BUFTOOSMALL;
        }
        XP_FREEIF(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }
    
    if (err == REGERR_OK)
        err = NR_RegGetEntryString( vreg, key, vrName, sharedfilesstr,
                                    sizeof(sharedfilesstr) );
    return err;

}   /* UninstallFileExistsInList */

VR_INTERFACE(REGERR) VR_UninstallEnumSharedFiles(char *component_path, REGENUM *state, 
                                         char *buffer, uint32 buflen)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf;
    char *converted_component_path;
    uint32 convertedDataLength = 0;
    uint32 regbuflen = 0;
    uint32 curregbuflen = 0;
    uint32 len = 0;
 
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( component_path == NULL )
        return REGERR_PARAM;

    convertedDataLength = 2 * XP_STRLEN(component_path) + 1;
    converted_component_path = (char*)XP_ALLOC(convertedDataLength);
    if (converted_component_path == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }
    err = vr_convertPackageName(component_path, converted_component_path, convertedDataLength);
    if (err != REGERR_OK)
    {
        XP_FREEIF(converted_component_path);
        return err;
    }

    regbuflen = 256 + XP_STRLEN(converted_component_path);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(converted_component_path, regbuf, regbuflen); 
        if (err == REGERR_OK)
        {
            curregbuflen = XP_STRLEN(regbuf);
            len = XP_STRLEN(SHAREDFILESSTR);
            if (len < (regbuflen - curregbuflen))
            {
                 XP_STRCAT(regbuf, SHAREDFILESSTR);
                 err = NR_RegGetKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
            }
            else
                err = REGERR_BUFTOOSMALL;
        }
        XP_FREE(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }
    
    XP_FREE(converted_component_path);

    if (err == REGERR_OK)
        err = NR_RegEnumEntries( vreg, key, state, buffer, buflen, NULL);

    return err;

}   /* UninstallEnumSharedFiles */

VR_INTERFACE(REGERR) VR_UninstallDeleteFileFromList(char *component_path, char *vrName)
{
    REGERR err;
    RKEY key = 0;
    char *regbuf;
    char *converted_component_path;
    uint32 convertedDataLength = 0;
    uint32 regbuflen = 0;
    uint32 curregbuflen = 0;
    uint32 len = 0;
        
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( component_path == NULL )
        err = REGERR_PARAM;

    if ( vrName == NULL )
        err = REGERR_PARAM;

    convertedDataLength = 2 * XP_STRLEN(component_path) + 1;
    converted_component_path = (char*)XP_ALLOC(convertedDataLength);
    if (converted_component_path == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }
    err = vr_convertPackageName(component_path, converted_component_path, convertedDataLength);
    if (err != REGERR_OK)
    {
        XP_FREEIF(converted_component_path);
        return err;
    }

    regbuflen = 256 + XP_STRLEN(converted_component_path);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(converted_component_path, regbuf, regbuflen);  
         if (err == REGERR_OK)
        {
            curregbuflen = XP_STRLEN(regbuf);
            len = XP_STRLEN(SHAREDFILESSTR);
            if (len < (regbuflen - curregbuflen))
            {
                XP_STRCAT(regbuf, SHAREDFILESSTR);
                err = NR_RegGetKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
            }
            else
                err = REGERR_BUFTOOSMALL;
        }
        XP_FREE(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }
    
    XP_FREE(converted_component_path);

    if (err == REGERR_OK)
        err = NR_RegDeleteEntry( vreg, key, vrName);

    return err;

}   /* UninstallDeleteFileFromList */

VR_INTERFACE(REGERR) VR_UninstallDeleteSharedFilesKey(char *component_path)
{
    REGERR err;
    char *regbuf;
    char *converted_component_path;
    uint32 convertedDataLength = 0;
    uint32 regbuflen = 0;
    uint32 curregbuflen = 0;
    uint32 len = 0;
        
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( component_path == NULL )
        err = REGERR_PARAM;

    convertedDataLength = 2 * XP_STRLEN(component_path) + 1;
    converted_component_path = (char*)XP_ALLOC(convertedDataLength);
    if (converted_component_path == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }
    err = vr_convertPackageName(component_path, converted_component_path, convertedDataLength);
    if (err != REGERR_OK)
    {
        XP_FREEIF(converted_component_path);
        return err;
    }

    regbuflen = 256 + XP_STRLEN(converted_component_path);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(converted_component_path, regbuf, regbuflen); 
        if (err == REGERR_OK)
        {
            curregbuflen = XP_STRLEN(regbuf);
            len = XP_STRLEN(SHAREDFILESSTR);
            if (len < (regbuflen - curregbuflen))
            {
                XP_STRCAT(regbuf, SHAREDFILESSTR);
                err = NR_RegDeleteKey( vreg, ROOTKEY_PRIVATE, regbuf );
            }
            else
                err = REGERR_BUFTOOSMALL;
        }
        XP_FREE(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }

    XP_FREE(converted_component_path);
    return err;

}   /* UninstallDeleteSharedFilesKey */

VR_INTERFACE(REGERR) VR_UninstallDestroy(char *component_path)
{
    REGERR err;
    char *regbuf;
    char *converted_component_path;
    uint32 convertedDataLength = 0;
    uint32 regbuflen = 0;
        
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    if ( component_path == NULL )
        err = REGERR_PARAM;
    
    convertedDataLength = 2 * XP_STRLEN(component_path) + 1;
    converted_component_path = (char*)XP_ALLOC(convertedDataLength);
    if (converted_component_path == NULL ) {
        err = REGERR_MEMORY;
        return err;
    }
    err = vr_convertPackageName(component_path, converted_component_path, convertedDataLength);
    if (err != REGERR_OK)
    {
        XP_FREEIF(converted_component_path);
        return err;
    }

    regbuflen = 256 + XP_STRLEN(converted_component_path);
    regbuf = (char*)XP_ALLOC( regbuflen );
    if (regbuf != NULL )
    {
        err = vr_GetUninstallItemPath(converted_component_path, regbuf, regbuflen);  
        if (err == REGERR_OK)
        {
            err = NR_RegDeleteKey( vreg, ROOTKEY_PRIVATE, regbuf );
        }
        else
        {
            err = REGERR_BUFTOOSMALL;
        }
        XP_FREE(regbuf);
    }
    else
    {
        err = REGERR_MEMORY;
    }
    
    XP_FREE(converted_component_path);
    return err;

}   /* UninstallDestroy */

VR_INTERFACE(REGERR) VR_EnumUninstall(REGENUM *state, char* userPackageName,
                                    int32 len1, char*regPackageName, int32 len2, XP_Bool bSharedList)
{
    REGERR err;
    RKEY key;
    RKEY key1;
    char regbuf[MAXREGPATHLEN+1] = {0};
    char temp[MAXREGPATHLEN+1] = {0};
   
    err = vr_Init();
    if (err != REGERR_OK)
        return err;

    XP_STRCPY( regbuf, REG_UNINSTALL_DIR );
    if (bSharedList)
    {
        XP_STRCAT( regbuf, SHAREDSTR );
    }
    else
    {
        XP_STRCAT( regbuf, gCurstr );
    }  
                   
    err = NR_RegGetKey( vreg, ROOTKEY_PRIVATE, regbuf, &key );
    if (err != REGERR_OK)
        return err;

    *regbuf = '\0';
    *userPackageName = '\0';
    err = NR_RegEnumSubkeys( vreg, key, state, regbuf, sizeof(regbuf), REGENUM_CHILDREN);

    if (err == REGERR_OK && !bSharedList )
    {
        if (XP_STRCMP(regbuf, UNINSTALL_NAV_STR) == 0)
        {
            /* skip Communicator package, get the next one instead */
            err = NR_RegEnumSubkeys( vreg, key, state, regbuf, sizeof(regbuf), REGENUM_CHILDREN);
        }
    }
    if (err != REGERR_OK)
        return err;

    err = NR_RegGetKey( vreg, key, regbuf, &key1 );
    if (err != REGERR_OK)
        return err;

    err = NR_RegGetEntryString( vreg, key1, PACKAGENAMESTR, userPackageName, len1);

    if (err != REGERR_OK)
    {
        *userPackageName = '\0';
        return err;
    }

    if (len2 <= (int32)XP_STRLEN(regbuf))
    {
        err =  REGERR_BUFTOOSMALL;
        *userPackageName = '\0';
        return err;
    }
    
    *regPackageName = '\0';
    if (bSharedList)
    {
        XP_STRCPY(temp, "/");
        XP_STRCAT(temp, regbuf);
        *regbuf = '\0';
        XP_STRCPY(regbuf, temp);
    }

    err = vr_unmanglePackageName(regbuf, regPackageName, len2);
    return err;

}   /* EnumUninstall */

#ifdef XP_MAC
#pragma export reset
#endif

/* EOF: VerReg.c */
