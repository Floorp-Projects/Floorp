/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIServiceManager.h"
#include "plbase64.h"

#ifndef XP_MAC
#include "nsIPSMComponent.h"
#endif

#ifdef XP_UNIX
  #include <sys/stat.h>
#elif defined (XP_PC)
  #include <io.h>
#endif

//----------------------------------------------
// Errors and other utility definitions
//----------------------------------------------
#ifndef __gen_nsIFile_h__
#define NS_ERROR_FILE_UNRECOGNIZED_PATH         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 1)
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
    _ptr = nsnull;         \
  }

//----------------------------------------------
// nsJARManifestItem declaration
//----------------------------------------------
/*
 * nsJARManifestItem contains meta-information pertaining 
 * to an individual JAR entry, taken from the 
 * META-INF/MANIFEST.MF and META-INF/ *.SF files.
 * This is security-critical information, defined here so it is not
 * accessible from anywhere else.
 */
typedef enum
{
  JAR_INVALID       = 1,
  JAR_INTERNAL      = 2,
  JAR_EXTERNAL      = 3
} JARManifestItemType;

class nsJARManifestItem
{
public:
  JARManifestItemType mType;

  // the entity which signed this item
  nsCOMPtr<nsIPrincipal>  mPrincipal;

  // True if the second step of verification (VerifyEntry) 
  // has taken place:
  PRBool              step2Complete; 
  
  // Not signed, valid, or failure code
  PRInt16             status;
  
  // Internal storage of digests
  char*               calculatedSectionDigest;
  char*               storedEntryDigest;

  nsJARManifestItem();
  virtual ~nsJARManifestItem();
};

//-------------------------------------------------
// nsJARManifestItem constructors and destructor
//-------------------------------------------------
nsJARManifestItem::nsJARManifestItem(): mType(JAR_INTERNAL),
                                        step2Complete(PR_FALSE),
                                        status(nsIZipReader::NOT_SIGNED),
                                        calculatedSectionDigest(nsnull),
                                        storedEntryDigest(nsnull)
{
}

nsJARManifestItem::~nsJARManifestItem()
{
  // Delete digests if necessary
  PR_FREEIF(calculatedSectionDigest);
  PR_FREEIF(storedEntryDigest);
}

//----------------------------------------------
// nsJAR constructor/destructor
//----------------------------------------------
PR_STATIC_CALLBACK(PRBool)
DeleteManifestEntry(nsHashKey* aKey, void* aData, void* closure)
{
//-- deletes an entry in  mManifestData.
  PR_FREEIF(aData);
  return PR_TRUE;
}

// The following initialization makes a guess of 25 entries per jarfile.
nsJAR::nsJAR(): mManifestData(nsnull, nsnull, DeleteManifestEntry, nsnull, 25),
                step1Complete(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsJAR::~nsJAR()
{ 
}

NS_IMPL_ISUPPORTS1(nsJAR, nsIZipReader);

//----------------------------------------------
// nsJAR public implementation
//----------------------------------------------
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

  nsZipItem *item = 0;
  PRInt32 err = mZip.ExtractFileToFileDesc(zipEntry, fd, &item);
  PR_Close(fd);

  if (err != ZIP_OK)
    outFile->Delete(PR_FALSE);
  else
  {
#if defined(XP_UNIX)
    char *path;

    rv = outFile->GetPath(&path); 
    if (NS_SUCCEEDED(rv))
      chmod(path, item->mode);
#endif

    RestoreModTime(item, outFile);  // non-fatal if this fails, ignore errors
  }

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
nsJAR::FindEntries(const char *aPattern, nsISimpleEnumerator **result)
{
  if (!result)
    return NS_ERROR_INVALID_POINTER;
    
  nsZipFind *find = mZip.FindInit(aPattern);
  if (!find)
    return NS_ERROR_OUT_OF_MEMORY;

  nsISimpleEnumerator *zipEnum = new nsJAREnumerator(find);
  if (!zipEnum)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF( zipEnum );

  *result = zipEnum;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::GetInputStream(const char *aFilename, nsIInputStream **result)
{
  if (!result)
    return NS_OK;
  return CreateInputStream(aFilename, PR_TRUE, result);
}

//-- The following #defines are used by ParseManifest()
//   and ParseOneFile(). The header strings are defined in the JAR specification.
#define JAR_MF 1
#define JAR_SF 2
#define JAR_MF_SEARCH_STRING "(M|/M)ETA-INF/(M|m)(ANIFEST|anifest).(MF|mf)$"
#define JAR_SF_SEARCH_STRING "(M|/M)ETA-INF/*.(SF|sf)$"
#define JAR_MF_HEADER (const char*)"Manifest-Version: 1.0"
#define JAR_SF_HEADER (const char*)"Signature-Version: 1.0"

NS_IMETHODIMP
nsJAR::ParseManifest()
{
#ifdef XP_MAC
return NS_OK;
#else
  //-- Verification Step 1
  if (step1Complete)
    return NS_OK;
  step1Complete = PR_TRUE;   

  //-- (1)Manifest (MF) file
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> files;
  rv = FindEntries(JAR_MF_SEARCH_STRING, getter_AddRefs(files));
  if (!files) rv = NS_ERROR_FAILURE;
  if (NS_FAILED(rv)) return rv;

  //-- Load the file into memory
  nsCOMPtr<nsJARItem> file;
  rv = files->GetNext(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file)  return rv;
  PRBool more;
  rv = files->HasMoreElements(&more);
  if (NS_FAILED(rv))  return rv;
  if (more) return NS_ERROR_FILE_CORRUPTED; // More than one MF file
  nsXPIDLCString manifestFilename;
  rv = file->GetName(getter_Copies(manifestFilename));
  if (!manifestFilename || NS_FAILED(rv)) return rv;
  nsXPIDLCString manifestBuffer;
  rv = LoadEntry(manifestFilename, getter_Copies(manifestBuffer));
  if (NS_FAILED(rv)) return rv;

  //-- Parse it
  rv = ParseOneFile(manifestBuffer, JAR_MF, nsnull, 0);
  if (NS_FAILED(rv)) return rv;
  DumpMetadata("PM Pass 1 End");

  //-- (2)Signature (SF) file
  // If there are multiple signatures, we select one at random.
  rv = FindEntries(JAR_SF_SEARCH_STRING, getter_AddRefs(files));
  if (!files) rv = NS_ERROR_FAILURE;
  if (NS_FAILED(rv)) return rv;
  //-- Get an SF file
  rv = files->GetNext(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return rv;
  rv = file->GetName(getter_Copies(manifestFilename));
  if (NS_FAILED(rv)) return rv;

  PRUint32 manifestLen;
  rv = LoadEntry(manifestFilename, getter_Copies(manifestBuffer), &manifestLen);
  if (NS_FAILED(rv)) return rv;
  
  //-- Get its corresponding signature file
  nsCAutoString sigFilename;
  sigFilename = manifestFilename;
  PRInt32 extension = sigFilename.RFindChar('.') + 1;
  NS_ASSERTION(extension != 0, "Manifest Parser: Missing file extension.");
  (void)sigFilename.Cut(extension, 2);
  nsXPIDLCString sigBuffer;
  PRUint32 sigLen;
  rv = LoadEntry(sigFilename+"rsa", getter_Copies(sigBuffer), &sigLen);
  if (NS_FAILED(rv))
    rv = LoadEntry(sigFilename+"RSA", getter_Copies(sigBuffer), &sigLen);
  if (NS_FAILED(rv))
    rv = LoadEntry(sigFilename+"dsa", getter_Copies(sigBuffer), &sigLen);
  if (NS_FAILED(rv))
    rv = LoadEntry(sigFilename+"DSA", getter_Copies(sigBuffer), &sigLen);
  if (NS_FAILED(rv)) return rv;
  
  //-- Verify that the signature file is a valid signature of the SF file
  nsCOMPtr<nsIPrincipal> principal;
  PRInt16 preStatus;
  rv = VerifySignature(manifestBuffer, manifestLen, 
                       sigBuffer, sigLen, getter_AddRefs(principal), &preStatus);
  if (NS_FAILED(rv)) return rv;
  
  //-- Parse the SF file. If the verification above failed, principal
  // is null, and ParseOneFile will mark the relevant entries as invalid.
  // if ParseOneFile fails, then it has no effect, and we can safely 
  // continue to the next SF file, or return. 
  ParseOneFile(manifestBuffer, JAR_SF, principal, preStatus);
  DumpMetadata("PM Pass 2 End");

  return NS_OK;
  #endif
}

NS_IMETHODIMP  
nsJAR::GetCertificatePrincipal(const char* aFilename, nsIPrincipal** aPrincipal, 
                               PRInt16* result)
{
#ifdef XP_MAC
	return NS_ERROR_NOT_IMPLEMENTED;
#else
  //-- Parameter check
  if (!aFilename)
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aPrincipal)
    return NS_ERROR_NULL_POINTER;
  *aPrincipal = nsnull;

  DumpMetadata("GetPrincipal");
  //-- Find the item
  nsStringKey key(aFilename);
  nsJARManifestItem* manItem = (nsJARManifestItem*)mManifestData.Get(&key);
  if (manItem)
    NS_ASSERTION(manItem->step2Complete,
                 "Attempt to get principal before verifying signature.");
  if(!manItem || !manItem->step2Complete)
  {
    *result = nsIZipReader::NOT_SIGNED;
    return NS_OK;
  }

  *result = manItem->status;
  if (manItem->status == nsIZipReader::VALID)
  {
    *aPrincipal = manItem->mPrincipal;
    NS_IF_ADDREF(*aPrincipal);
  }

  return NS_OK;
#endif
}

//----------------------------------------------
// nsJAR private implementation
//----------------------------------------------
nsresult nsJAR::CreateInputStream(const char* aFilename, PRBool verify,
                                  nsIInputStream** result)
{
  nsresult rv;
  nsJARInputStream* jis = nsnull;
  rv = nsJARInputStream::Create(nsnull, NS_GET_IID(nsIInputStream), (void**)&jis);
  if (!jis) return NS_ERROR_FAILURE;

  rv = jis->Init(this, aFilename, verify);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  *result = (nsIInputStream*)jis;
  return NS_OK;
}

nsresult 
nsJAR::LoadEntry(const char* aFilename, char** aBuf, PRUint32* aBufLen)
{
  //-- Get a stream for reading the manifest file
  nsresult rv;
  nsCOMPtr<nsIInputStream> manifestStream;
  rv = CreateInputStream(aFilename, PR_FALSE, getter_AddRefs(manifestStream));
  if (NS_FAILED(rv)) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
  
  //-- Read the manifest file into memory
  char* buf;
  PRUint32 len;
  rv = manifestStream->Available(&len);
  if (NS_FAILED(rv)) return rv;
  buf = (char*)PR_MALLOC(len+1);
  if (!buf) return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 bytesRead;
  rv = manifestStream->Read(buf, len, &bytesRead);
  if (bytesRead != len) 
    rv = NS_ERROR_FILE_CORRUPTED;
  if (NS_FAILED(rv)) return rv;
  buf[len] = '\0'; //Null-terminate the buffer
  *aBuf = buf;
  if (aBufLen)
    *aBufLen = len;
  return NS_OK;
}


PRInt32
nsJAR::ReadLine(const char** src)
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

nsresult
nsJAR::ParseOneFile(const char* filebuf, PRInt16 aFileType,
                    nsIPrincipal* aPrincipal, PRInt16 aPreStatus)
{
  //-- Check file header
  const char* nextLineStart = filebuf;
  nsCAutoString curLine;
  PRInt32 linelen;
  linelen = ReadLine(&nextLineStart);
  curLine.Assign(filebuf, linelen);
  if ( ((aFileType == JAR_MF) && (curLine != JAR_MF_HEADER) ) ||
       ((aFileType == JAR_SF) && (curLine != JAR_SF_HEADER) ) )
     return NS_ERROR_FILE_CORRUPTED;

  //-- Skip header section
  do {
    linelen = ReadLine(&nextLineStart);
  } while (linelen > 0);

  //-- Set up parsing variables
  const char* curPos;
  const char* sectionStart = nextLineStart;

  nsJARManifestItem* curItemMF;
  PRBool foundName = PR_FALSE;
  if (aFileType == JAR_MF)
    curItemMF = new nsJARManifestItem();
  nsCAutoString curItemName;
  nsCAutoString storedSectionDigest;

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
        if (curItemMF->mType != JAR_INVALID)
        { 
          //-- Did this section have a name: line?
          if(!foundName)
            curItemMF->mType = JAR_INVALID;
          else 
          {
            if (curItemMF->mType == JAR_INTERNAL)
            {
            //-- If it's an internal item, it must correspond 
            //   to a valid jar entry
              nsIZipEntry* entry;
              PRInt32 result = GetEntry(curItemName, &entry);
              if (result != ZIP_OK || !entry)
                curItemMF->mType = JAR_INVALID;
            }
            //-- Check for duplicates
            nsStringKey key(curItemName);
            if (mManifestData.Exists(&key))
              curItemMF->mType = JAR_INVALID;
          }
        }

        if (curItemMF->mType == JAR_INVALID)
          delete curItemMF;
        else //-- calculate section digest
        {
          PRUint32 sectionLength = curPos - sectionStart;
          CalculateDigest(sectionStart, sectionLength,
                          &(curItemMF->calculatedSectionDigest));
          //-- Save item in the hashtable
          nsStringKey itemKey(curItemName);
          mManifestData.Put(&itemKey, (void*)curItemMF);
        }
        if (nextLineStart == nsnull) // end-of-file
          break;

        sectionStart = nextLineStart;
        curItemMF = new nsJARManifestItem();
      } // (aFileType == JAR_MF)
      else
        //-- file type is SF, compare digest with calculated 
        //   section digests from MF file.
      {
        if (foundName)
        {
          nsJARManifestItem* curItemSF;
          nsStringKey key(curItemName);
          curItemSF = (nsJARManifestItem*)mManifestData.Get(&key);
          if(curItemSF)
          {
            NS_ASSERTION(curItemSF->status == nsJAR::NOT_SIGNED,
                         "SECURITY ERROR: nsJARManifestItem not correctly initialized");
            curItemSF->status = aPreStatus;
            if (curItemSF->status == nsIZipReader::VALID)
            { // Compare digests
              if (storedSectionDigest.Length() == 0)
                curItemSF->status = nsIZipReader::NOT_SIGNED;
              else
              {
                if (storedSectionDigest !=
                    (const char*)curItemSF->calculatedSectionDigest)
                  curItemSF->status = nsIZipReader::INVALID_MANIFEST;
                else
                  curItemSF->mPrincipal = aPrincipal;
                JAR_NULLFREE(curItemSF->calculatedSectionDigest)
                storedSectionDigest = "";
              }
            } // (aPrincipal != nsnull)
          } // if(curItemSF)
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
      nsCAutoString continuation(curPos+1, continuationLen);
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
    // (1) Digest:
    if (lineName.Compare("SHA1-Digest",PR_TRUE) == 0) 
    //-- This is a digest line, save the data in the appropriate place 
    {
      if(aFileType == JAR_MF)
      {
        curItemMF->storedEntryDigest = (char*)PR_MALLOC(lineData.Length()+1);
        if (!(curItemMF->storedEntryDigest))
          return NS_ERROR_OUT_OF_MEMORY;
        PL_strcpy(curItemMF->storedEntryDigest, lineData);
      }
      else
        storedSectionDigest = lineData;
      continue;
    }
    
    // (2) Name: associates this manifest section with a file in the jar.
    if (!foundName && lineName.Compare("Name", PR_TRUE) == 0) 
    {
      curItemName = lineData;
      foundName = PR_TRUE;
      continue;
    }

    // (3) Magic: this may be an inline Javascript. 
    //     We can't do any other kind of magic.
    if ( aFileType == JAR_MF &&
         lineName.Compare("Magic", PR_TRUE) == 0) 
    {
      if(lineData.Compare("javascript", PR_TRUE) == 0)
        curItemMF->mType = JAR_EXTERNAL;
      else
        curItemMF->mType = JAR_INVALID;
      continue;
    }

  } // for (;;)
  return NS_OK;
} //ParseOneFile()

nsresult
nsJAR::VerifyEntry(const char* aEntryName, char* aEntryData,
                   PRUint32 aLen)
{
#ifdef XP_MAC
	return NS_OK;
#else
  //-- Verification Step 2
  // Check that verification is supported and step 1 has been done

  NS_ASSERTION(step1Complete, 
               "Verification step 2 called before step 1 complete");
  if (!step1Complete) return NS_ERROR_FAILURE;

  //-- Get the manifest item
  nsStringKey key(aEntryName);
  nsJARManifestItem* manItem = (nsJARManifestItem*)mManifestData.Get(&key);
  if (!manItem)
    return NS_OK;
  if (manItem->status == nsIZipReader::VALID)
  {
    if(!manItem->storedEntryDigest)
      // No entry digests in manifest file. Entry is unsigned.
      manItem->status = nsIZipReader::NOT_SIGNED;
    else
    { //-- Calculate and compare digests
      char* calculatedEntryDigest;
      nsresult rv = CalculateDigest(aEntryData, aLen, &calculatedEntryDigest);
      if (NS_FAILED(rv)) return rv;
      if (PL_strcmp(manItem->storedEntryDigest, calculatedEntryDigest) != 0)
        manItem->status = nsIZipReader::INVALID_ENTRY;
      JAR_NULLFREE(calculatedEntryDigest)
      JAR_NULLFREE(manItem->storedEntryDigest)
    }

    if(manItem->status != nsIZipReader::VALID)
      manItem->mPrincipal = null_nsCOMPtr();
  }

  manItem->step2Complete = PR_TRUE;
  DumpMetadata("VerifyEntry end");
#endif
  return NS_OK;
}

nsresult
nsJAR::RestoreModTime(nsZipItem *aItem, nsIFile *aExtractedFile)
{
  if (!aItem || !aExtractedFile)
    return NS_ERROR_NULL_POINTER;
  
  char *timestr;
  PRTime prtime;
  nsresult rv = NS_OK;
  
  timestr = aItem->GetModTime();
  if (timestr)
  {
    if (PR_SUCCESS == PR_ParseTimeString(timestr, PR_FALSE, &prtime))
      rv = aExtractedFile->SetLastModificationDate(prtime);

    JAR_NULLFREE(timestr);
  }

  return rv;
}

//----------------------------------------------
// Hashing and verification functions
//----------------------------------------------

nsresult nsJAR::CalculateDigest(const char* aInBuf, PRUint32 aLen,
				char** digest)
{
#ifndef XP_MAC
  *digest = nsnull;
  nsresult rv;
  NS_WITH_SERVICE(nsIPSMComponent, psmComp, PSM_COMPONENT_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  //-- Calculate the digest
  PRUint32 id;
  rv = psmComp->HashBegin(nsIPSMComponent::SHA1, &id);
  if (NS_FAILED(rv)) return rv;

  rv = psmComp->HashUpdate(id, aInBuf, aLen);
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 len;
  char* rawDigest = (char*)PR_MALLOC(nsIPSMComponent::SHA1_LENGTH);
  if (rawDigest == nsnull) return NS_ERROR_OUT_OF_MEMORY;
  rv = psmComp->HashEnd(id, &rawDigest, &len, nsIPSMComponent::SHA1_LENGTH);
  if (NS_FAILED(rv)) { PR_FREEIF(rawDigest); return rv; }

  //-- Encode the digest in base64
  *digest = PL_Base64Encode(rawDigest, len, *digest);
  if (!(*digest)) { PR_FREEIF(rawDigest); return NS_ERROR_OUT_OF_MEMORY; }
  
  PR_FREEIF(rawDigest);
 #endif
  return NS_OK;
}

nsresult nsJAR::VerifySignature(const char* sfBuf, PRUint32 sfBufLen,
				const char* rsaBuf, PRUint32 rsaBufLen,
				nsIPrincipal** aPrincipal, PRInt16* status)
{
  nsresult rv = NS_OK;
#ifndef XP_MAC
  NS_WITH_SERVICE(nsIPSMComponent, psmComp, PSM_COMPONENT_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 id;
  rv = psmComp->VerifyRSABegin(&id);
  if (NS_SUCCEEDED(rv))
    rv = psmComp->VerifyRSAUpdate(id, rsaBuf, rsaBufLen);
  PRInt32 psmStatus;
  if (NS_SUCCEEDED(rv))
    rv = psmComp->VerifyRSAEnd(id, sfBuf, sfBufLen, PR_TRUE, aPrincipal, &psmStatus);
  switch (psmStatus)
  {
  case nsIPSMComponent::VERIFY_NOSIG:
    *status = nsIZipReader::NOT_SIGNED; break;
  case nsIPSMComponent::VERIFY_OK:
    *status = nsIZipReader::VALID; break;
  case nsIPSMComponent::VERIFY_ERROR_UNKNOWN_CA: 
    *status = nsIZipReader::INVALID_UNKNOWN_CA; break;
  default:
    *status = nsIZipReader::INVALID_SIG;
  }
#endif
  return rv;
}

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
      char* name = key2->GetString().ToNewCString();
      if (PL_strcmp(name, "") != 0)
      {
        printf("------------\nName:%s.\n",name);
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
        printf("step2Complete:%i.\n",manItem->step2Complete);
        printf("valid:%i.\n",manItem->valid);
        /*
        for (PRInt32 x=0; x<JAR_DIGEST_COUNT; x++)
          printf("calculated section digest:%s.\n",
                 manItem->calculatedSectionDigests[x]);
        for (PRInt32 y=0; y<JAR_DIGEST_COUNT; y++)
          printf("stored entry digest:%s.\n",
                 manItem->storedEntryDigests[y]);
        */
      }
    }
    return PR_TRUE;
}
#endif

void nsJAR::DumpMetadata(const char* aMessage)
{
#if 0
  printf("### nsJAR::DumpMetadata at %s ###\n", aMessage);
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

