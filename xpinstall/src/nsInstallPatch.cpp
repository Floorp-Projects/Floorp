/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "nsFileSpec.h"
#include "prmem.h"
#include "nsInstall.h"
#include "nsInstallPatch.h"
#include "nsInstallResources.h"
#include "nsIDOMInstallVersion.h"
#include "zlib.h"

#include "gdiff.h"

#include "VerReg.h"
#include "ScheduledTasks.h"
#include "plstr.h"
#include "xp_file.h" /* for XP_PlatformFileToURL */


#ifdef XP_MAC
#include "PatchableAppleSingle.h"
#endif

#define BUFSIZE     32768
#define OPSIZE      1
#define MAXCMDSIZE  12
#define SRCFILE     0
#define OUTFILE     1

#define getshort(s) (uint16)( ((uchar)*(s) << 8) + ((uchar)*((s)+1)) )

#define getlong(s)  \
            (uint32)( ((uchar)*(s) << 24) + ((uchar)*((s)+1) << 16 ) + \
                      ((uchar)*((s)+2) << 8) + ((uchar)*((s)+3)) )



static int32   gdiff_parseHeader( pDIFFDATA dd );
static int32   gdiff_validateFile( pDIFFDATA dd, int file );
static int32   gdiff_valCRC32( pDIFFDATA dd, PRFileDesc* fh, uint32 chksum );
static int32   gdiff_ApplyPatch( pDIFFDATA dd );
static int32   gdiff_getdiff( pDIFFDATA dd, uchar *buffer, uint32 length );
static int32   gdiff_add( pDIFFDATA dd, uint32 count );
static int32   gdiff_copy( pDIFFDATA dd, uint32 position, uint32 count );
static int32   gdiff_validateFile( pDIFFDATA dd, int file );


nsInstallPatch::nsInstallPatch( nsInstall* inInstall,
                                const nsString& inVRName,
                                const nsString& inVInfo,
                                const nsString& inJarLocation,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    char tempTargetFile[MAXREGPATHLEN];
    char* tempVersionString = inVRName.ToNewCString();

    PRInt32 err = VR_GetPath(tempVersionString, MAXREGPATHLEN, tempTargetFile );
    
    delete [] tempVersionString;
    
    if (err != REGERR_OK)
    {
        *error = nsInstall::NO_SUCH_COMPONENT;
        return;
    }
	nsString folderSpec(tempTargetFile);

    mPatchFile      =   nsnull;
    mTargetFile     =   nsnull;
    mPatchedFile    =   nsnull;
    mRegistryName   =   new nsString(inVRName);
    mJarLocation    =   new nsString(inJarLocation);
    mTargetFile     =   new nsFileSpec(folderSpec);
    mVersionInfo    =   new nsInstallVersion();
    
    if (mRegistryName == nsnull ||
        mJarLocation  == nsnull ||
        mTargetFile   == nsnull ||
        mVersionInfo  == nsnull )
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }

    mVersionInfo->Init(inVInfo);
}


nsInstallPatch::nsInstallPatch( nsInstall* inInstall,
                                const nsString& inVRName,
                                const nsString& inVInfo,
                                const nsString& inJarLocation,
                                const nsString& folderSpec,
                                const nsString& inPartialPath,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    if ((inInstall == nsnull) || (inVRName == "null") || (inJarLocation == "null")) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mPatchFile      =   nsnull;
    mTargetFile     =   nsnull;
    mPatchedFile    =   nsnull;
    mRegistryName   =   new nsString(inVRName);
    mJarLocation    =   new nsString(inJarLocation);
    mVersionInfo    =   new nsInstallVersion();
    mTargetFile     =   new nsFileSpec(folderSpec);

    if (mRegistryName == nsnull ||
        mJarLocation  == nsnull ||
        mTargetFile   == nsnull ||
        mVersionInfo  == nsnull )
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }
    
    mVersionInfo->Init(inVInfo);
    
    
    if(inPartialPath != "null")
        *mTargetFile += inPartialPath;
}

nsInstallPatch::~nsInstallPatch()
{
    if (mVersionInfo)
        delete mVersionInfo;

    if (mTargetFile)
        delete mTargetFile;

    if (mJarLocation)
        delete mJarLocation;
    
    if (mRegistryName)
        delete mRegistryName;

    if (mPatchedFile)
        delete mPatchedFile;
    
    if (mPatchFile)
        delete mPatchFile;

}


PRInt32 nsInstallPatch::Prepare()
{
    PRInt32 err;
    PRBool deleteOldSrc;
    
    if (mTargetFile == nsnull)
        return  nsInstall::INVALID_ARGUMENTS;

    if (mTargetFile->Exists())
    {
        if (mTargetFile->IsFile())
        {
            err = nsInstall::SUCCESS;
        }
        else
        {
            err = nsInstall::FILE_IS_DIRECTORY;
        }
    }
    else
    {
        err = nsInstall::FILE_DOES_NOT_EXIST;
    }

    if (err != nsInstall::SUCCESS)
    {   
        return err;
    }

    err =  mInstall->ExtractFileFromJar(*mJarLocation, mTargetFile, &mPatchFile);
   
    
    nsFileSpec *fileName = nsnull;
    nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) ) );
    
    mInstall->GetPatch(&ikey, &fileName);

    if (fileName != nsnull) 
    {
        deleteOldSrc = PR_TRUE;
    } 
    else 
    {
        fileName     = mTargetFile;
        deleteOldSrc = PR_FALSE;
    }

    err = NativePatch(  *fileName,           // the file to patch
                        *mPatchFile,         // the patch that was extracted from the jarfile
                        &mPatchedFile);     // the new patched file
    
    // clean up extracted diff data file
    if ( (mPatchFile != nsnull) && (mPatchFile->Exists()) )
    {
        mPatchFile->Delete(PR_FALSE);
    }


    if (err != nsInstall::SUCCESS)
    {   
        return err;
    }

    PR_ASSERT(mPatchedFile != nsnull);
	mInstall->AddPatch(&ikey, mPatchedFile );
	
    if ( deleteOldSrc ) 
    {
		DeleteFileNowOrSchedule(*fileName );
    }
  
    return err;
}

PRInt32 nsInstallPatch::Complete()
{  
    if ((mInstall == nsnull) || (mVersionInfo == nsnull) || (mPatchedFile == nsnull) || (mTargetFile == nsnull)) 
    {
        return nsInstall::INVALID_ARGUMENTS;
    }
    
    PRInt32 err = nsInstall::SUCCESS;

    nsFileSpec *fileName = nsnull;
    nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) )  );
    
    mInstall->GetPatch(&ikey, &fileName);
    
    if (fileName != nsnull && (*fileName == *mPatchedFile) )
    {
        // the patch has not been superceded--do final replacement
        err = ReplaceFileNowOrSchedule( *mPatchedFile, *mTargetFile);
        if ( 0 == err || nsInstall::REBOOT_NEEDED == err ) 
        {
            nsString tempVersionString;
            mVersionInfo->ToString(tempVersionString);
            
            char* tempRegName = mRegistryName->ToNewCString();
            char* tempVersion = tempVersionString.ToNewCString();

            err = VR_Install( tempRegName, 
							  (char*) (const char *) nsNSPRPath(*mTargetFile),
                              tempVersion, 
                              PR_FALSE );
            
            if (tempRegName) delete [] tempRegName;
            if (tempVersion) delete [] tempVersion;

        }
        else
        {
            err = nsInstall::UNEXPECTED_ERROR;
        }
    }
    else
    {
        // nothing -- old intermediate patched file was
        // deleted by a superceding patch
    }

    return err;
}

void nsInstallPatch::Abort()
{
    nsFileSpec *fileName = nsnull;
    nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) ) );

    mInstall->GetPatch(&ikey, &fileName);

    if (fileName != nsnull && (*fileName == *mPatchedFile) )
    {
        DeleteFileNowOrSchedule( *mPatchedFile );
    }
}

PRUnichar* nsInstallPatch::toString()
{
	PRUnichar* buffer = new PRUnichar[1024];
    
    if (buffer == nsnull)
        return buffer;

    if (mTargetFile != nsnull) 
        sprintf( (char *)buffer, nsInstallResources::GetPatchFileString(), mTargetFile->GetCString()); 

	return buffer;
}


PRBool
nsInstallPatch::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallPatch::RegisterPackageNode()
{
    return PR_FALSE;
}


PRInt32
nsInstallPatch::NativePatch(const nsFileSpec &sourceFile, const nsFileSpec &patchFile, nsFileSpec **newFile)
{

	DIFFDATA	*dd;
	PRInt32		status		= GDIFF_ERR_MEM;
	char 		*tmpurl		= NULL;
	char 		*realfile	= PL_strdup(nsNSPRPath(sourceFile)); // needs to be sourceFile!!!
	nsFileSpec  *outFileSpec = new nsFileSpec;
    
    if (!outFileSpec) {
        status = GDIFF_ERR_MEM;
        goto cleanup;
    }
    
    *outFileSpec = sourceFile;

	dd = (DIFFDATA *)PR_Calloc( 1, sizeof(DIFFDATA));
	if (dd != NULL)
	{
		dd->databuf = (uchar*)PR_Malloc(BUFSIZE);
		if (dd->databuf == NULL) 
		{
			status = GDIFF_ERR_MEM;
			goto cleanup;
		}


		dd->bufsize = BUFSIZE;

		// validate patch header & check for special instructions
		dd->fDiff = PR_Open (nsNSPRPath(patchFile), PR_RDONLY, 0666);


		if (dd->fDiff != NULL)
		{
			status = gdiff_parseHeader(dd);
		} else {
			status = GDIFF_ERR_ACCESS;
		}

#ifdef dono
#ifdef WIN32

        /* unbind Win32 images */
        if ( dd->bWin32BoundImage && status == GDIFF_OK ) {
            tmpurl = WH_TempName( xpURL, NULL );
            if ( tmpurl != NULL ) {
                if (su_unbind( srcfile, srctype, tmpurl, xpURL ))  	
				{
                    PL_strfree(realfile);
					realfile = tmpurl;
                    realtype = xpURL;
                    
                }
            }
            else
                status = GDIFF_ERR_MEM;
        }
#endif
#endif

#ifdef XP_MAC

		if ( dd->bMacAppleSingle && status == GDIFF_OK ) 
		{
           // create a tmp file, so that we can AppleSingle the src file
           nsSpecialSystemDirectory tempMacFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
           nsString srcName = sourceFile.GetLeafName();
           tempMacFile.SetLeafName(srcName);
           tempMacFile.MakeUnique();
           
           // Encode! 
		   // Encode src file, and put into temp file
		   FSSpec sourceSpec = sourceFile.GetFSSpec();
		   FSSpec tempSpec   = tempMacFile.GetFSSpec();
		    
			status = PAS_EncodeFile(&sourceSpec, &tempSpec);   
				
			if (status == noErr)
			{
				// set
                PL_strfree(realfile);
				realfile = PL_strdup(nsNSPRPath(tempMacFile));
			}
		}
#endif 



		if (status != NS_OK)
			goto cleanup;

		// make a unique file at the same location of our source file  (FILENAME-ptch.EXT)
        nsString patchFileName = "-ptch";
		nsString newFileName = sourceFile.GetLeafName();

        PRInt32 index;
		if ((index = newFileName.RFindChar('.')) > 0)
		{
            nsString extention;
            nsString fileName;
            newFileName.Right(extention, (newFileName.Length() - index) );        
            newFileName.Left(fileName, (newFileName.Length() - (newFileName.Length() - index)));
            newFileName = fileName + patchFileName + extention;

        } else {
            newFileName += patchFileName;
        }
        

		outFileSpec->SetLeafName(newFileName);
		outFileSpec->MakeUnique();

        char *outFile = PL_strdup(nsNSPRPath(*outFileSpec));

		// apply patch to the source file
		dd->fSrc = PR_Open ( realfile, PR_RDONLY, 0666);
		dd->fOut = PR_Open ( outFile, PR_RDWR|PR_CREATE_FILE|PR_TRUNCATE, 0666);

		if (dd->fSrc != NULL && dd->fOut != NULL)
		{
			status = gdiff_validateFile (dd, SRCFILE);

			// specify why diff failed
			if (status == GDIFF_ERR_CHECKSUM)
				status = GDIFF_ERR_CHECKSUM_TARGET;

			if (status == GDIFF_OK)
				status = gdiff_ApplyPatch(dd);

			if (status == GDIFF_OK)
				status = gdiff_validateFile (dd, OUTFILE);

			if (status == GDIFF_ERR_CHECKSUM)
				status = GDIFF_ERR_CHECKSUM_RESULT;

            if (status == GDIFF_OK)
            {
                *newFile = outFileSpec;
                if ( outFile != nsnull)
                    PL_strfree( outFile );
            }

		} else {

			status = GDIFF_ERR_ACCESS;
		}		
	}



#ifdef XP_MAC
	if ( dd->bMacAppleSingle && status == GDIFF_OK ) 
	{
        // create another file, so that we can decode somewhere
        nsFileSpec *anotherName = outFileSpec;
        anotherName->MakeUnique();
        
		// Close the out file so that we can read it 		
		PR_Close( dd->fOut );
		dd->fOut = NULL;
		
		
		FSSpec outSpec = outFileSpec->GetFSSpec();
		FSSpec anotherSpec = anotherName->GetFSSpec();
		
			
        status =  PAS_DecodeFile(&outSpec, &anotherSpec);
		if (status != noErr)
		{
		   	goto cleanup;
        }
		
        nsFileSpec parent;
        
        outFileSpec->GetParent(parent);
        
        outFileSpec->Delete(PR_FALSE);
        anotherName->Copy(parent);
        
        *newFile = anotherName;
	}
	
#endif 


cleanup:
    if ( dd != NULL ) 
    {
        if ( dd->fSrc != nsnull )
            PR_Close( dd->fSrc );


        if ( dd->fDiff != nsnull )
            PR_Close( dd->fDiff );
		
		if ( dd->fOut != nsnull )
        {
            PR_Close( dd->fOut );
        }
        
        if ( status != GDIFF_OK )
		   	//XP_FileRemove( outfile, outtype );
			newFile = NULL;
		        
		PR_FREEIF( dd->databuf );
        PR_FREEIF( dd->oldChecksum );
        PR_FREEIF( dd->newChecksum );
        PR_DELETE(dd);
    }

	if ( tmpurl != NULL ) {
        //XP_FileRemove( tmpurl, xpURL );
		tmpurl = NULL;
        PR_DELETE( tmpurl );
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
				status = nsInstall::PATCH_BAD_DIFF;
				break;
		case GDIFF_ERR_CHECKSUM_TARGET:
				status = nsInstall::PATCH_BAD_CHECKSUM_TARGET;
				break;
		case GDIFF_ERR_CHECKSUM_RESULT:
				status = nsInstall::PATCH_BAD_CHECKSUM_RESULT;
				break;
		case GDIFF_ERR_OLDFILE:
		case GDIFF_ERR_ACCESS:
		case GDIFF_ERR_MEM:
		case GDIFF_ERR_UNKNOWN:
		default:
				status = nsInstall::UNEXPECTED_ERROR;
				break;
	}

    return status;

	// return -1;	//old return value
}


void* 
nsInstallPatch::HashFilePath(const nsFilePath& aPath)
{
    PRUint32 rv = 0;

	char* cPath = PL_strdup(nsNSPRPath(aPath));
    
    if(cPath != nsnull) 
    {
        char  ch;
        char* pathIndex = cPath;

        while ((ch = *pathIndex++) != 0) 
        {
            // FYI: rv = rv*37 + ch
            rv = ((rv << 5) + (rv << 2) + rv) + ch;
        }
    }

	PL_strfree(cPath);

    return (void*)rv;
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

	nRead = PR_Read (dd->fDiff, header, GDIFF_HEADERSIZE);
    if ( nRead != GDIFF_HEADERSIZE ||
         memcmp( header, GDIFF_MAGIC, GDIFF_MAGIC_LEN ) != 0  ||
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
            PR_ASSERT( newcslen == oldcslen );

            dd->checksumLength = oldcslen;
            dd->oldChecksum = (uchar*)PR_MALLOC(oldcslen);
            dd->newChecksum = (uchar*)PR_MALLOC(newcslen);

            if ( dd->oldChecksum != NULL && dd->newChecksum != NULL )
            {
				nRead = PR_Read (dd->fDiff, dd->oldChecksum, oldcslen);
                if ( nRead == oldcslen )
                {
					nRead = PR_Read (dd->fDiff, dd->newChecksum, newcslen);
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

			nRead = PR_Read(dd->fDiff, lenbuf, GDIFF_APPDATALEN);
            if ( nRead == GDIFF_APPDATALEN ) 
            {
                appdataSize = getlong(lenbuf);

                if ( appdataSize > 0 ) 
                {
                    buf = (uchar *)PR_MALLOC( appdataSize );

                    if ( buf != NULL )
                    {
						nRead = PR_Read (dd->fDiff, buf, appdataSize);
                        if ( nRead == appdataSize ) 
                        {
                            if ( 0 == memcmp( buf, APPFLAG_W32BOUND, appdataSize ) )
                                dd->bWin32BoundImage = TRUE;

                            if ( 0 == memcmp( buf, APPFLAG_APPLESINGLE, appdataSize ) )
                                dd->bMacAppleSingle = TRUE;
                        }
                        else {
                            err = GDIFF_ERR_HEADER;
                        }

                        PR_DELETE( buf );
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
    int32		result;
    PRFileDesc*	fh;
    uchar*		chksum;

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
    PR_Seek( fh, 0, PR_SEEK_SET );

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
    PR_Seek( fh, 0, PR_SEEK_SET );
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
int32 gdiff_valCRC32( pDIFFDATA dd, PRFileDesc* fh, uint32 chksum )
{
    uint32 crc;
    uint32 nRead;

    crc = crc32(0L, Z_NULL, 0);

	nRead = PR_Read (fh, dd->databuf, dd->bufsize);
    while ( nRead > 0 ) 
    {
        crc = crc32( crc, dd->databuf, nRead );
		nRead = PR_Read (fh, dd->databuf, dd->bufsize);
    }

    if ( crc == chksum )
        return GDIFF_OK;
    else
        return GDIFF_ERR_CHECKSUM;
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

	bytesRead = PR_Read (dd->fDiff, buffer, length);
    if ( bytesRead != length )
        return GDIFF_ERR_BADDIFF;

    return GDIFF_OK;
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
		nRead = PR_Read (dd->fDiff, dd->databuf, chunksize);
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_BADDIFF;
            break;
        }

		PR_Write (dd->fOut, dd->databuf, chunksize);

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

	PR_Seek (dd->fSrc, position, PR_SEEK_SET);

    while ( count > 0 ) {
        chunksize = (count > dd->bufsize) ? dd->bufsize : count;

		nRead = PR_Read (dd->fSrc, dd->databuf, chunksize);
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_OLDFILE;
            break;
        }

		PR_Write (dd->fOut, dd->databuf, chunksize);

        count -= chunksize;
    }

    return (err);
}

