/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "zlib.h"
#include "nsFileSpec.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "nsInstall.h"
#include "nsInstallPatch.h"
#include "nsInstallResources.h"
#include "nsIDOMInstallVersion.h"

#include "gdiff.h"

#include "VerReg.h"
#include "ScheduledTasks.h"
#include "plstr.h"
#include "prlog.h"

#ifdef XP_MAC
#include "PatchableAppleSingle.h"
#include "nsILocalFileMac.h"
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
#ifdef WIN32
static PRBool  su_unbind(char* oldsrc, char* newsrc);
#endif

MOZ_DECL_CTOR_COUNTER(nsInstallPatch)

nsInstallPatch::nsInstallPatch( nsInstall* inInstall,
                                const nsString& inVRName,
                                const nsString& inVInfo,
                                const nsString& inJarLocation,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallPatch);

    char tempTargetFile[MAXREGPATHLEN];

    PRInt32 err = VR_GetPath( NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(inVRName).get()),
                              sizeof(tempTargetFile), tempTargetFile );
    
    if (err != REGERR_OK)
    {
        *error = nsInstall::NO_SUCH_COMPONENT;
        return;
    }
    nsString folderSpec; folderSpec.AssignWithConversion(tempTargetFile);
	
    nsCOMPtr<nsILocalFile> tmp;
    NS_NewLocalFile((char*)tempTargetFile, PR_TRUE, getter_AddRefs(tmp));

    mPatchFile      =   nsnull;
    mTargetFile     =   nsnull;
    mPatchedFile    =   nsnull;
    mRegistryName   =   new nsString(inVRName);
    mJarLocation    =   new nsString(inJarLocation);
    mVersionInfo    =   new nsInstallVersion();
    tmp->Clone(getter_AddRefs(mTargetFile));
    
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
                                nsInstallFolder* folderSpec,
                                const nsString& inPartialPath,
                                PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallPatch);

    if ((inInstall == nsnull) || (inVRName.IsEmpty()) || (inJarLocation.IsEmpty())) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    nsCOMPtr<nsIFile> tmp = folderSpec->GetFileSpec();
    if (!tmp)
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
    tmp->Clone(getter_AddRefs(mTargetFile));

    if (mRegistryName == nsnull ||
        mJarLocation  == nsnull ||
        mTargetFile   == nsnull ||
        mVersionInfo  == nsnull )
    {
        *error = nsInstall::OUT_OF_MEMORY;
        return;
    }
    
    mVersionInfo->Init(inVInfo);
    
    if(! inPartialPath.IsEmpty())
        mTargetFile->Append(NS_LossyConvertUCS2toASCII(inPartialPath).get());
}

nsInstallPatch::~nsInstallPatch()
{
    if (mVersionInfo)
        delete mVersionInfo;

    //if (mTargetFile)
    //    delete mTargetFile;

    if (mJarLocation)
        delete mJarLocation;
    
    if (mRegistryName)
        delete mRegistryName;

    //if (mPatchedFile)
    //    delete mPatchedFile;
    
    //if (mPatchFile)
    //    delete mPatchFile;

    MOZ_COUNT_DTOR(nsInstallPatch);
}


PRInt32 nsInstallPatch::Prepare()
{
    PRInt32 err;
    PRBool deleteOldSrc, flagExists, flagIsFile;
    
    if (mTargetFile == nsnull)
        return  nsInstall::INVALID_ARGUMENTS;

    mTargetFile->Exists(&flagExists);
    if (flagExists)
    {
        mTargetFile->IsFile(&flagIsFile);
        if (flagIsFile)
        {
            err = nsInstall::SUCCESS;
        }
        else
        {
            err = nsInstall::IS_DIRECTORY;
        }
    }
    else
    {
        err = nsInstall::DOES_NOT_EXIST;
    }

    if (err != nsInstall::SUCCESS)
    {   
        return err;
    }

    err =  mInstall->ExtractFileFromJar(*mJarLocation, mTargetFile, getter_AddRefs(mPatchFile));
   
    
    nsCOMPtr<nsIFile> fileName = nsnull;
    //nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) ) );//nsIFileXXX: nsFilePath?
    nsVoidKey ikey( HashFilePath( mTargetFile ));

    mInstall->GetPatch(&ikey, getter_AddRefs(fileName));

    if (fileName != nsnull) 
    {
        deleteOldSrc = PR_TRUE;
    } 
    else 
    {
        fileName     = mTargetFile;
        deleteOldSrc = PR_FALSE;
    }

    err = NativePatch(  fileName,           // the file to patch
                        mPatchFile,         // the patch that was extracted from the jarfile
                        getter_AddRefs(mPatchedFile));     // the new patched file
    
    // clean up extracted diff data file
    mPatchFile->Exists(&flagExists);
    if ( (mPatchFile != nsnull) && (flagExists) )
    {
        mPatchFile->Remove(PR_FALSE);
    }


    if (err != nsInstall::SUCCESS)
    {   
        // clean up tmp patched file since patching failed
      mPatchFile->Exists(&flagExists);
 		  if ((mPatchedFile != nsnull) && (flagExists))
      {
			    mPatchedFile->Remove(PR_FALSE);
      }
		return err;
    }

    PR_ASSERT(mPatchedFile != nsnull);
    mInstall->AddPatch(&ikey, mPatchedFile );

    if ( deleteOldSrc ) 
    {
    DeleteFileNowOrSchedule(fileName );
    }
  
    return err;
}

PRInt32 nsInstallPatch::Complete()
{  
    PRBool flagEquals;

    if ((mInstall == nsnull) || (mVersionInfo == nsnull) || (mPatchedFile == nsnull) || (mTargetFile == nsnull)) 
    {
        return nsInstall::INVALID_ARGUMENTS;
    }
    
    PRInt32 err = nsInstall::SUCCESS;

    nsCOMPtr<nsIFile> fileName = nsnull;
    //nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) )  );//nsIFileXXX: nsFilePath?
    nsVoidKey ikey( HashFilePath( mTargetFile ));
    
    mInstall->GetPatch(&ikey, getter_AddRefs(fileName));
    
    if (fileName == nsnull)
    {
        err = nsInstall::UNEXPECTED_ERROR;
    }
    else
    {
      fileName->Equals(mPatchedFile, &flagEquals);
      if (flagEquals) 
      {
        // the patch has not been superceded--do final replacement
        err = ReplaceFileNowOrSchedule( mPatchedFile, mTargetFile);
        if ( 0 == err || nsInstall::REBOOT_NEEDED == err ) 
        {
            nsString tempVersionString;
            mVersionInfo->ToString(tempVersionString);
            
            nsXPIDLCString tempPath;
            mTargetFile->GetPath(getter_Copies(tempPath));

            // DO NOT propogate version registry errors, it will abort 
            // FinalizeInstall() leaving things hosed. These piddly errors
            // aren't worth that.
            VR_Install( NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(*mRegistryName).get()),
                        NS_CONST_CAST(char *, tempPath.get()),
                        NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(tempVersionString).get()),
                        PR_FALSE );
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
    }
    return err;
}

void nsInstallPatch::Abort()
{
    PRBool flagEquals;
    nsCOMPtr<nsIFile> fileName = nsnull;
    //nsVoidKey ikey( HashFilePath( nsFilePath(*mTargetFile) ) ); //nsIFileXXX: nsFilePath?
    nsVoidKey ikey( HashFilePath( mTargetFile ));

    mInstall->GetPatch(&ikey, getter_AddRefs(fileName));

    fileName->Equals(mPatchedFile, &flagEquals);
    if (fileName != nsnull && (flagEquals) )
    {
        DeleteFileNowOrSchedule( mPatchedFile );
    }
}

char* nsInstallPatch::toString()
{
	  char* buffer = new char[1024];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return buffer;

    if (mTargetFile != nsnull)
    {
        rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("Patch"));

        if (rsrcVal)
        {
            char* temp;
            mTargetFile->GetPath(&temp);
            sprintf( buffer, rsrcVal, temp); 
            nsCRT::free(rsrcVal);
        }
    }

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
    return PR_TRUE;
}


PRInt32
nsInstallPatch::NativePatch(nsIFile *sourceFile, nsIFile *patchFile, nsIFile **newFile)  //nsIFileXXX: changed & to *
{

	PRBool flagExists;
  nsresult rv;
  DIFFDATA	  *dd;
	PRInt32		  status		   = GDIFF_ERR_MEM;
	char 		    *tmpurl		   = NULL;
	//nsFileSpec  *outFileSpec = new nsFileSpec; 
  //nsFileSpec  *tempSrcFile = new nsFileSpec;   // TODO: do you need to free?
  nsCOMPtr<nsIFile> outFileSpec;
  nsCOMPtr<nsIFile> tempSrcFile;
  nsCOMPtr<nsILocalFile> uniqueSrcFile;
  nsCOMPtr<nsILocalFile> patchFileLocal = do_QueryInterface(patchFile, &rv);
  
  char*        realfile;
  sourceFile->GetPath(&realfile);

  sourceFile->Clone(getter_AddRefs(outFileSpec));

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
    patchFileLocal->OpenNSPRFileDesc(PR_RDONLY, 0666, &dd->fDiff);

		if (dd->fDiff != NULL)
		{
			status = gdiff_parseHeader(dd);
		} 
    else 
    {
			status = GDIFF_ERR_ACCESS;
		}


    // in case we need to unbind Win32 images OR encode Mac file
    if (( dd->bWin32BoundImage || dd->bMacAppleSingle) && (status == GDIFF_OK ))
    {
        // make an unique tmp file  (FILENAME-src.EXT)
        char* leafName;
        rv = sourceFile->GetLeafName(&leafName);

        nsString tmpName; tmpName.AssignWithConversion("-src");
        nsString tmpFileName; tmpFileName.AssignWithConversion(leafName);

        PRInt32 i;
        if ((i = tmpFileName.RFindChar('.')) > 0)
        {
            nsString ext;
            nsString fName;
            tmpFileName.Right(ext, (tmpFileName.Length() - i) );        
            tmpFileName.Left(fName, (tmpFileName.Length() - (tmpFileName.Length() - i)));
            tmpFileName = fName + tmpName + ext;

        } else {
               tmpFileName += tmpName;
        }
        
    
        rv = sourceFile->Clone(getter_AddRefs(tempSrcFile));  //Clone the sourceFile
        tempSrcFile->SetLeafName(NS_LossyConvertUCS2toASCII(tmpFileName).get()); //Append the new leafname
        uniqueSrcFile = do_QueryInterface(tempSrcFile, &rv);  //Create an nsILocalFile version to pass to MakeUnique
        MakeUnique(uniqueSrcFile); 

       char*        realfile;
       sourceFile->GetPath(&realfile);

#ifdef WIN32
        // unbind Win32 images

        char* unboundFile;
        uniqueSrcFile->GetPath(&unboundFile);

        if (su_unbind(realfile, unboundFile))  //
        {
            PL_strfree(realfile);
            realfile = PL_strdup(unboundFile);
        }
        else
        {
            status = GDIFF_ERR_MEM;
        }
        PL_strfree(unboundFile);
#endif
#ifdef XP_MAC
   // Encode src file, and put into temp file
        FSSpec sourceSpec, tempSpec;
        nsCOMPtr<nsILocalFileMac> tempSourceFile;
        tempSourceFile = do_QueryInterface(sourceFile, &rv);
        tempSourceFile->GetFSSpec(&sourceSpec);
    
        status = PAS_EncodeFile(&sourceSpec, &tempSpec);   

        if (status == noErr)
        {
            // set
            PL_strfree(realfile);
            tempSrcFile->GetPath(&realfile);
        }
#endif
    }

    if (status != NS_OK)
    goto cleanup;

    // make a unique file at the same location of our source file  (FILENAME-ptch.EXT)
    nsString patchFileName; patchFileName.AssignWithConversion("-ptch");
    char* leafName;
    sourceFile->GetLeafName(&leafName);
    nsString newFileName; newFileName.AssignWithConversion(leafName);;

    PRInt32 index;
    if ((index = newFileName.RFindChar('.')) > 0)
    {
        nsString extention;
        nsString fileName;
        newFileName.Right(extention, (newFileName.Length() - index) );        
        newFileName.Left(fileName, (newFileName.Length() - (newFileName.Length() - index)));
        newFileName = fileName + patchFileName + extention;

    }
    else
    {
        newFileName += patchFileName;
    }


    outFileSpec->SetLeafName(NS_LossyConvertUCS2toASCII(newFileName).get());  //Set new leafname
    nsCOMPtr<nsILocalFile> outFileLocal = do_QueryInterface(outFileSpec, &rv); //Create an nsILocalFile version 
                                                                               //to send to MakeUnique()
    MakeUnique(outFileLocal);

    // apply patch to the source file
    //dd->fSrc = PR_Open ( realfile, PR_RDONLY, 0666);
    //dd->fOut = PR_Open ( outFile, PR_RDWR|PR_CREATE_FILE|PR_TRUNCATE, 0666);
    nsCOMPtr<nsILocalFile> realFileLocal = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);;
    realFileLocal->InitWithPath(realfile);

    realFileLocal->OpenNSPRFileDesc(PR_RDONLY, 0666, &dd->fSrc);
    outFileLocal->OpenNSPRFileDesc(PR_RDWR|PR_CREATE_FILE|PR_TRUNCATE, 0666, &dd->fOut);

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

        rv = outFileSpec->Clone(newFile);
    } 
    else 
    {
        status = GDIFF_ERR_ACCESS;
    }
  }



#ifdef XP_MAC
  if ( dd->bMacAppleSingle && status == GDIFF_OK ) 
 {
        // create another file, so that we can decode somewhere
        //nsFileSpec anotherName = *outFileSpec;
        nsCOMPtr<nsILocalFile> anotherName;
        nsCOMPtr<nsIFile> bsTemp;
        
        outFileSpec->Clone(getter_AddRefs(bsTemp));   //Clone because we'll be changing the name
        anotherName = do_QueryInterface(bsTemp, &rv); //Set the old name
        MakeUnique(anotherName);  //Now give it the new name
        
		// Close the out file so that we can read it 		
		PR_Close( dd->fOut );
		dd->fOut = NULL;
		
		
		FSSpec outSpec;
		FSSpec anotherSpec;
		nsCOMPtr<nsILocalFileMac> outSpecMacSpecific;
		nsCOMPtr<nsILocalFileMac> anotherNameMacSpecific;
		
		anotherNameMacSpecific = do_QueryInterface(anotherName, &rv); //set value to nsILocalFileMac (sheesh)
		outSpecMacSpecific = do_QueryInterface(outFileSpec, &rv); //ditto 
		
		anotherNameMacSpecific->GetFSSpec(&anotherSpec);
		outSpecMacSpecific->GetFSSpec(&outSpec);
		
		outFileSpec->Exists(&flagExists);
		if ( flagExists )
		{
		    PRInt64 fileSize;
		    outFileSpec->GetFileSize(&fileSize);
		}
		
			
        status =  PAS_DecodeFile(&outSpec, &anotherSpec);
		if (status != noErr)
		{
		   	goto cleanup;
        }
		
        nsCOMPtr<nsIFile> parent;
        
        outFileSpec->GetParent(getter_AddRefs(parent));
        
        outFileSpec->Remove(PR_FALSE);
        
        char* leaf;
        anotherName->GetLeafName(&leaf);
        anotherName->CopyTo(parent, leaf);
        
        anotherName->Clone(newFile);
        
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
            PR_Close( dd->fOut );

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

    if (realfile != NULL)
    {
        PL_strfree(realfile);
    }
    
    if (tempSrcFile)
    {
        tempSrcFile->Exists(&flagExists);
        if (flagExists)
            tempSrcFile->Remove(PR_FALSE);
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
nsInstallPatch::HashFilePath(nsIFile* aPath)
{
    PRUint32 rv = 0;

    char* cPath;
    aPath->GetPath(&cPath);
    
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
    PRBool done;
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


#ifdef WIN32
/*---------------------------------------------------------
 *  su_unbind()
 *
 *  strips import binding information from Win32
 *  executables and .DLL's
 *---------------------------------------------------------
 */
static 
PRBool su_unbind(char* oldfile, char* newfile)
{
    PRBool bSuccess = FALSE;

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

    return bSuccess;
}
#endif
