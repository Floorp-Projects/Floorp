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

/* this file contains stubs needed to build the registry routines
 * into a stand-alone library for use with our installers
 */

#include <stdio.h>
#include <string.h>

#ifndef STANDALONE_REGISTRY
#include "prtypes.h"
#endif

#include "vr_stubs.h"

#ifdef XP_MAC
#include <Folders.h>
#include <Script.h>
#include <stdlib.h>
#include <Errors.h>
#include "MoreFiles.h"
#include "FullPath.h"  /* For FSpLocationFromFullPath() */
#endif

extern char* globalRegName;
/* ------------------------------------------------------------------
 *  OS/2 STUBS
 * ------------------------------------------------------------------
 */
#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>

extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_WRITE_BIN );
    }

    return fh;
}

extern void vr_findGlobalRegName ()
{
    char    path[ CCHMAXPATH ];
    int     pathlen;
    XP_File fh = NULL;
    struct stat st;

#ifdef XP_OS2_HACK
    /*DSR050197 - at this point, I need some front-end call to get the install directory of*/
    /*communicator... for now I will let it default to the current directory...*/
#endif
    XP_STRCPY(path, ".");
    pathlen = strlen(path);

    if ( pathlen > 0 ) {
        XP_STRCPY( path+pathlen, "\\nsreg.dat" );
        globalRegName = XP_STRDUP(path);
    }
}
#endif /* XP_OS2 */


/* ------------------------------------------------------------------
 *  WINDOWS STUBS
 * ------------------------------------------------------------------
 */
#ifdef XP_PC
#include "windows.h"
#define PATHLEN 260

extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_WRITE_BIN );
    }

    return fh;
}

extern void vr_findGlobalRegName ()
{
    char    path[ PATHLEN ];
    int     pathlen;
   
    pathlen = GetWindowsDirectory(path, PATHLEN);
    if ( pathlen > 0 ) {
        XP_STRCPY( path+pathlen, "\\nsreg.dat" );
        globalRegName = XP_STRDUP(path);
    }
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

#endif /* XP_PC */


/* ------------------------------------------------------------------
 *  MACINTOSH STUBS
 * ------------------------------------------------------------------
 */

#ifdef XP_MAC
#include <Files.h>
#include "FullPath.h"

extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;
	OSErr	anErr;
	FSSpec	newFSSpec;
	
	anErr = FSpLocationFromFullPath(strlen(name), name, &newFSSpec);
	
	if (anErr == -43)
	{ 
		/* if file doesn't exist */
		anErr = FSpCreate(&newFSSpec, 'MOSS', 'REGS', smSystemScript);
	}
	else
	{
		/* there is not much to do here.  if we got noErr, the file exists.  If we did not get
		   noErr or -43, we are pretty hosed.
		*/
	}
		
    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN ); /* If/when we switch to MSL C Lib (gromit uses this), we might have to take out the Macro per bug #62382 */
	    else 
	    {
	       	/* should never get here! */
            fh = fopen( name, XP_FILE_WRITE_BIN );
     	}
	}
    return fh;
}

extern void vr_findGlobalRegName ()
{
    FSSpec	regSpec;
    OSErr	err;
    short	foundVRefNum;
    long	foundDirID;
    short	pathLen;
    Handle	thePath;
    int     bCreate = 0;
	Ptr		finalPath;
	
	err = FindFolder(kOnSystemDisk,'pref', false, &foundVRefNum, &foundDirID);

	if (!err) {

		err = FSMakeFSSpec(foundVRefNum, foundDirID, "\pNetscape Registry", &regSpec);

		if (err == -43) { /* if file doesn't exist */
			err = FSpCreate(&regSpec, 'MOSS', 'REGS', smSystemScript);
            bCreate = 1;
		}

		if (err == noErr) {
			err = FSpGetFullPath(&regSpec, &pathLen, &thePath);
			
			finalPath = NewPtrClear(pathLen+1);
			BlockMoveData(*thePath, finalPath, pathLen);
			globalRegName = XP_STRDUP(finalPath);			
            DisposePtr(finalPath);
		}
	}
}

/* Moves and renames a file or directory.
   Returns 0 on success, -1 on failure (errno contains mac error code).
 */
extern int nr_RenameFile(char *from, char *to)
{
	OSErr			err = -1;
	FSSpec			fromSpec;
	FSSpec			toSpec;
	FSSpec			destDirSpec;
	FSSpec			beforeRenameSpec;
	
	errno = 0; // reset errno
	
	if (from && to) {
    	err = FSpLocationFromFullPath(strlen(from), from, &fromSpec);
    	if (err != noErr) goto exit;
    	
    	err = FSpLocationFromFullPath(strlen(to), to, &toSpec);
        if (err != noErr && err != fnfErr) goto exit;
    	
    	// make an FSSpec for the destination directory
		err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, nil, &destDirSpec);
    	if (err != noErr) goto exit; // parent directory must exist

		// move it to the directory specified
    	err = FSpCatMove(&fromSpec, &destDirSpec);
    	if (err != noErr) goto exit;
	    
	    // make a new FSSpec for the file or directory in its new location	
		err = FSMakeFSSpec(toSpec.vRefNum, toSpec.parID, fromSpec.name, &beforeRenameSpec);
    	if (err != noErr) goto exit;
    	
    	// rename the file or directory
    	err = FSpRename(&beforeRenameSpec, toSpec.name);
	}
		
	exit:
	if (err != noErr)
		errno = err;
	return (err == noErr ? 0 : -1);
}


#if 1
/* Uncomment the following for older Mac build environments
 * that don't support these functions
 */
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

int strcasecmp(const char *str1, const char *str2)
{
	char 	currentChar1, currentChar2;

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
	char 	currentChar1, currentChar2;

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
#endif /* 0 */

#endif /* XP_MAC */


/* ------------------------------------------------------------------
 *  UNIX STUBS
 * ------------------------------------------------------------------
 */

/*allow OS/2 to use this main to test...*/
#if defined(XP_UNIX) || defined(XP_OS2)

#include <stdlib.h>
#ifdef XP_OS2
#include <io.h>
#define W_OK 0x02 /*evil hack from the docs...*/
#else
#include <unistd.h>
#endif
#include "NSReg.h"
#include "VerReg.h"

char *TheRegistry = "registry"; 
char *Flist;

/* WARNING: build hackery */
#ifdef STANDALONE_REGISTRY
long BUILDNUM =
#include "../../../build/build_number"
;
#endif


REGERR vr_ParseVersion(char *verstr, VERSION *result);
int main(int argc, char *argv[]);

#ifdef XP_UNIX

#define DEF_REG "/.netscape/registry"

extern XP_File vr_fileOpen (const char *name, const char * mode)
{
    XP_File fh = NULL;
    struct stat st;

    if ( name != NULL ) {
        if ( stat( name, &st ) == 0 )
            fh = fopen( name, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( name, XP_FILE_WRITE_BIN );
    }

    return fh;
}

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
        globalRegName = TheRegistry;
    }
    XP_FREEIF(def);
#else
    globalRegName = TheRegistry;
#endif
}

#endif


#ifdef STANDALONE_REGISTRY

int main(int argc, char *argv[])
{
	XP_File fh;
	char	*entry;
	char	*p;
    char    *v;
	char	buff[1024];
	char	name[MAXREGPATHLEN+1];
	char	path[MAXREGPATHLEN+1];
    char    ver[MAXREGPATHLEN+1];

	if ( argc >= 3 )
	{
		TheRegistry = argv[1];
		Flist = argv[2];
	}
	else
	{
		fprintf(stderr, "Usage: %s RegistryName FileList\n", argv[0]);
        fprintf(stderr, "    The FileList file contains lines with comma-separated fields:\n");
        fprintf(stderr, "    <regItemName>,<version>,<full filepath>\n");
		exit (1);
	}

    /* tmp use of buff to get the registry directory, which must be
     * the navigator home directory.  Preserve the slash to match
     * FE_GetDirectoryPath() called during navigator set-up
     */


    strcpy(buff, TheRegistry);
    if ( p = strrchr( buff, '/' ))
    {
       char pwd[1024];

       *(p+1) = '\0';
       getcwd(pwd, sizeof(pwd));
       chdir(buff); getcwd(buff, sizeof(buff));
       chdir(pwd); 
    }
    else
    {
       getcwd(buff, sizeof(buff));
    }
    strcat(buff, "/");


    NR_StartupRegistry();
    VR_SetRegDirectory(buff);


	if ( -1 == (access( TheRegistry, W_OK )) ) {
        sprintf(ver,"4.50.0.%ld",BUILDNUM);
		VR_CreateRegistry("Communicator", buff, ver);
    }


	if ( !(fh = fopen( Flist, "r" )) )
	{
		fprintf(stderr, "%s: Cannot open \"%s\"\n", argv[0], Flist);
		exit (1);
	}

	while ( fgets ( buff, 1024, fh ) )
	{
		if (  *(entry = &buff[strlen(buff)-1]) == '\n' )
			*entry = '\0';

		entry = strchr(buff, ',');
		strcpy(name, strtok(buff, ","));
        strcpy(ver,  strtok( NULL, ","));
        strcpy(path, strtok( NULL, ","));

        v = ver;
        while (*v && *v == ' ')
            v++;

		p = path;
		while (*p && *p == ' ')
			p++;

		VR_Install ( name, p, v, FALSE );
	}
	fclose( fh );
	return 0;
}

#endif /* STANDALONE_REGISTRY */

#endif /* XP_UNIX || XP_OS2 */


