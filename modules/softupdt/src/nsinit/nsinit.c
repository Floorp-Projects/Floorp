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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <windows.h>


#define    BSIZE    16384    
#define    KSIZE    1024    
#define    COPYSIZE 32000

#define    OK       1
#define    ERROR    0


#ifdef DEBUG_dveditz
#define DBG(msg) MessageBox((HWND)NULL, (msg), "NS-Init Debug message", 0);
#else
#define DBG(msg) ((void)0)
#endif


//--- Macros for readability ---
#define SECTION    "Rename"
#define INIFILE    "WININIT.INI"

#define GetRenameString(key, buf, bufsize) \
    GetPrivateProfileString( SECTION, (key), "", (buf), (bufsize), INIFILE)

#define DeleteRenameString(key) \
    WritePrivateProfileString( SECTION, (key), NULL, INIFILE )

// returns buf filled with null separated list of key names
// terminated by a double null.
#define GetRenameKeys(buf, bufsize) \
    GetRenameString( NULL, (buf), (bufsize) )



//--- prototypes ---
int FileReplace( LPCSTR src, LPCSTR dest );
int RenameFile( LPCSTR src, LPCSTR dest );

char *copybuf;

int PASCAL WinMain(HINSTANCE hinst, HINSTANCE hPrevInst,
    LPSTR lpCmdLine, int nCmdShow)
{
    char    *Buff;
    char    *Src;

    Buff = (char*)malloc(BSIZE);
    Src  = (char*)malloc(KSIZE);
    copybuf = (char*)malloc(COPYSIZE);

    if ( Buff == NULL || Src == NULL || copybuf == NULL )
        return -1;
     
    // extract all the keys
    if ( GetRenameKeys( Buff, BSIZE) ) {
        char *pDest = Buff;

        // for each key do the specified file swap
        while ( *pDest ) {
            if ( GetRenameString( pDest, Src, KSIZE) )  {
                if ( FileReplace( Src, pDest) ) {
                    DeleteRenameString( pDest );
                }
            }
            // get the next key
            pDest += strlen(pDest)+1;
        }
    }

    // if there are no more keys left in the WININIT.INI [Rename] section then
    // take the utility out of the WIN.INI run list.  We re-check the INI file
    // (rather than set a flag) because buff may not have been big enough.
    if ( 0 == GetRenameKeys( Buff, BSIZE ) ) {
        DBG("[Rename] section empty");

        if ( GetProfileString( "Windows", "Run", "", Buff, BSIZE ) ) {
            int namelen, taillen;
            char *pName, *pTail;

            DBG(Buff);
            namelen = GetModuleFileName( hinst, Src, KSIZE );
            strupr(Src);
            strupr(Buff);

            pName = strstr( Buff, Src );
            if ( pName != NULL ) {
                if ( *(pName+namelen) == '\0' ) {
                    // there are no trailing commands: chop off the end
                    while( pName > Buff && *pName != ',' ) {
                        pName--;
                    }
                    *pName = '\0';
                }
                else {
                    pTail = pName+namelen;
                    while( *pTail && *pTail == ' ' ) 
                        pTail++;
                    if ( *pTail == ',' )
                        pTail++;
                    taillen = strlen(pTail)+1;  // copy '\0' too
                    memmove( pName, pTail, taillen );
                }
                DBG(Buff);
                WriteProfileString( "Windows", "Run", Buff );
            }
        }
    }

    free(Buff);
    free(Src);
    free(copybuf);
    return 0;
}



int FileReplace ( LPCSTR Src, LPCSTR Dest )
{
    char buf[KSIZE];
    struct stat st;
    int status;

#ifdef DEBUG_dveditz
    sprintf (buf,"Renaming %s to %s\n", Src, Dest);
    DBG(buf);
#endif


    if ( stat( Src, &st ) != 0 ) {
        DBG("Missing sourcefile");
        // Srcfile is missing, nothing to do
        return OK;
    }

    if ( stricmp( Dest, "NUL" ) == 0 ) {
        // special case: delete Srcfile 
        // (part of MS spec for WININIT.INI -- ASD won't generate these)
        DBG("Special delete case");
        return ( 0 == unlink( Src ) );
    }


    if ( stat( Dest, &st ) != 0 ) {
        DBG("Dest doesn't exist");
        // Dest doesn't exist
        status = RenameFile( Src, Dest );
    }
    else {
        if ( NULL == tmpnam(buf) || !RenameFile( Dest, buf ) ) {
            DBG("tmpfile error");
            status = ERROR;
        }
        else {
            status = RenameFile( Src, Dest );
            if ( status == OK ) {
                DBG("rename success");
                unlink(buf);
            }
            else {
                // put old file back
                DBG("rename failure");
                RenameFile( buf, Dest );
            }
        }
    }

    return status;
}




int RenameFile ( LPCSTR in, LPCSTR out)
{
    FILE    *ifp, *ofp;
    int     rbytes, wbytes;
    int     status = OK;

    if (!in || !out)
        return ERROR;

    // try a rename first
    if ( 0 == rename( in, out ) )
        return OK;
    else if ( errno != EXDEV )
        return ERROR;

    // can't rename across drives so do ugly copy
    DBG("trying copy");
    ifp = fopen (in, "rb");
    if (ifp == NULL) {
        return ERROR;
    }

    ofp = fopen (out, "wb");
    if (ofp == NULL) {
        fclose (ifp);
        return ERROR;
    }

    while ((rbytes = fread (copybuf, 1, COPYSIZE, ifp)) > 0) {
        if ((rbytes < COPYSIZE) && (ferror(ifp) != 0)) {
            DBG("Read error");
            status = ERROR;
            break;
        }

        if ( (wbytes = fwrite (copybuf, 1, rbytes, ofp)) < rbytes ) {
            DBG("Write error");
            status = ERROR;
            break;
        }
    }

    fclose (ofp);
    fclose (ifp);

    if ( status == ERROR )
        unlink(out);
    else
        unlink(in);

    return status;
}
