/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*--------------------------------------------------------------------
 *  NSUnbind
 *
 *  A utility to remove Bound Imports info from Win32 images
 *
 *       nsunbind <exename>
 *
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <winnt.h>

/*--------------------------------------
 *  types
 *------------------------------------*/

typedef unsigned long   uint32;
typedef long            int32;
typedef unsigned short  uint16;
typedef short           int16;
typedef unsigned char   uchar;
typedef unsigned char   XP_Bool;
typedef unsigned char   uint8;
typedef FILE*           XP_File;

#include "gdiff.h"


/*--------------------------------------
 *  prototypes
 *------------------------------------*/

void    unbind( char * );


/*--------------------------------------
 *  main
 *------------------------------------*/

int main( int argc, char *argv[] )
{
    int i;

    /* Parse command line */

    if ( argc < 2 ) {
        fprintf( stderr, "\n    usage: nsunbind file ...\n\n" );
    }
    else {
        for ( i = 1; i < argc; i++ ) 
        {
            fprintf( stderr, "NSUNBIND: %s\n", argv[i] );
            unbind( argv[i] );
        }
    }

    return 0;
}


void unbind( char* fname )
{
    int     i;
    DWORD   nRead;
    PDWORD  pOrigThunk;
    PDWORD  pBoundThunk;
    FILE    *fh;
    char    *buf;
    BOOL    bModified = FALSE;
    BOOL    bImports = FALSE;

    IMAGE_DOS_HEADER            mz;
    IMAGE_NT_HEADERS            nt;
    IMAGE_SECTION_HEADER        sec;

    PIMAGE_DATA_DIRECTORY       pDir;
    PIMAGE_IMPORT_DESCRIPTOR    pImp;


    fh = fopen( fname, "r+b" );
    if ( fh == NULL ) {
        fprintf( stderr, "    ERROR: Couldn't open %s\n", fname );
        return;
    }


    /* read and validate the MZ header */

    nRead = fread( &mz, 1, sizeof(mz), fh );
    if ( nRead != sizeof(mz) ) {
        fprintf( stderr, "    ERROR: Unexpected EOF reading DOS header\n" );
        goto bail;
    }
    else if ( mz.e_magic != IMAGE_DOS_SIGNATURE ) {
        fprintf( stderr, "    ERROR: Invalid DOS header\n" );
        goto bail;
    }



    /* read and validate the NT header */

    fseek( fh, mz.e_lfanew, SEEK_SET );
    nRead = fread( &nt, 1, sizeof(nt), fh );
    if ( nRead != sizeof(nt) ) {
        fprintf( stderr, "    ERROR: Unexpected EOF reading PE headers\n" );
        goto bail;
    }

    if ( nt.Signature != IMAGE_NT_SIGNATURE ) {
        fprintf( stderr, "    ERROR: Not a Win32 Portable Executable (PE) file\n" );
        goto bail;
    }
    else if ( nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ) {
        fprintf( stderr, "    ERROR: Invalid PE Optional Header\n" );
        goto bail;
    }



    /* find .idata section */

    for (i = nt.FileHeader.NumberOfSections; i > 0; i--)
    {
        nRead = fread( &sec, 1, sizeof(sec), fh );
        if ( nRead != sizeof(sec) ) {
            fprintf( stderr, "    ERROR: EOF reading section headers\n" );
            goto bail;
        }

        if ( memcmp( sec.Name, ".idata", 6 ) == 0 ) {
            bImports = TRUE;
            break;
        }
    }



    /* Zap any binding in the imports section */

    if ( bImports ) 
    {
        buf = (char*)malloc( sec.SizeOfRawData );
        if ( buf == NULL ) {
            fprintf( stderr, "    ERROR: Memory allocation problem\n" );
            goto bail;
        }

        fseek( fh, sec.PointerToRawData, SEEK_SET );
        nRead = fread( buf, 1, sec.SizeOfRawData, fh );
        if ( nRead != sec.SizeOfRawData ) {
            fprintf( stderr, "    ERROR: Unexpected EOF reading .idata\n" );
            goto bail;
        }

        pImp = (PIMAGE_IMPORT_DESCRIPTOR)buf;
        while ( pImp->OriginalFirstThunk != 0 )
        {
            if ( pImp->TimeDateStamp != 0 || pImp->ForwarderChain != 0 )
            {
                /* found a bound .DLL */
                pImp->TimeDateStamp = 0;
                pImp->ForwarderChain = 0;
                bModified = TRUE;

                pOrigThunk = (PDWORD)(buf + (DWORD)(pImp->OriginalFirstThunk) - sec.VirtualAddress);
                pBoundThunk = (PDWORD)(buf + (DWORD)(pImp->FirstThunk) - sec.VirtualAddress);

                for ( ; *pOrigThunk != 0; pOrigThunk++, pBoundThunk++ ) {
                    *pBoundThunk = *pOrigThunk;
                }

                fprintf( stdout, "    %s bindings removed\n",
                    buf + (pImp->Name - sec.VirtualAddress) );
            }

            pImp++;
        }

        if ( bModified ) 
        {
            /* it's been changed, write out the section */
            fseek( fh, sec.PointerToRawData, SEEK_SET );
            fwrite( buf, 1, sec.SizeOfRawData, fh );
        }

        free( buf );
    }



    /* Check for a Bound Import Directory in the headers */
    
    pDir = &nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT];
    if ( pDir->VirtualAddress != 0 ) 
    {
        /* we've got one, so stomp it */
        fprintf( stdout, "    Deleting NT Bound Import Directory\n" );

        buf = (char*)calloc( pDir->Size, 1 );
        if ( buf == NULL ) {
            fprintf( stderr, "    ERROR: Memory allocation problem\n" );
            goto bail;
        }

        fseek( fh, pDir->VirtualAddress, SEEK_SET );
        fwrite( buf, pDir->Size, 1, fh );
        free( buf );

        pDir->VirtualAddress = 0;
        pDir->Size = 0;

        bModified = TRUE;
    }



    /* Write out changed headers if necessary */
    
    if ( bModified )
    {
        /* zap checksum since it's now invalid */
        nt.OptionalHeader.CheckSum = 0;

        fseek( fh, mz.e_lfanew, SEEK_SET );
        nRead = fwrite( &nt, 1, sizeof(nt), fh );
    }
    else
    {
        fprintf( stdout, "    Nothing to unbind\n");
    }

bail:
    fclose(fh);
}