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

#include "xp_mcom.h"
#include "zlib.h"
#include "nsSoftUpdateEnums.h"

#include "gdiff.h"

#ifdef WIN32
#include <windows.h>
#include <winnt.h>
#endif

#ifdef SUNOS4
#include <unistd.h>  /* for SEEK_SET */
#endif

#ifdef XP_MAC
#include "Errors.h"	/* for fnfErr */
#endif

/*--------------------------------------
 *  constants / macros
 *------------------------------------*/

#define BUFSIZE     32768
#define OPSIZE      1
#define MAXCMDSIZE  12
#define SRCFILE     0
#define OUTFILE     1

#define getshort(s) (uint16)( ((uchar)*(s) << 8) + ((uchar)*((s)+1)) )

#define getlong(s)  \
            (uint32)( ((uchar)*(s) << 24) + ((uchar)*((s)+1) << 16 ) + \
                      ((uchar)*((s)+2) << 8) + ((uchar)*((s)+3)) )



/*--------------------------------------
 *  prototypes
 *------------------------------------*/

static int32   gdiff_ApplyPatch( pDIFFDATA dd );
static int32   gdiff_add( pDIFFDATA dd, uint32 count );
static int32   gdiff_copy( pDIFFDATA dd, uint32 position, uint32 count );
static int32   gdiff_getdiff( pDIFFDATA dd, uchar *buffer, uint32 length );
static int32   gdiff_parseHeader( pDIFFDATA dd );
static int32   gdiff_validateFile( pDIFFDATA dd, int file );
static int32   gdiff_valCRC32( pDIFFDATA dd, XP_File fh, uint32 chksum );

#ifdef WIN32
static XP_Bool su_unbind( char *oldsrc, XP_FileType, char *newsrc, XP_FileType);
#endif



/*--------------------------------------------------------------
 *  SU_PatchFile()
 *
 *  Applies a GDIFF "patchfile" to "srcfile", creating
 *  the new "outfile" to contain the results
 *--------------------------------------------------------------
 */
int32 SU_PatchFile( char* srcfile, XP_FileType srctype, char* patchfile, 
                    XP_FileType patchtype, char* outfile, XP_FileType outtype )
{
    DIFFDATA    *dd;
    int32       status 			= 	GDIFF_ERR_MEM;
    char        *realfile 		= 	srcfile;
    XP_FileType realtype 		= 	srctype;

    char        *tmpurl		= NULL;
    
    dd = XP_CALLOC( 1, sizeof(DIFFDATA) );
    if ( dd != NULL ) 
    {
        dd->databuf = XP_ALLOC(BUFSIZE);
        if ( dd->databuf == NULL ) {
            status = GDIFF_ERR_MEM;
            goto cleanup;
        }
        dd->bufsize = BUFSIZE;


        /* validate the patch header and check for special instructions */

        dd->fDiff = XP_FileOpen( patchfile, patchtype, XP_FILE_READ_BIN );

        if ( dd->fDiff != NULL ) {
            status = gdiff_parseHeader( dd );
        }
        else {
            status = GDIFF_ERR_ACCESS;
        }
        
#ifdef WIN32
        /* unbind Win32 images */
        if ( dd->bWin32BoundImage && status == GDIFF_OK ) {
            tmpurl = WH_TempName( xpURL, NULL );
            if ( tmpurl != NULL ) {
                if (su_unbind( srcfile, srctype, tmpurl, xpURL )) {
                    realfile = tmpurl;
                    realtype = xpURL;
                    
                }
            }
            else
                status = GDIFF_ERR_MEM;
        }
#endif

#ifdef XP_MAC

	if ( dd->bMacAppleSingle && status == GDIFF_OK ) 
	{
		FSSpec 	srcFSSpec, tmpFSSpec;
		char	*srcMacPath, *tmpMacPath;		
	
        /* We need a tmp file so that we can AppleSingle the src file */
        tmpurl 		= WH_TempName( xpURL, NULL );
        
        /* Put the source file into a mac file path */
		tmpMacPath	= WH_FileName( tmpurl, xpURL );
		
		/* Get the mac filepath for srcMacPath */
		srcMacPath 	= WH_FileName( srcfile, srctype );
		
		/* bail if we did not get our files! */
        if ( tmpMacPath == NULL || srcMacPath == NULL) 
        {
        	XP_FREEIF( srcMacPath );
	   		XP_FREEIF( tmpMacPath );
	   		
	   		goto cleanup;
        }
                
		/* Get the source FSSpec */
		status = FSpLocationFromFullPath(strlen(srcMacPath), srcMacPath, &srcFSSpec);
        if (status != noErr) 
        {
        	XP_FREEIF( srcMacPath );
	   		XP_FREEIF( tmpMacPath );

        	goto cleanup;
        }
        
        
        /* Get the temporary FSSpec.  THis is where the encoded file will go */
        status = FSpLocationFromFullPath(strlen(tmpMacPath), tmpMacPath, &tmpFSSpec);
        if (status != noErr && status != fnfErr) 
        {
        	XP_FREEIF( srcMacPath );
	   		XP_FREEIF( tmpMacPath );

        	goto cleanup;
        }
        
       /* Encode! */
		status = PAS_EncodeFile(&srcFSSpec, &tmpFSSpec);
			
		if (status == noErr)
		{
			/* set */
			realfile 		= tmpurl;
            realtype 		= xpURL;
		}
		
		XP_FREEIF( srcMacPath );
   		XP_FREEIF( tmpMacPath );
	}
#endif 


        if ( status != GDIFF_OK )
            goto cleanup;


        /* apply the patch to the source file */

        dd->fSrc   = XP_FileOpen( realfile, realtype, XP_FILE_READ_BIN );
        dd->fOut   = XP_FileOpen( outfile, outtype, XP_FILE_TRUNCATE_BIN );

        if ( dd->fSrc != NULL && dd->fOut != NULL )
        {
            status = gdiff_validateFile( dd, SRCFILE );
			
			/* specify why diff failed */
			if (status == GDIFF_ERR_CHECKSUM)
				status = GDIFF_ERR_CHECKSUM_TARGET;
			
            if ( status == GDIFF_OK )
                status = gdiff_ApplyPatch( dd );

            if ( status == GDIFF_OK )
                status = gdiff_validateFile( dd, OUTFILE );

			/* specify why diff failed */
			if (status == GDIFF_ERR_CHECKSUM)
				status = GDIFF_ERR_CHECKSUM_RESULT;
			
        }
        else 
		{
            status = GDIFF_ERR_ACCESS;
        }
    }


#ifdef XP_MAC
	if ( dd->bMacAppleSingle && status == GDIFF_OK ) 
	{
		FSSpec 	outFSSpec;
		char	*outMacPath;
		
		char*	anotherURL;
		char*	anotherURLMacPath;
		FSSpec	anotherFSSpec;
		
		Str63	fileName;
		
		
		/* We need a temp file so that we can decode somewhere */
		anotherURL 	= WH_TempName( xpURL, NULL );
		
		/* Get the mac file path for anotherURL */
		anotherURLMacPath	= WH_FileName( anotherURL, xpURL );
		
		/* Get the mac file path for outfile */
		outMacPath 	= WH_FileName( outfile, outtype );

		if ( anotherURLMacPath == NULL || outMacPath == NULL) 
        {
        	status = mFulErr;
		   	goto macPostProcessCleanup;
        }
			
			
		/* Get the out FSSpec.  This is where the decoded will end up */

		status = FSpLocationFromFullPath(strlen(outMacPath), outMacPath, &outFSSpec);
       
        if (status != noErr) 
        {
		   	goto macPostProcessCleanup;
        }
			
        /* Get the anotherURL FSSpec.  This is where the decoded will first be placed */

        status = FSpLocationFromFullPath(strlen(anotherURLMacPath), anotherURLMacPath, &anotherFSSpec);
        if (status != noErr && status != fnfErr)
		{
		   	goto macPostProcessCleanup;
        }
			
		/* Close the out file so that we can read it */
			
		XP_FileClose( dd->fOut );
		dd->fOut = NULL;

		status =  PAS_DecodeFile(&outFSSpec, &anotherFSSpec);
		if (status != noErr)
		{
		   	goto macPostProcessCleanup;
        }
			
		/* Delete the outfile so that we can replace */				
		XP_FileRemove( outfile, outtype );
		
			
		/* get the name of the file */
		BlockMove(outFSSpec.name, fileName, sizeof(Str63) );

		/* We want the parent */
		outFSSpec.name[0] = 0;
		
		/* Copy the output of the decodef to the outfile location passed to us */
		status = FSpFileCopy(&anotherFSSpec, &outFSSpec, fileName, NULL, 0,true);
		
		if (status != noErr)
		{
		   	goto macPostProcessCleanup;
        }
       
       
macPostProcessCleanup:       
       
		XP_FREEIF( outMacPath );
		XP_FREEIF( anotherURLMacPath );
			
		if(anotherURL) 
		{
			XP_FileRemove( anotherURL, xpURL );
			XP_FREEIF( anotherURL );
		}
		
		if (status != noErr)
		{
		   	goto cleanup;
        }
	}
	
#endif 


cleanup:
    if ( dd != NULL ) 
    {
        if ( dd->fSrc != NULL )
            XP_FileClose( dd->fSrc );


        if ( dd->fDiff != NULL )
            XP_FileClose( dd->fDiff );
		
		if ( dd->fOut != NULL )
            XP_FileClose( dd->fOut );

		if ( status != GDIFF_OK )
		   	XP_FileRemove( outfile, outtype );
		        
		XP_FREEIF( dd->databuf );
        XP_FREEIF( dd->oldChecksum );
        XP_FREEIF( dd->newChecksum );
        XP_FREE(dd);
    }

	if ( tmpurl != NULL ) {
        XP_FileRemove( tmpurl, xpURL );
        XP_FREE( tmpurl );
    }
    
	/* lets map any GDIFF error to nice SU errors */

	switch (status)
	{
        case GDIFF_OK:
                break;
		case GDIFF_ERR_HEADER:
		case GDIFF_ERR_BADDIFF:
		case GDIFF_ERR_OPCODE:
		case GDIFF_ERR_CHKSUMTYPE:
				status = SUERR_PATCH_BAD_DIFF;
				break;
		case GDIFF_ERR_CHECKSUM_TARGET:
				status = SUERR_PATCH_BAD_CHECKSUM_TARGET;
				break;
		case GDIFF_ERR_CHECKSUM_RESULT:
				status = SUERR_PATCH_BAD_CHECKSUM_RESULT;
				break;
		case GDIFF_ERR_OLDFILE:
		case GDIFF_ERR_ACCESS:
		case GDIFF_ERR_MEM:
		case GDIFF_ERR_UNKNOWN:
		default:
				status = SUERR_UNEXPECTED_ERROR;
				break;
	}

    return status;
}



/*---------------------------------------------------------
 *  gdiff_ApplyPatch()
 *
 *  Combines patch data with source file to produce the
 *  new target file.  Assumes all three files have been
 *  opened, GDIFF header read, and all other setup complete
 *
 *  The GDIFF patch is processed sequentially which random
 *  access is neccessary for the source file.
 *---------------------------------------------------------
 */
static
int32 gdiff_ApplyPatch( pDIFFDATA dd )
{
    int32   err;
    XP_Bool done;
    uint32  position;
    uint32  count;
    uchar   opcode;
    uchar   cmdbuf[MAXCMDSIZE];

    done = FALSE;
    while ( !done ) {
        err = gdiff_getdiff( dd, &opcode, OPSIZE );
        if ( err != GDIFF_OK )
            break;

        switch (opcode)
        {
            case ENDDIFF:
                done = TRUE;
                break;

            case ADD16:
                err = gdiff_getdiff( dd, cmdbuf, ADD16SIZE );
                if ( err == GDIFF_OK ) {
                    err = gdiff_add( dd, getshort( cmdbuf ) );
                }
                break;

            case ADD32:
                err = gdiff_getdiff( dd, cmdbuf, ADD32SIZE );
                if ( err == GDIFF_OK ) {
                    err = gdiff_add( dd, getlong( cmdbuf ) );
                }
                break;

            case COPY16BYTE:
                err = gdiff_getdiff( dd, cmdbuf, COPY16BYTESIZE );
                if ( err == GDIFF_OK ) {
                    position = getshort( cmdbuf );
                    count = *(cmdbuf + sizeof(short));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY16SHORT:
                err = gdiff_getdiff( dd, cmdbuf, COPY16SHORTSIZE );
                if ( err == GDIFF_OK ) {
                    position = getshort( cmdbuf );
                    count = getshort(cmdbuf + sizeof(short));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY16LONG:
                err = gdiff_getdiff( dd, cmdbuf, COPY16LONGSIZE );
                if ( err == GDIFF_OK ) {
                    position = getshort( cmdbuf );
                    count =  getlong(cmdbuf + sizeof(short));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY32BYTE:
                err = gdiff_getdiff( dd, cmdbuf, COPY32BYTESIZE );
                if ( err == GDIFF_OK ) {
                    position = getlong( cmdbuf );
                    count = *(cmdbuf + sizeof(long));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY32SHORT:
                err = gdiff_getdiff( dd, cmdbuf, COPY32SHORTSIZE );
                if ( err == GDIFF_OK ) {
                    position = getlong( cmdbuf );
                    count = getshort(cmdbuf + sizeof(long));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY32LONG:
                err = gdiff_getdiff( dd, cmdbuf, COPY32LONGSIZE );
                if ( err == GDIFF_OK ) {
                    position = getlong( cmdbuf );
                    count = getlong(cmdbuf + sizeof(long));
                    err = gdiff_copy( dd, position, count );
                }
                break;

            case COPY64:
                /* we don't support 64-bit file positioning yet */
                err = GDIFF_ERR_OPCODE;
                break;

            default:
                err = gdiff_add( dd, opcode );
                break;
        }

        if ( err != GDIFF_OK )
            done = TRUE;
    }

    /* return status */
    return (err);
}



/*---------------------------------------------------------
 *  gdiff_add()
 *
 *  append "count" bytes from diff file to new file
 *---------------------------------------------------------
 */
static
int32 gdiff_add( pDIFFDATA dd, uint32 count )
{
    int32   err = GDIFF_OK;
    uint32  nRead;
    uint32  chunksize;

    while ( count > 0 ) {
        chunksize = ( count > dd->bufsize) ? dd->bufsize : count;
        nRead = XP_FileRead( dd->databuf, chunksize, dd->fDiff );
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_BADDIFF;
            break;
        }

        XP_FileWrite( dd->databuf, chunksize, dd->fOut );

        count -= chunksize;
    }

    return (err);
}



/*---------------------------------------------------------
 *  gdiff_copy()
 *
 *  copy "count" bytes from "position" in source file
 *---------------------------------------------------------
 */
static
int32 gdiff_copy( pDIFFDATA dd, uint32 position, uint32 count )
{
    int32 err = GDIFF_OK;
    uint32 nRead;
    uint32 chunksize;

    XP_FileSeek( dd->fSrc, position, SEEK_SET );

    while ( count > 0 ) {
        chunksize = (count > dd->bufsize) ? dd->bufsize : count;

        nRead = XP_FileRead( dd->databuf, chunksize, dd->fSrc );
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_OLDFILE;
            break;
        }

        XP_FileWrite( dd->databuf, chunksize, dd->fOut );

        count -= chunksize;
    }

    return (err);
}



/*---------------------------------------------------------
 *  gdiff_getdiff()
 *
 *  reads the next "length" bytes of the diff into "buffer"
 *
 *  XXX: need a diff buffer to optimize reads!
 *---------------------------------------------------------
 */
static
int32 gdiff_getdiff( pDIFFDATA dd, uchar *buffer, uint32 length )
{
    uint32 bytesRead;

    bytesRead = XP_FileRead( buffer, length, dd->fDiff );
    if ( bytesRead != length )
        return GDIFF_ERR_BADDIFF;

    return GDIFF_OK;
}



/*---------------------------------------------------------
 *  gdiff_parseHeader()
 *
 *  reads and validates the GDIFF header info
 *---------------------------------------------------------
 */
static
int32 gdiff_parseHeader( pDIFFDATA dd )
{
    int32   err = GDIFF_OK;
    uint8   cslen;
    uint8   oldcslen;
    uint8   newcslen;
    uint32  nRead;
    uchar   header[GDIFF_HEADERSIZE];

    /* Read the fixed-size part of the header */

    nRead = XP_FileRead( header, GDIFF_HEADERSIZE, dd->fDiff );
    if ( nRead != GDIFF_HEADERSIZE ||
         XP_MEMCMP( header, GDIFF_MAGIC, GDIFF_MAGIC_LEN ) != 0  ||
         header[GDIFF_VER_POS] != GDIFF_VER )
    {
        err = GDIFF_ERR_HEADER;
    }
    else
    {
        /* get the checksum information */

        dd->checksumType = header[GDIFF_CS_POS];
        cslen = header[GDIFF_CSLEN_POS];

        if ( cslen > 0 )
        {
            oldcslen = cslen / 2;
            newcslen = cslen - oldcslen;
            XP_ASSERT( newcslen == oldcslen );

            dd->checksumLength = oldcslen;
            dd->oldChecksum = XP_ALLOC(oldcslen);
            dd->newChecksum = XP_ALLOC(newcslen);

            if ( dd->oldChecksum != NULL && dd->newChecksum != NULL )
            {
                nRead = XP_FileRead( dd->oldChecksum, oldcslen, dd->fDiff );
                if ( nRead == oldcslen )
                {
                    nRead = XP_FileRead( dd->newChecksum, newcslen, dd->fDiff);
                    if ( nRead != newcslen ) {
                        err = GDIFF_ERR_HEADER;
                    }
                }
                else {
                    err = GDIFF_ERR_HEADER;
                }
            }
            else {
                err = GDIFF_ERR_MEM;
            }
        }


        /* get application data, if any */

        if ( err == GDIFF_OK )
        {
            uint32  appdataSize;
            uchar   *buf;
            uchar   lenbuf[GDIFF_APPDATALEN];

            nRead = XP_FileRead( lenbuf, GDIFF_APPDATALEN, dd->fDiff );
            if ( nRead == GDIFF_APPDATALEN ) 
            {
                appdataSize = getlong(lenbuf);

                if ( appdataSize > 0 ) 
                {
                    buf = (uchar *)XP_ALLOC( appdataSize );

                    if ( buf != NULL )
                    {
                        nRead = XP_FileRead( buf, appdataSize, dd->fDiff );
                        if ( nRead == appdataSize ) 
                        {
                            if ( 0 == XP_MEMCMP( buf, APPFLAG_W32BOUND, appdataSize ) )
                                dd->bWin32BoundImage = TRUE;

                            if ( 0 == XP_MEMCMP( buf, APPFLAG_APPLESINGLE, appdataSize ) )
                                dd->bMacAppleSingle = TRUE;
                        }
                        else {
                            err = GDIFF_ERR_HEADER;
                        }

                        XP_FREE( buf );
                    }
                    else {
                        err = GDIFF_ERR_MEM;
                    }
                }
            }
            else {
                err = GDIFF_ERR_HEADER;
            }
        }
    }

    return (err);
}





/*---------------------------------------------------------
 *  gdiff_validateFile()
 *
 *  computes the checksum of the file and compares it to 
 *  the value stored in the GDIFF header
 *---------------------------------------------------------
 */
static 
int32 gdiff_validateFile( pDIFFDATA dd, int file )
{
    int32   result;
    XP_File fh;
    uchar * chksum;

    /* which file are we dealing with? */
    if ( file == SRCFILE ) {
        fh = dd->fSrc;
        chksum = dd->oldChecksum;
    }
    else { /* OUTFILE */
        fh = dd->fOut;
        chksum = dd->newChecksum;
    }

    /* make sure file's at beginning */
    XP_FileSeek( fh, 0, SEEK_SET );

    /* calculate appropriate checksum */
    switch (dd->checksumType)
    {
        case GDIFF_CS_NONE:
            result = GDIFF_OK;
            break;


        case GDIFF_CS_CRC32:
            if ( dd->checksumLength == CRC32_LEN )
                result = gdiff_valCRC32( dd, fh, getlong(chksum) );
            else
                result = GDIFF_ERR_HEADER;
            break;


        case GDIFF_CS_MD5:


        case GDIFF_CS_SHA:


        default:
            /* unsupported checksum type */
            result = GDIFF_ERR_CHKSUMTYPE;
            break;
    }

    /* reset file position to beginning and return status */
    XP_FileSeek( fh, 0, SEEK_SET );
    return (result);
}






/*---------------------------------------------------------
 *  gdiff_valCRC32()
 *
 *  computes the checksum of the file and compares it to 
 *  the passed in checksum.  Assumes file is positioned at
 *  beginning.
 *---------------------------------------------------------
 */
static 
int32 gdiff_valCRC32( pDIFFDATA dd, XP_File fh, uint32 chksum )
{
    uint32 crc;
    uint32 nRead;

    crc = crc32(0L, Z_NULL, 0);

    nRead = XP_FileRead( dd->databuf, dd->bufsize, fh );
    while ( nRead > 0 ) 
    {
        crc = crc32( crc, dd->databuf, nRead );
        nRead = XP_FileRead( dd->databuf, dd->bufsize, fh );
    }

    if ( crc == chksum )
        return GDIFF_OK;
    else
        return GDIFF_ERR_CHECKSUM;
}




#ifdef WIN32
/*---------------------------------------------------------
 *  su_unbind()
 *
 *  strips import binding information from Win32
 *  executables and .DLL's
 *---------------------------------------------------------
 */
static 
XP_Bool su_unbind( char *oldurl, XP_FileType oldtype, char *newurl, XP_FileType newtype)
{
    XP_Bool bSuccess = FALSE;
    char    *oldfile;
    char    *newfile;

    int     i;
    DWORD   nRead;
    PDWORD  pOrigThunk;
    PDWORD  pBoundThunk;
    FILE    *fh = NULL;
    char    *buf;
    BOOL    bModified = FALSE;
    BOOL    bImports = FALSE;

    IMAGE_DOS_HEADER            mz;
    IMAGE_NT_HEADERS            nt;
    IMAGE_SECTION_HEADER        sec;

    PIMAGE_DATA_DIRECTORY       pDir;
    PIMAGE_IMPORT_DESCRIPTOR    pImp;

    typedef BOOL (__stdcall *BINDIMAGEEX)(DWORD Flags,
									  LPSTR ImageName,
									  LPSTR DllPath,
									  LPSTR SymbolPath,
									  PVOID StatusRoutine);
    HINSTANCE   hImageHelp;
    BINDIMAGEEX pfnBindImageEx;


    oldfile = WH_FileName( oldurl, oldtype );
    newfile = WH_FileName( newurl, newtype );

    if ( oldfile != NULL && newfile != NULL &&
         CopyFile( oldfile, newfile, FALSE ) )
    {   
        /* call BindImage() first to make maximum room for a possible
         * NT-style Bound Import Descriptors which can change various
         * offsets in the file */
	    hImageHelp = LoadLibrary("IMAGEHLP.DLL");
	    if ( hImageHelp > (HINSTANCE)HINSTANCE_ERROR ) {
        	pfnBindImageEx = (BINDIMAGEEX)GetProcAddress(hImageHelp, "BindImageEx");
    	    if (pfnBindImageEx) {
                pfnBindImageEx(0, newfile, NULL, NULL, NULL);
            }
		    FreeLibrary(hImageHelp);
	    }
        
        
        fh = fopen( newfile, "r+b" );
        if ( fh == NULL )
            goto bail;


        /* read and validate the MZ header */
        nRead = fread( &mz, 1, sizeof(mz), fh );
        if ( nRead != sizeof(mz) || mz.e_magic != IMAGE_DOS_SIGNATURE )
            goto bail;


        /* read and validate the NT header */
        fseek( fh, mz.e_lfanew, SEEK_SET );
        nRead = fread( &nt, 1, sizeof(nt), fh );
        if ( nRead != sizeof(nt) || 
             nt.Signature != IMAGE_NT_SIGNATURE ||
             nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ) 
        {
            goto bail;
        }


        /* find .idata section */
        for (i = nt.FileHeader.NumberOfSections; i > 0; i--)
        {
            nRead = fread( &sec, 1, sizeof(sec), fh );
            if ( nRead != sizeof(sec) ) 
                goto bail;

            if ( memcmp( sec.Name, ".idata", 6 ) == 0 ) {
                bImports = TRUE;
                break;
            }
        }


        /* Zap any binding in the imports section */
        if ( bImports ) 
        {
            buf = (char*)malloc( sec.SizeOfRawData );
            if ( buf == NULL )
                goto bail;

            fseek( fh, sec.PointerToRawData, SEEK_SET );
            nRead = fread( buf, 1, sec.SizeOfRawData, fh );
            if ( nRead != sec.SizeOfRawData ) {
                free( buf );
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
            buf = (char*)calloc( pDir->Size, 1 );
            if ( buf == NULL )
                goto bail;
        
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
            fwrite( &nt, 1, sizeof(nt), fh );
        }

        bSuccess = TRUE;
    }

bail:
    if ( fh != NULL ) 
        fclose(fh);

    XP_FREEIF( oldfile );
    XP_FREEIF( newfile );
    return bSuccess;
}
#endif
