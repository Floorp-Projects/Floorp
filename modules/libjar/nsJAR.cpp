/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 *     Mitch Stoltz <mstoltz@netsape.com>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */
#include <string.h>
#include "nsIPrincipal.h"
#include "nsILocalFile.h"
#include "nsJARInputStream.h"
#include "nsJAR.h"
#include "nsXPIDLString.h"


#ifndef __gen_nsIFile_h__
#define NS_ERROR_FILE_UNRECONGNIZED_PATH        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 1)
#define NS_ERROR_FILE_UNRESOLVABLE_SYMLINK      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 2)
#define NS_ERROR_FILE_EXECUTION_FAILED          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 3)
#define NS_ERROR_FILE_UNKNOWN_TYPE              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 4)
#define NS_ERROR_FILE_DESTINATION_NOT_DIR       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 5)
#define NS_ERROR_FILE_TARGET_DOES_NOT_EXIST     NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 6)
#define NS_ERROR_FILE_COPY_OR_MOVE_FAILED       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 7)
#define NS_ERROR_FILE_ALREADY_EXISTS            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 8)
#define NS_ERROR_FILE_INVALID_PATH              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 9)
#define NS_ERROR_FILE_DISK_FULL                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 10)
#define NS_ERROR_FILE_CORRUPTED                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 11)
#endif

static nsresult
ziperr2nsresult(PRInt32 ziperr)
{
  switch (ziperr) {
    case ZIP_OK:                return NS_OK;
    case ZIP_ERR_MEMORY:        return NS_ERROR_OUT_OF_MEMORY;
    case ZIP_ERR_DISK:          return NS_ERROR_FILE_DISK_FULL;
    case ZIP_ERR_CORRUPT:       return NS_ERROR_FILE_CORRUPTED;
    case ZIP_ERR_PARAM:         return NS_ERROR_ILLEGAL_VALUE;
    case ZIP_ERR_FNF:           return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
    case ZIP_ERR_UNSUPPORTED:   return NS_ERROR_NOT_IMPLEMENTED;
    default:                    return NS_ERROR_FAILURE;
  }
}

//-- PR_Free doesn't null the pointer. 
//   This macro takes care of that.
#define JAR_NULLFREE(_ptr) \
  {                        \
    PR_FREEIF(_ptr);       \
    *(_ptr) = nsnull;      \
  }

//-- Used by the JAR hashtables to delete their entries.
PR_STATIC_CALLBACK(PRBool)
DeleteManifestEntry(nsHashKey* aKey, void* aData, void* closure)
{
  PR_FREEIF(aData);
  return PR_TRUE;
}

// The following initialization makes a guess of 25 entries per jarfile.
nsJAR::nsJAR(): mManifestData(nsnull, nsnull, DeleteManifestEntry, nsnull, 25),
                manifestParsed(PR_FALSE),
                mVerificationService(nsnull)
{
  NS_INIT_REFCNT();
}

nsJAR::~nsJAR()
{ 
  NS_IF_RELEASE(mVerificationService);
}

NS_IMPL_ISUPPORTS1(nsJAR, nsIZipReader);

NS_IMETHODIMP
nsJAR::Init(nsIFile* zipFile)
{
  mZipFile = zipFile;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Open()
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(mZipFile, &rv);
  if (NS_FAILED(rv)) return rv;

  PRFileDesc* fd;
  rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0664, &fd);
  if (NS_FAILED(rv)) return rv;

  PRInt32 err = mZip.OpenArchiveWithFileDesc(fd);
  
  return ziperr2nsresult(err);
}

NS_IMETHODIMP
nsJAR::Close()
{
  PRInt32 err = mZip.CloseArchive();
  return ziperr2nsresult(err);
}

NS_IMETHODIMP
nsJAR::Extract(const char *zipEntry, nsIFile* outFile)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(outFile, &rv);
  if (NS_FAILED(rv)) return rv;

  PRFileDesc* fd;
  rv = localFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 0664, &fd);
  if (NS_FAILED(rv)) return rv;

  PRUint16 mode;
  PRInt32 err = mZip.ExtractFileToFileDesc(zipEntry, fd, &mode);
  PR_Close(fd);
  return ziperr2nsresult(err);
}

NS_IMETHODIMP    
nsJAR::GetEntry(const char *zipEntry, nsIZipEntry* *result)
{
  nsZipItem* zipItem;
  PRInt32 err = mZip.GetItem(zipEntry, &zipItem);
  if (err != ZIP_OK) return ziperr2nsresult(err);

  nsJARItem* jarItem = new nsJARItem();
  if (jarItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(jarItem);
  jarItem->Init(zipItem);
  *result = jarItem;
  return NS_OK;
}

NS_IMETHODIMP    
nsJAR::FindEntries(const char *aPattern, nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_POINTER;
    
  nsZipFind *find = mZip.FindInit(aPattern);
  if (!find)
    return NS_ERROR_OUT_OF_MEMORY;

  nsISimpleEnumerator *zipEnum = new nsJAREnumerator(find);
  if (!zipEnum)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF( zipEnum );

  *_retval = zipEnum;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::GetInputStream(const char *aFilename, nsIInputStream **_retval)
{
  if (!_retval)
    return NS_OK;
  return CreateInputStream(aFilename, this, _retval);
}

#define JAR_MF 1
#define JAR_SF 2
#define JAR_MF_SEARCH_STRING "(M|/M)ETA-INF/(M|m)(ANIFEST|anifest).(MF|mf)$"
#define JAR_SF_SEARCH_STRING "(M|/M)ETA-INF/*.(SF|sf)$"
#define JAR_MF_HEADER (const char*)"Manifest-Version: 1.0"
#define JAR_SF_HEADER (const char*)"Signature-Version: 1.0"

NS_IMETHODIMP
nsJAR::ParseManifest()
{
  if (manifestParsed || !SupportsRSAVerification())
    return NS_OK;
  nsresult rv;
  char* manifestBuffer = nsnull;
  char* rsaBuffer = nsnull;
  PRUint32 rsaLen;
  nsCOMPtr<nsISimpleEnumerator> files;
  nsCOMPtr<nsJARItem> file;
  nsXPIDLCString manifestFilename;

  //-- (1)Manifest (MF) file
  rv = FindEntries(JAR_MF_SEARCH_STRING, getter_AddRefs(files));
  if (NS_FAILED(rv) || !files) goto cleanup;

  //-- Load the file into memory
  rv = files->GetNext(getter_AddRefs(file));
  if (NS_FAILED(rv)) goto cleanup;
  if (!file) { rv = NS_ERROR_FILE_CORRUPTED; goto cleanup; } // No MF file
  PRBool more;
  rv = files->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  if (more) { rv = NS_ERROR_FILE_CORRUPTED; goto cleanup; }  // More than one MF file
  rv = file->GetName(getter_Copies(manifestFilename));
  if (!manifestFilename || NS_FAILED(rv)) return rv;
  rv = LoadEntry(manifestFilename, (const char**)&manifestBuffer);
  if (NS_FAILED(rv)) goto cleanup;

  //-- Parse it
  rv = ParseOneFile(manifestBuffer, JAR_MF);
  if (NS_FAILED(rv)) goto cleanup;
  JAR_NULLFREE(manifestBuffer)

  //-- (2)Signature (SF) file
  // For now, we only read one SF file, even if the jar has multiple signatures.
  rv = FindEntries(JAR_SF_SEARCH_STRING, getter_AddRefs(files));
  if (NS_FAILED(rv) || !files) goto cleanup;

  {
    //-- Get an SF file
    rv = files->GetNext(getter_AddRefs(file));
    if (NS_FAILED(rv) || !file) goto cleanup;
    rv = file->GetName(getter_Copies(manifestFilename));
    if (NS_FAILED(rv)) goto cleanup;

    rv = LoadEntry(manifestFilename, (const char**)&manifestBuffer);
    if (NS_FAILED(rv)) goto cleanup;

    //-- Get its corresponding RSA file
    nsCAutoString rsaFilename(manifestFilename);
    PRInt32 extension = rsaFilename.RFindChar('.') + 1;
    NS_ASSERTION(extension != 0, "Manifest Parser: Missing file extension.");
    (void)rsaFilename.Cut(extension, 2);
    (void)rsaFilename.Append("rsa");
    rv = LoadEntry(rsaFilename, (const char**)&rsaBuffer, &rsaLen);
    if (NS_FAILED(rv)) // Try uppercase
    { 
      JAR_NULLFREE(rsaBuffer)
      (void)rsaFilename.Cut(extension, 3);
      (void)rsaFilename.Append("RSA");
      rv = LoadEntry(rsaFilename, (const char**)&rsaBuffer, &rsaLen);
    }
    if (NS_FAILED(rv)) goto cleanup;

    //-- Verify that the RSA file is a valid signature of the SF file
    nsCOMPtr<nsIPrincipal> principal;
    rv = VerifySignature(manifestBuffer, rsaBuffer, rsaLen, getter_AddRefs(principal));
    if (NS_FAILED(rv)) goto cleanup;
    JAR_NULLFREE(rsaBuffer);

    //-- Parse the SF file. If the verification above failed, principal
    // is null, and ParseOneFile will mark the relevant entries as invalid.
    // if ParseOneFile fails, then it has no effect, and we can safely 
    // continue to the next SF file, or return. 
    rv = ParseOneFile(manifestBuffer, JAR_SF, principal);
    JAR_NULLFREE(manifestBuffer)
    // End of signature file parsing
  }

 cleanup:
  DumpMetadata();
  JAR_NULLFREE(manifestBuffer)
  JAR_NULLFREE(rsaBuffer)
  manifestParsed = NS_SUCCEEDED(rv);
  return rv;
}

NS_IMETHODIMP
nsJAR::GetPrincipal(const char* aEntryName, nsIPrincipal** _retval)
{
  // Check that verification is supported and manifest has been parsed
  if (!SupportsRSAVerification() || !manifestParsed)
    return NS_ERROR_FAILURE;
  // Parameter check
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  if(!aEntryName) // Caller wants global file principal
    aEntryName = JAR_GLOBALMETA;

  nsStringKey key(aEntryName);
  nsJARManifestItem* manItem = (nsJARManifestItem*)mManifestData.Get(&key);
  if (!manItem) return NS_ERROR_FAILURE;

  //-- Verify the file digest if we haven't done so already.
  if (manItem->mType != JAR_GLOBAL && (!manItem->verifyComplete))
  { 
    if (manItem->hasPrincipal() && manItem->valid)
    {
      //-- Check if entry digests have been calculated (by nsJARInputStream)
      if (!manItem->entryDigestsCalculated)
        return NS_ERROR_FAILURE;
      
      PRBool foundDigest = PR_FALSE;
      for (PRInt32 a=0; a<JAR_DIGEST_COUNT; a++)
      {
        manItem->valid = manItem->valid && 
          ( (!manItem->storedEntryDigests[a]) ||
            PL_strcmp(manItem->storedEntryDigests[a],
                      manItem->calculatedEntryDigests[a]) == 0 );
        foundDigest = foundDigest || (manItem->storedEntryDigests[a]);
      }
      if (!foundDigest)
        // No digests in manifest file. Entry is valid but has no owner 
        manItem->valid = PR_TRUE;
      if (!manItem->valid)
        manItem->setPrincipal(nsnull);
    }
    // Free up stored digests
    for(PRInt32 b = 0; b<JAR_DIGEST_COUNT; b++)
    {
      JAR_NULLFREE(manItem->calculatedEntryDigests[b])
      JAR_NULLFREE(manItem->storedEntryDigests[b])
    }
      manItem->verifyComplete = PR_TRUE;
  }
  
  if (manItem && manItem->valid)
  {
    manItem->getPrincipal(_retval);
    return NS_OK;
  }
  else if (!manItem) 
  {
    *_retval = nsnull;
    return NS_OK;
  }
  else // Not valid
    return NS_ERROR_FAILURE; // XXX: Should this be a security error?
}

NS_IMETHODIMP
nsJAR::VerifyExternalFile(const char* aFilename, const char* aBuf,
                          PRUint32 aLen)
{
  //-- Check if verification is supported
  if (!SupportsRSAVerification())
    return NS_OK;
  //-- Parameter check
  if (!aFilename || !aBuf)
    return NS_ERROR_ILLEGAL_VALUE;

  nsStringKey key(aFilename);
  nsJARManifestItem* manItem = (nsJARManifestItem*)mManifestData.Get(&key);
  if (!manItem)
    return NS_ERROR_FAILURE;
  if (!manItem->entryDigestsCalculated)
  {
    nsresult rv;
    for (PRInt32 b=0; b<JAR_DIGEST_COUNT && NS_SUCCEEDED(rv); b++)
    {
      PRUint32 id;
      rv = DigestBegin(&id, b);
      if (NS_SUCCEEDED(rv))
        rv = DigestData(id, aBuf, aLen);
      if (NS_SUCCEEDED(rv))
        rv = CalculateDigest(id, &(manItem->calculatedEntryDigests[b]));
    }
	if (NS_SUCCEEDED(rv))
	  manItem->entryDigestsCalculated = PR_TRUE;
  }
  if (manItem->entryDigestsCalculated)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

//----------------------------------------------
// nsJAR private implementation
//----------------------------------------------

nsresult
nsJAR::CreateInputStream(const char* aFilename, nsJAR* aJAR, nsIInputStream **is)
{
  nsresult rv;
  nsJARInputStream* jis = nsnull;
  rv = nsJARInputStream::Create(nsnull, NS_GET_IID(nsIInputStream), (void**)&jis);
  if (!jis) return NS_ERROR_FAILURE;

  rv = jis->Init(aJAR, &mZip, aFilename);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  *is = (nsIInputStream*)jis;
  NS_ADDREF(*is);
  return NS_OK;
}

nsresult nsJAR::LoadEntry(const char* aFilename, const char** aBuf, 
                          PRUint32* aBufLen)
{
  //-- Get a stream for reading the manifest file
  nsresult rv;
  nsCOMPtr<nsIInputStream> manifestStream;
  rv = CreateInputStream(aFilename, nsnull, getter_AddRefs(manifestStream));
  if (NS_FAILED(rv)) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
  
  //-- Read the manifest file into memory
  char* buf;
  PRUint32 len;
  rv = manifestStream->Available(&len);
  if (NS_FAILED(rv)) return rv;
  buf = (char*)PR_MALLOC(len);
  if (!buf) return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 bytesRead;
  rv = manifestStream->Read(buf, len, &bytesRead);
  if (bytesRead != len) 
    rv = NS_ERROR_FILE_CORRUPTED;
  if (NS_FAILED(rv)) return rv;
  (void)manifestStream->Close();
  *aBuf = buf;
  if (aBufLen)
    *aBufLen = len;
  return NS_OK;
}


PRInt32 nsJAR::ReadLine(const char** src)
{
  //--Moves pointer to beginning of next line and returns line length
  //  not including CR/LF.
  PRInt32 length;
  char* eol = PL_strpbrk(*src, "\r\n");

  if (eol == nsnull) // Probably reached end of file before newline
  {
    length = PL_strlen(*src);
    if (length == 0) // immediate end-of-file
      *src = nsnull;
    else             // some data left on this line
      *src += length;
  }
  else
  {
    length = eol - *src;
    if (eol[0] == '\r' && eol[1] == '\n')      // CR LF, so skip 2
      *src = eol+2;
    else                                       // Either CR or LF, so skip 1
      *src = eol+1;
  }
  return length;
}

nsresult nsJAR::ParseOneFile(const char* filebuf, PRInt16 aFileType,
                             nsIPrincipal* aPrincipal,
                             PRBool aLastSFFile) // Allows us to free up resources
{
  //-- Set up parsing variables
  nsJARManifestItem* curItem;
  if (aFileType == JAR_MF)
  {
    curItem = new nsJARManifestItem();
    curItem->mType = JAR_GLOBAL; // First section is the global section
  }
  PRBool foundName = PR_TRUE;
  nsCAutoString curItemName(JAR_GLOBALMETA);
  const char* sectionStart  = filebuf;
  const char* curPos        = filebuf;
  const char* nextLineStart = filebuf;
  nsCAutoString curLine;
  PRInt32 linelen;
  nsCAutoString storedSectionDigests[JAR_DIGEST_COUNT];

  //-- Check file header
  linelen = ReadLine(&nextLineStart);
  curLine.Assign(curPos, linelen);
  if ( ((aFileType == JAR_MF) && (curLine != JAR_MF_HEADER) ) ||
       ((aFileType == JAR_SF) && (curLine != JAR_SF_HEADER) ) )
     return NS_ERROR_FILE_CORRUPTED;

  for(;;)
  {
    curPos = nextLineStart;
    linelen = ReadLine(&nextLineStart);
    curLine.Assign(curPos, linelen);
    if (linelen == 0) 
    // end of section (blank line or end-of-file)
    {
      if (aFileType == JAR_MF)
      {
        if (curItem->mType != JAR_INVALID)
        { 
          //-- Did this section have a name: line (or the global?)
          if(!foundName)
            curItem->mType = JAR_INVALID;
          else 
          {
            if (curItem->mType == JAR_INTERNAL)
            //-- If it's an internal item, it must correspond 
            //   to a valid jar entry
            {
              nsIZipEntry* entry;
              PRInt32 result = GetEntry(curItemName, &entry);
              if (result != ZIP_OK || !entry)
                curItem->mType = JAR_INVALID;
            }
            //-- Check for duplicates
            nsStringKey key(curItemName);
            if (mManifestData.Exists(&key))
              curItem->mType = JAR_INVALID;
          }
        }

        if (curItem->mType == JAR_INVALID)
          delete curItem;
        else //-- calculate section digests 
        {
          PRUint32 sectionLength = curPos - sectionStart;
          for (PRInt32 d=0; d<JAR_DIGEST_COUNT; d++)
          {
            PRUint32 id;
            nsresult rv = DigestBegin(&id, d);
            if (NS_SUCCEEDED(rv))
              rv = DigestData(id, sectionStart, sectionLength);
            if (NS_SUCCEEDED(rv))
              rv = CalculateDigest(id, &(curItem->calculatedSectionDigests[d]));
          }
          //-- Save item in the hashtable
          nsStringKey itemKey(curItemName);
          mManifestData.Put(&itemKey, (void*)curItem);
        }
        if (nextLineStart == nsnull) // end-of-file
          break;

        sectionStart = nextLineStart;
        curItem = new nsJARManifestItem();
      } // (aFileType == JAR_MF)
      else
        //-- file type is SF, compare digest with calculated 
        //   section digests from MF file.
      {
        if (foundName)
        {
          nsJARManifestItem* curSFItem;
          nsStringKey key(curItemName);
          curSFItem = (nsJARManifestItem*)mManifestData.Get(&key);
          if(curSFItem)
          {
            PRBool valid = PR_TRUE;
            if (aPrincipal == nsnull) 
              // SF file failed verification
              valid = PR_FALSE;
            else // Compare calculated digests
            {
              PRBool foundDigest = PR_FALSE;
              for (int a=0; a<JAR_DIGEST_COUNT; a++)
              {
                valid = valid && 
                        ( (storedSectionDigests[a].Length() == 0) ||
                          storedSectionDigests[a] == 
                            (const char*)curSFItem->calculatedSectionDigests[a] );
                foundDigest = foundDigest || (storedSectionDigests[a].Length() != 0);
              }
              if (foundDigest && valid)
              {
                curSFItem->setPrincipal(aPrincipal);
              }
            }
            curSFItem->valid = valid;
            if (aLastSFFile)
              // Free the calculated digests; we won't need them again.
              for(PRInt16 countB=0; countB<JAR_DIGEST_COUNT; countB++)
                JAR_NULLFREE(curSFItem->calculatedSectionDigests[countB])
          } // if(curSFItem)
        } // if(foundName)

        if(nextLineStart == nsnull) // end-of-file
          break;
      } // aFileType == JAR_SF
      foundName = PR_FALSE;
      continue;
    } // if(linelen == 0)

    //-- Look for continuations (beginning with a space) on subsequent lines
    //   and append them to the current line.
    while(*nextLineStart == ' ')
    {
      curPos = nextLineStart;
      PRInt32 continuationLen = ReadLine(&nextLineStart) - 1;
      nsAutoString continuation(curPos+1, continuationLen);
      curLine += continuation;
      linelen += continuationLen;
    }

    //-- Find colon in current line, this separates name from value
    PRInt32 colonPos = curLine.FindChar(':');
    if (colonPos == -1)    // No colon on line, ignore line
      continue;
    //-- Break down the line
    nsCAutoString lineName;
    curLine.Left(lineName, colonPos);
    nsCAutoString lineData;
    curLine.Mid(lineData, colonPos+2, linelen - (colonPos+2));

    //-- Lines to look for:
    // (1) *-Digest:
    if (lineName.Find("-Digest",PR_TRUE) != -1) 
    //-- This is a digest line, save the data in the appropriate place 
    {
      PRInt32 digest;
      //** Note that in the following comparisons, the third argument to 
      //   Compare MUST be the length of the first argument. **
      if (lineName.Compare("MD5-", PR_TRUE, 4) == 0)
        digest = JAR_MD5;
      else if ( lineName.Compare("SHA1-", PR_TRUE, 5) == 0 ||
                lineName.Compare("SHA-", PR_TRUE, 4) == 0 )
        digest = JAR_SHA1;
      else // Unsupported algorithm
        continue; 

      if(aFileType == JAR_MF)
      {
        curItem->storedEntryDigests[digest] = 
          (char*)PR_MALLOC(lineData.Length()+1);
        if (!(curItem->storedEntryDigests[digest]))
          return NS_ERROR_OUT_OF_MEMORY;
        (void)PL_strcpy(curItem->storedEntryDigests[digest], lineData);
      }
      else
        storedSectionDigests[digest] = lineData;
      continue;
    }
    
    // (2) Name: associates this manifest section with a file in the jar.
    if (lineName.Compare("Name", PR_TRUE) == 0) 
    {
      if (!(aFileType == JAR_MF && curItem->mType == JAR_GLOBAL))
        curItemName = lineData;
      foundName = PR_TRUE;
      continue;
    }

    // (3) Magic: this may be an inline Javascript. 
    //     We can't do any other kind of magic.
    if ( aFileType == JAR_MF &&
         lineName.Compare("Magic", PR_TRUE) == 0 && 
         curItem->mType != JAR_GLOBAL ) 
    {
      if(lineData.Compare("javascript",PR_TRUE) == 0)
        curItem->mType = JAR_EXTERNAL;
      else
        curItem->mType = JAR_INVALID;
      continue;
    }

  } // for (;;)
  return NS_OK;
} //ParseOneFile()

//----------------------------------------------
// Debugging functions
//----------------------------------------------

#if 0
PR_STATIC_CALLBACK(PRBool)
PrintManItem(nsHashKey* aKey, void* aData, void* closure)
{
  nsJARManifestItem* manItem = (nsJARManifestItem*)aData;
    if (manItem)
    {
      nsStringKey* key2 = (nsStringKey*)aKey;
      printf("------------\nName:%s.\n",key2->GetString().ToNewCString());
      {
        if (manItem->mPrincipal)
        {
          char* toStr;
          char* caps;
          manItem->mPrincipal->ToString(&toStr);
          manItem->mPrincipal->CapabilitiesToString(&caps);
          printf("Principal: %s.\n Caps: %s.\n", toStr, caps);
        }
        else
          printf("No Principal.\n");
        printf("verifyComplete:%i.\n",manItem->verifyComplete);
        printf("valid:%i.\n",manItem->valid);
        printf("entryDigestsCalculated:%i.\n",manItem->entryDigestsCalculated);
        for (PRInt32 x=0; x<JAR_DIGEST_COUNT; x++)
          printf("calculated section digest:%s.\n",
                 manItem->calculatedSectionDigests[x]);
        for (PRInt32 y=0; y<JAR_DIGEST_COUNT; y++)
          printf("stored entry digest:%s.\n",
                 manItem->storedEntryDigests[y]);
      }
    }
    return PR_TRUE;
}
#endif

void nsJAR::DumpMetadata()
{
#if 0
  printf("######## nsJAR::DumpMetadata Start ############\n");
  mManifestData.Enumerate(PrintManItem);
  printf("########  nsJAR::DumpMetadata End  ############\n");
#endif
} 

//----------------------------------------------
// nsJAREnumerator constructor and destructor
//----------------------------------------------

nsJAREnumerator::nsJAREnumerator(nsZipFind *aFind)
: mFind(aFind),
  mCurr(nsnull),
  mIsCurrStale(PR_TRUE)
{
    mArchive = mFind->GetArchive();
    NS_INIT_REFCNT();
}

nsJAREnumerator::~nsJAREnumerator()
{
    mArchive->FindFree(mFind);
}

NS_IMPL_ISUPPORTS(nsJAREnumerator, NS_GET_IID(nsISimpleEnumerator));

//----------------------------------------------
// nsJAREnumerator::HasMoreElements
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::HasMoreElements(PRBool* aResult)
{
    PRInt32 err;

    if (!mFind)
        return NS_ERROR_NOT_INITIALIZED;

    // try to get the next element
    if (mIsCurrStale)
    {
        err = mArchive->FindNext( mFind, &mCurr );
        if (err == ZIP_ERR_FNF)
        {
            *aResult = PR_FALSE;
            return NS_OK;
        }
        if (err != ZIP_OK)
            return NS_ERROR_FAILURE; // no error translation

        mIsCurrStale = PR_FALSE;
    }

    *aResult = PR_TRUE;
    return NS_OK;
}

//----------------------------------------------
// nsJAREnumerator::GetNext
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::GetNext(nsISupports** aResult)
{
    nsresult rv;
    PRBool   bMore;

    // check if the current item is "stale"
    if (mIsCurrStale)
    {
        rv = HasMoreElements( &bMore );
        if (NS_FAILED(rv))
            return rv;
        if (bMore == PR_FALSE)
        {
            *aResult = nsnull;  // null return value indicates no more elements
            return NS_OK;
        }
    }

    // pack into an nsIJARItem
    nsJARItem* jarItem = new nsJARItem();
    if(jarItem)
    {
      NS_ADDREF(jarItem);
      jarItem->Init(mCurr);
      *aResult = jarItem;
      mIsCurrStale = PR_TRUE; // we just gave this one away
      return NS_OK;
    }
    else
      return NS_ERROR_OUT_OF_MEMORY;
}




//-------------------------------------------------
// nsJARItem constructors and destructor
//-------------------------------------------------

nsJARItem::nsJARItem()
{
    NS_INIT_ISUPPORTS();
}

nsJARItem::~nsJARItem()
{
}

NS_IMPL_ISUPPORTS1(nsJARItem, nsIZipEntry);

void nsJARItem::Init(nsZipItem* aZipItem)
{
  mZipItem = aZipItem;
}

//------------------------------------------
// nsJARItem::GetName
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetName(char * *aName)
{
    char *namedup;

    if ( !aName )
        return NS_ERROR_NULL_POINTER;
    if ( !mZipItem->name )
        return NS_ERROR_FAILURE;

    namedup = PL_strndup( mZipItem->name, mZipItem->namelen );
    if ( !namedup )
        return NS_ERROR_OUT_OF_MEMORY;

    *aName = namedup;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetCompression
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetCompression(PRUint16 *aCompression)
{
    if (!aCompression)
        return NS_ERROR_NULL_POINTER;
    if (!mZipItem->compression)
        return NS_ERROR_FAILURE;

    *aCompression = mZipItem->compression;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetSize
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetSize(PRUint32 *aSize)
{
    if (!aSize)
        return NS_ERROR_NULL_POINTER;
    if (!mZipItem->size)
        return NS_ERROR_FAILURE;

    *aSize = mZipItem->size;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetRealSize
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetRealSize(PRUint32 *aRealsize)
{
    if (!aRealsize)
        return NS_ERROR_NULL_POINTER;
    if (!mZipItem->realsize)
        return NS_ERROR_FAILURE;

    *aRealsize = mZipItem->realsize;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetCrc32
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetCRC32(PRUint32 *aCrc32)
{
    if (!aCrc32)
        return NS_ERROR_NULL_POINTER;
    if (!mZipItem->crc32)
        return NS_ERROR_FAILURE;

    *aCrc32 = mZipItem->crc32;
    return NS_OK;
}


//-------------------------------------------------
// nsJARManifestItem constructors and destructor
//-------------------------------------------------
nsJARManifestItem::nsJARManifestItem(): mType(JAR_INTERNAL),
                                        verifyComplete(PR_FALSE),
                                        valid(PR_TRUE),
                                        entryDigestsCalculated(PR_FALSE),
                                        mPrincipal(nsnull)
{
  // Initialize digests to null
  for(PRInt32 count = 0; count<JAR_DIGEST_COUNT; count++)
  {
    calculatedSectionDigests[count] = nsnull;
    calculatedEntryDigests[count] = nsnull;
    storedEntryDigests[count] = nsnull;
  }
}

nsJARManifestItem::~nsJARManifestItem()
{
  // Delete digests if necessary
  for(PRInt32 count = 0; count<JAR_DIGEST_COUNT; count++)
  {
    PR_FREEIF(calculatedSectionDigests[count]);
    PR_FREEIF(calculatedEntryDigests[count]);
    PR_FREEIF(storedEntryDigests[count]);
  }
  NS_IF_RELEASE(mPrincipal);
}
    

//-------------------------------------------------
// nsJARManifestItem accessor functions
//-------------------------------------------------
void nsJARManifestItem::setPrincipal(nsIPrincipal *aPrincipal) 
{
  NS_IF_RELEASE(mPrincipal);
  mPrincipal = aPrincipal;
  NS_IF_ADDREF(mPrincipal);
}
    
void nsJARManifestItem::getPrincipal(nsIPrincipal **result) 
{
  if (verifyComplete && valid) 
  {
    *result = mPrincipal;
    NS_IF_ADDREF(*result);
  } 
  else 
    *result = nsnull;
}

PRBool nsJARManifestItem::hasPrincipal()
{
  return verifyComplete && valid && mPrincipal;
}
