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
#ifdef STANDALONE_REGISTRY

#include <stdio.h>
#include <string.h>
#include "vr_stubs.h"

#ifdef XP_MAC
#include <Folders.h>
#include <Script.h>
#include <size_t.h>
#include <stdlib.h>
#endif

/* ------------------------------------------------------------------
 *  OS/2 STUBS
 * ------------------------------------------------------------------
 */
#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
extern XP_File VR_StubOpen (const char * mode)
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

        if ( stat( path, &st ) == 0 )
            fh = fopen( path, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( path, XP_FILE_WRITE_BIN );
    }

    return fh;
}

/* ------------------------------------------------------------------
 *  WINDOWS STUBS
 * ------------------------------------------------------------------
 */
#elif XP_PC
#include "windows.h"
#define PATHLEN 260

extern XP_File VR_StubOpen (const char * mode)
{
    char    path[ PATHLEN ];
    int     pathlen;
    XP_File fh = NULL;
    struct stat st;

    pathlen = GetWindowsDirectory(path, PATHLEN);
    if ( pathlen > 0 ) {
        XP_STRCPY( path+pathlen, "\\nsreg.dat" );

        if ( stat( path, &st ) == 0 )
            fh = fopen( path, XP_FILE_UPDATE_BIN );
        else
            fh = fopen( path, XP_FILE_WRITE_BIN );
    }

    return fh;
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
extern XP_File VR_StubOpen (const char *mode)
{

    XP_File fh = NULL;
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
			err = FSpCreate(&regSpec, '    ', '    ', smSystemScript);
            bCreate = 1;
		}

		if (err == noErr) {
			err = FSpGetFullPath(&regSpec, &pathLen, &thePath);
			
			finalPath = NewPtrClear(pathLen+1);
			BlockMoveData(*thePath, finalPath, pathLen);
						
            if (bCreate)
			    fh = fopen( finalPath, XP_FILE_WRITE_BIN );
            else 
                fh = fopen( finalPath, "w+b" ); /* this hack to support CW11 MSL C Lib fopen update mode */
             
            DisposePtr(finalPath);
		}
	}
								
    return fh;
}

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

char *TheRegistry; 
char *Flist;
/* WARNING: build hackery */
long BUILDNUM =
#include "../../../build/build_number"
;


REGERR vr_ParseVersion(char *verstr, VERSION *result);
int main(int argc, char *argv[]);

#ifdef XP_UNIX
XP_File VR_StubOpen (const char * mode)
{
	XP_File fh;
    struct stat st;

    if ( stat( TheRegistry, &st ) == 0 )
        fh = fopen( TheRegistry, XP_FILE_UPDATE_BIN );
    else
        fh = fopen( TheRegistry, XP_FILE_WRITE_BIN );

	return fh;
}
#endif

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

	if ( -1 == (access( TheRegistry, W_OK )) ) {
        sprintf(ver,"4.1.0.%ld",BUILDNUM);
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
#endif /* XP_UNIX || XP_OS2 */

#endif /* STANDALONE_REGISTRY */
