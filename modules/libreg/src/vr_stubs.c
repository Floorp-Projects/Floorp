/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* this file contains stubs needed to build the registry routines
 * into a stand-alone library for use with our installers
 */

#ifdef STANDALONE_REGISTRY

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#else

#include "prtypes.h"
#include "plstr.h"

#endif /*STANDALONE_REGISTRY*/

#include "vr_stubs.h"

#if defined(XP_MAC)
#include <Folders.h>
#include <Script.h>
#include <stdlib.h>
#include <Errors.h>
#include "MoreFiles.h"
#include "FullPath.h"  /* For FSpLocationFromFullPath() */
#endif

#if defined(XP_MACOSX)
#include <Folders.h>
#include <stdlib.h>
#endif

#ifdef XP_BEOS
#include <FindDirectory.h>
#endif 

#ifdef XP_MACOSX
/* So that we're not dependent on the size of chars in a wide string literal */
static const UniChar kOSXRegParentName[] =
  { 'M', 'o', 'z', 'i', 'l', 'l', 'a' };
static const UniChar kOSXRegName[] =
  { 'G', 'l', 'o', 'b', 'a', 'l', '.', 'r', 'e', 'g', 's' };
static const UniChar kOSXVersRegName[] =
  { 'V', 'e', 'r', 's', 'i', 'o', 'n', 's', '.', 'r', 'e', 'g', 's' };

#define UNICHAR_ARRAY_LEN(s) (sizeof(s) / sizeof(UniChar))
#endif

#define DEF_REG "/.mozilla/registry"
#define WIN_REG "\\mozregistry.dat"
#define MAC_REG "\pMozilla Registry"
#define BEOS_REG "/mozilla/registry"

#define DEF_VERREG "/.mozilla/mozver.dat"
#define WIN_VERREG "\\mozver.dat"
#define MAC_VERREG "\pMozilla Versions"
#define BEOS_VERREG "/mozilla/mozver.dat"


/* ------------------------------------------------------------------
 *  OS/2 STUBS
 * ------------------------------------------------------------------
 */
#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>

#ifdef STANDALONE_REGISTRY
extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_TRUNCATE_BIN );
    }

    return fh;
}
#endif /*STANDALONE_REGISTRY*/

extern void vr_findGlobalRegName ()
{
    char    path[ CCHMAXPATH ];
    int     pathlen;
    XP_File fh = NULL;
    struct stat st;

    XP_STRCPY(path, ".");
    pathlen = strlen(path);

    if ( pathlen > 0 ) {
        XP_STRCPY( path+pathlen, WIN_REG );
        globalRegName = XP_STRDUP(path);
    }
}

char* vr_findVerRegName()
{
    /* need to find a global place for the version registry */
    if ( verRegName == NULL )
    {
        if ( globalRegName == NULL)
            vr_findGlobalRegName();
        verRegName = XP_STRDUP(globalRegName);
    }

    return verRegName;
}

#endif /* XP_OS2 */


/* ------------------------------------------------------------------
 *  WINDOWS STUBS
 * ------------------------------------------------------------------
 */
#if defined(XP_WIN)
#include "windows.h"
#define PATHLEN 260

#ifdef STANDALONE_REGISTRY
extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_TRUNCATE_BIN );
    }

    return fh;
}
#endif /*STANDALONE_REGISTRY*/

extern void vr_findGlobalRegName ()
{
    char    path[ PATHLEN ];
    int     pathlen;
   
    pathlen = GetWindowsDirectory(path, PATHLEN);
    if ( pathlen > 0 ) {
        XP_FREEIF(globalRegName);
        XP_STRCPY( path+pathlen, WIN_REG );
        globalRegName = XP_STRDUP(path);
    }
}

char* vr_findVerRegName()
{
    char    path[ PATHLEN ];
    int     pathlen;
   
    if ( verRegName == NULL )
    {
        pathlen = GetWindowsDirectory(path, PATHLEN);
        if ( pathlen > 0 ) {
            XP_STRCPY( path+pathlen, WIN_VERREG );
            verRegName = XP_STRDUP(path);
        }
    }

    return verRegName;
}

#if !defined(WIN32) && !defined(__BORLANDC__)
int FAR PASCAL _export WEP(int);

int FAR PASCAL LibMain(HANDLE hInst, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine)
{
    if ( wHeapSize > 0 )
        UnlockData(0);
    return 1;
}

int FAR PASCAL _export WEP(int nParam)
{ 
    return 1; 
}
#endif /* not WIN32 */

#endif /* XP_WIN */


/* ------------------------------------------------------------------
 *  MACINTOSH STUBS
 * ------------------------------------------------------------------
 */

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Files.h>

#ifdef STANDALONE_REGISTRY
extern XP_File vr_fileOpen(const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;
    
#ifdef STANDALONE_REGISTRY
    errno = 0; /* reset errno (only if we're using stdio) */
#endif

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else 
        {
            /* should never get here! */
            fh = fopen( name, XP_FILE_TRUNCATE_BIN );
        }
    }
    return fh;
}
#endif /*STANDALONE_REGISTRY*/

#if defined (XP_MACOSX)
extern void vr_findGlobalRegName()
{
    OSErr   err;
    FSRef   foundRef;
    
    err = FSFindFolder(kLocalDomain, kDomainLibraryFolderType, kDontCreateFolder, &foundRef);
    if (err == noErr)
    {
        FSRef parentRef;
        err = FSMakeFSRefUnicode(&foundRef, UNICHAR_ARRAY_LEN(kOSXRegParentName), kOSXRegParentName,
                                 kTextEncodingUnknown, &parentRef);
        if (err == fnfErr)
        {
            err = FSCreateDirectoryUnicode(&foundRef, UNICHAR_ARRAY_LEN(kOSXRegParentName), kOSXRegParentName,
                                           kFSCatInfoNone, NULL, &parentRef, NULL, NULL);
        }
        if (err == noErr)
        {
            FSRef regRef;
            err = FSMakeFSRefUnicode(&parentRef, UNICHAR_ARRAY_LEN(kOSXRegName), kOSXRegName,
                                     kTextEncodingUnknown, &regRef);
            if (err == fnfErr)
            {
                FSCatalogInfo catalogInfo;
                FileInfo fileInfo = { 'REGS', 'MOSS', 0, { 0, 0 }, 0 };
                BlockMoveData(&fileInfo, &(catalogInfo.finderInfo), sizeof(FileInfo));
                err = FSCreateFileUnicode(&parentRef, UNICHAR_ARRAY_LEN(kOSXRegName), kOSXRegName,
                                               kFSCatInfoFinderInfo, &catalogInfo, &regRef, NULL);
            }
            if (err == noErr)
            {
                UInt8 pathBuf[PATH_MAX];
                err = FSRefMakePath(&regRef, pathBuf, sizeof(pathBuf));
                if (err == noErr)
                    globalRegName = XP_STRDUP(pathBuf);
            }
        }
    }
}
#else
extern void vr_findGlobalRegName()
{
    FSSpec  regSpec;
    OSErr   err;
    short   foundVRefNum;
    long    foundDirID;
    int     bCreate = 0;
    
    err = FindFolder(kOnSystemDisk,'pref', false, &foundVRefNum, &foundDirID);

    if (err == noErr)
    {
        err = FSMakeFSSpec(foundVRefNum, foundDirID, MAC_REG, &regSpec);

        if (err == -43) /* if file doesn't exist */
        {
            err = FSpCreate(&regSpec, 'MOSS', 'REGS', smSystemScript);
            bCreate = 1;
        }

        if (err == noErr)
        {
            Handle thePath;
            short pathLen;
            err = FSpGetFullPath(&regSpec, &pathLen, &thePath);
            if (err == noErr && thePath)
            {
                /* we have no idea if this moves memory, so better lock the handle */
            #if defined(STANDALONE_REGISTRY) || defined(USE_STDIO_MODES)
                HLock(thePath);
                globalRegName = (char *)XP_ALLOC(pathLen + 1);
                XP_STRNCPY(globalRegName, *thePath, pathLen);
                globalRegName[pathLen] = '\0';
            #else
                /* Since we're now using NSPR, this HAS to be a unix path! */
                const char* src;
                char* dst;
                HLock(thePath);
                globalRegName = (char*)XP_ALLOC(pathLen + 2);
                src = *(char**)thePath;
                dst = globalRegName;
                *dst++ = '/';
                while (pathLen--)
                {
                    char c = *src++;
                    *dst++ = (c == ':') ? '/' : c;
                }
                *dst = '\0';
            #endif
            }
            DisposeHandle(thePath);
        }
    }
}
#endif /* XP_MACOSX */

#ifdef XP_MACOSX
extern char* vr_findVerRegName()
{
    OSErr   err;
    FSRef   foundRef;
    
    err = FSFindFolder(kLocalDomain, kDomainLibraryFolderType, kDontCreateFolder, &foundRef);
    if (err == noErr)
    {
        FSRef parentRef;
        err = FSMakeFSRefUnicode(&foundRef, UNICHAR_ARRAY_LEN(kOSXRegParentName), kOSXRegParentName,
                                 kTextEncodingUnknown, &parentRef);
        if (err == fnfErr)
        {
            err = FSCreateDirectoryUnicode(&foundRef, UNICHAR_ARRAY_LEN(kOSXRegParentName), kOSXRegParentName,
                                           kFSCatInfoNone, NULL, &parentRef, NULL, NULL);
        }
        if (err == noErr)
        {
            FSRef regRef;
            err = FSMakeFSRefUnicode(&parentRef, UNICHAR_ARRAY_LEN(kOSXVersRegName), kOSXVersRegName,
                                     kTextEncodingUnknown, &regRef);
            if (err == fnfErr)
            {
                FSCatalogInfo catalogInfo;
                FileInfo fileInfo = { 'REGS', 'MOSS', 0, { 0, 0 }, 0 };
                BlockMoveData(&fileInfo, &(catalogInfo.finderInfo), sizeof(FileInfo));
                err = FSCreateFileUnicode(&parentRef, UNICHAR_ARRAY_LEN(kOSXVersRegName), kOSXVersRegName,
                                               kFSCatInfoFinderInfo, &catalogInfo, &regRef, NULL);
            }
            if (err == noErr)
            {
                UInt8 pathBuf[PATH_MAX];
                err = FSRefMakePath(&regRef, pathBuf, sizeof(pathBuf));
                if (err == noErr)
                    verRegName = XP_STRDUP(pathBuf);
            }
        }
    }
    return verRegName;
}
#else
extern char* vr_findVerRegName()
{
    FSSpec  regSpec;
    OSErr   err;
    short   foundVRefNum;
    long    foundDirID;
    int     bCreate = 0;
    
    /* quick exit if we have the info */
    if ( verRegName != NULL )
        return verRegName;

    err = FindFolder(kOnSystemDisk,'pref', false, &foundVRefNum, &foundDirID);

    if (err == noErr)
    {
        err = FSMakeFSSpec(foundVRefNum, foundDirID, MAC_VERREG, &regSpec);

        if (err == -43) /* if file doesn't exist */
        {
            err = FSpCreate(&regSpec, 'MOSS', 'REGS', smSystemScript);
            bCreate = 1;
        }

        if (err == noErr)
        {
            Handle thePath;
            short pathLen;
            err = FSpGetFullPath(&regSpec, &pathLen, &thePath);
            if (err == noErr && thePath)
            {
                /* we have no idea if this moves memory, so better lock the handle */
             #if defined(STANDALONE_REGISTRY) || defined(USE_STDIO_MODES)
                HLock(thePath);
                verRegName = (char *)XP_ALLOC(pathLen + 1);
                XP_STRNCPY(verRegName, *thePath, pathLen);
                verRegName[pathLen] = '\0';
            #else
                /* Since we're now using NSPR, this HAS to be a unix path! */
                const char* src;
                char* dst;
                HLock(thePath);
                verRegName = (char*)XP_ALLOC(pathLen + 2);
                src = *(char**)thePath;
                dst = verRegName;
                *dst++ = '/';
                while (pathLen--)
                {
                    char c = *src++;
                    *dst++ = (c == ':') ? '/' : c;
                }
                *dst = '\0';
            #endif
            }
            DisposeHandle(thePath);
        }
    }

    return verRegName;
}
#endif /* OS_MACOSX */

/* Moves and renames a file or directory.
   Returns 0 on success, -1 on failure (errno contains mac error code).
 */
#ifndef XP_MACOSX 
extern int nr_RenameFile(char *from, char *to)
{
    OSErr           err = -1;
    FSSpec          fromSpec;
    FSSpec          toSpec;
    FSSpec          destDirSpec;
    FSSpec          beforeRenameSpec;
    
#ifdef STANDALONE_REGISTRY
    errno = 0; /* reset errno (only if we're using stdio) */
#endif

    if (from && to) {
        err = FSpLocationFromFullPath(XP_STRLEN(from), from, &fromSpec);
        if (err != noErr) goto exit;
        
        err = FSpLocationFromFullPath(XP_STRLEN(to), to, &toSpec);
        if (err != noErr && err != fnfErr) goto exit;
        
        /* make an FSSpec for the destination directory */
        err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, nil, &destDirSpec);
        if (err != noErr) goto exit; /* parent directory must exist */

        /* move it to the directory specified */
        err = FSpCatMove(&fromSpec, &destDirSpec);
        if (err != noErr) goto exit;
        
        /* make a new FSSpec for the file or directory in its new location  */
        err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, fromSpec.name, &beforeRenameSpec);
        if (err != noErr) goto exit;
        
        /* rename the file or directory */
        err = FSpRename(&beforeRenameSpec, toSpec.name);
    }
        
    exit:
#ifdef STANDALONE_REGISTRY
    if (err != noErr)
        errno = err;
#endif
    return (err == noErr ? 0 : -1);
}
#endif


#ifdef STANDALONE_REGISTRY
char *strdup(const char *source)
{
        char    *newAllocation;
        size_t  stringLength;

        stringLength = strlen(source) + 1;

        newAllocation = (char *)XP_ALLOC(stringLength);
        if (newAllocation == NULL)
                return NULL;
        BlockMoveData(source, newAllocation, stringLength);
        return newAllocation;
}

#ifndef XP_MACOSX
int strcasecmp(const char *str1, const char *str2)
{
    char    currentChar1, currentChar2;

    while (1) {
    
        currentChar1 = *str1;
        currentChar2 = *str2;
        
        if ((currentChar1 >= 'a') && (currentChar1 <= 'z'))
            currentChar1 += ('A' - 'a');
        
        if ((currentChar2 >= 'a') && (currentChar2 <= 'z'))
            currentChar2 += ('A' - 'a');
                
        if (currentChar1 == '\0')
            break;
    
        if (currentChar1 != currentChar2)
            return currentChar1 - currentChar2;
            
        str1++;
        str2++;
    
    }
    
    return currentChar1 - currentChar2;
}

int strncasecmp(const char *str1, const char *str2, int length)
{
    char    currentChar1, currentChar2;

    while (length > 0) {

        currentChar1 = *str1;
        currentChar2 = *str2;

        if ((currentChar1 >= 'a') && (currentChar1 <= 'z'))
            currentChar1 += ('A' - 'a');

        if ((currentChar2 >= 'a') && (currentChar2 <= 'z'))
            currentChar2 += ('A' - 'a');

        if (currentChar1 == '\0')
            break;

        if (currentChar1 != currentChar2)
            return currentChar1 - currentChar2;

        str1++;
        str2++;

        length--;
    }

    return currentChar1 - currentChar2;
}
#endif /* XP_MACOSX */
#endif /* STANDALONE_REGISTRY */

#endif /* XP_MAC */


/* ------------------------------------------------------------------
 *  UNIX STUBS
 * ------------------------------------------------------------------
 */

/*allow OS/2 and Macintosh to use this main to test...*/
#if (defined(STANDALONE_REGISTRY) && defined(XP_MAC)) || defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)

#include <stdlib.h>

#ifdef XP_OS2
#include <io.h>
#define W_OK 0x02 /*evil hack from the docs...*/
#else
#include <unistd.h>
#endif

#include "NSReg.h"
#include "VerReg.h"
#include "nsBuildID.h"

char *TheRegistry = "registry"; 
char *Flist;

/* WARNING: build hackery */
#if defined(STANDALONE_REGISTRY) && !defined(XP_MAC) && !defined(XP_MACOSX)
long BUILDNUM = NS_BUILD_ID;
#endif


REGERR vr_ParseVersion(char *verstr, VERSION *result);

#if defined(XP_UNIX) && !defined(XP_MACOSX)

#ifdef STANDALONE_REGISTRY
extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_TRUNCATE_BIN );
    }

    return fh;
}
#endif /*STANDALONE_REGISTRY*/

extern void vr_findGlobalRegName ()
{
#ifndef STANDALONE_REGISTRY
    char *def = NULL;
    char *home = getenv("HOME");
    if (home != NULL) {
        def = (char *) XP_ALLOC(XP_STRLEN(home) + XP_STRLEN(DEF_REG)+1);
        if (def != NULL) {
          XP_STRCPY(def, home);
          XP_STRCAT(def, DEF_REG);
        }
    }
    if (def != NULL) {
        globalRegName = XP_STRDUP(def);
    } else {
        globalRegName = XP_STRDUP(TheRegistry);
    }
    XP_FREEIF(def);
#else
    globalRegName = XP_STRDUP(TheRegistry);
#endif /*STANDALONE_REGISTRY*/
}

char* vr_findVerRegName ()
{
    if ( verRegName != NULL )
        return verRegName;

#ifndef STANDALONE_REGISTRY
    {
        char *def = NULL;
        char *home = getenv("HOME");
        if (home != NULL) {
            def = (char *) XP_ALLOC(XP_STRLEN(home) + XP_STRLEN(DEF_VERREG)+1);
            if (def != NULL) {
                XP_STRCPY(def, home);
                XP_STRCAT(def, DEF_VERREG);
            }
        }
        if (def != NULL) {
            verRegName = XP_STRDUP(def);
        }
        XP_FREEIF(def);
    }
#else
    verRegName = XP_STRDUP(TheRegistry);
#endif /*STANDALONE_REGISTRY*/

    return verRegName;
}

#endif /*XP_UNIX*/

 /* ------------------------------------------------------------------
 *  BeOS STUBS
 * ------------------------------------------------------------------
 */

#ifdef XP_BEOS

#ifdef STANDALONE_REGISTRY
extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_TRUNCATE_BIN );
    }

    return fh;
}
#endif /*STANDALONE_REGISTRY*/

extern void vr_findGlobalRegName ()
{
#ifndef STANDALONE_REGISTRY
    char *def = NULL;
      char settings[1024];
      find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settings, sizeof(settings));
    if (settings != NULL) {
        def = (char *) XP_ALLOC(XP_STRLEN(settings) + XP_STRLEN(BEOS_REG)+1);
        if (def != NULL) {
          XP_STRCPY(def, settings);
          XP_STRCAT(def, BEOS_REG);
        }
    }
    if (def != NULL) {
        globalRegName = XP_STRDUP(def);
    } else {
        globalRegName = XP_STRDUP(TheRegistry);
    }
    XP_FREEIF(def);
#else
    globalRegName = XP_STRDUP(TheRegistry);
#endif /*STANDALONE_REGISTRY*/
}

char* vr_findVerRegName ()
{
    if ( verRegName != NULL )
        return verRegName;

#ifndef STANDALONE_REGISTRY
    {
        char *def = NULL;
        char settings[1024];
        find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settings, sizeof(settings));
        if (settings != NULL) {
            def = (char *) XP_ALLOC(XP_STRLEN(settings) + XP_STRLEN(BEOS_VERREG)+1);
            if (def != NULL) {
                XP_STRCPY(def, settings);
                XP_STRCAT(def, BEOS_VERREG);
            }
        }
        if (def != NULL) {
            verRegName = XP_STRDUP(def);
        }
        XP_FREEIF(def);
    }
#else
    verRegName = XP_STRDUP(TheRegistry);
#endif /*STANDALONE_REGISTRY*/

    return verRegName;
}

#endif /*XP_BEOS*/

#endif /* XP_UNIX || XP_OS2 */
