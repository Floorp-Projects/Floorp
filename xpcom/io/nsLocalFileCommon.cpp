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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#include "nsIServiceManager.h"
#ifndef XPCOM_STANDALONE
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIURLParser.h"
#endif /* XPCOM_STANDALONE */

#include "nsFileSpec.h"  // evil ftang hack

#include "nsLocalFile.h" // includes platform-specific headers

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#ifndef XPCOM_STANDALONE
static NS_DEFINE_CID(kStdURLParserCID, NS_STANDARDURLPARSER_CID);
#endif


class nsFSStringConversion {
public:
     static void CleanUp();
     static nsresult UCSToNewFS( const PRUnichar* aIn, char** aOut);  
     static nsresult FSToNewUCS( const char* aIn, PRUnichar** aOut);  

private:
     static nsresult PrepareFSCharset();
     static nsresult PrepareEncoder();
     static nsresult PrepareDecoder();
     static nsString* mFSCharset;
#ifndef XPCOM_STANDALONE
     static nsIUnicodeEncoder* mEncoder;
     static nsIUnicodeDecoder* mDecoder;
#endif /* XPCOM_STANDALONE */
};

nsString* nsFSStringConversion::mFSCharset = nsnull;
#ifndef XPCOM_STANDALONE
nsIUnicodeEncoder* nsFSStringConversion::mEncoder = nsnull;
nsIUnicodeDecoder* nsFSStringConversion::mDecoder = nsnull;
#endif /* XPCOM_STANDALONE */

#define GET_UCS( func , arg)                                    \
{                                                               \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = (func)(&tmp))) {                       \
     if(NS_SUCCEEDED(res = nsFSStringConversion::FSToNewUCS(tmp, (arg)))){ \
       nsMemory::Free(tmp);                                     \
     }                                                          \
   }                                                            \
   return res;                                                  \
}
#define VOID_SET_UCS( func , arg, assertion_msg)                \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((arg), &tmp))) { \
     (func)(tmp);                                               \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   NS_ASSERTION(NS_SUCCEEDED(res), assertion_msg);              \
}
#define SET_UCS( func , arg)                                    \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((arg), &tmp))) { \
     res = (func)(tmp);                                         \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_2( func , arg1, arg2)                     \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((arg2), &tmp))){ \
     res = (func)(arg1, tmp);                                   \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_1( func , path, followLinks, result)      \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((path), &tmp))){ \
     res = (func)(tmp, followLinks, result);                    \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}

void
nsFSStringConversion::CleanUp()
{
#ifndef XPCOM_STANDALONE
  NS_IF_RELEASE(mEncoder);
  NS_IF_RELEASE(mDecoder);
#endif /* XPCOM_STANDALONE */
}

/* static */ nsresult
nsFSStringConversion::PrepareFSCharset()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(!mFSCharset)
   { 
     // lazy eval of the file system charset
     nsCOMPtr<nsIPlatformCharset> pcharset = 
              do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
     if (NS_SUCCEEDED(res) && !pcharset) res = NS_ERROR_NULL_POINTER;
     if (NS_SUCCEEDED(res)) {
        mFSCharset = new nsString();
        if (!mFSCharset)
          res = NS_ERROR_OUT_OF_MEMORY;
     }
     if(NS_SUCCEEDED(res)) {
        res = pcharset->GetCharset(kPlatformCharsetSel_FileName, *mFSCharset);
     } 
     if (NS_FAILED(res)) {
       NS_WARNING("cannot get platform charset");
     }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
/* static */ nsresult
nsFSStringConversion::PrepareEncoder()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(! mEncoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           nsCOMPtr<nsICharsetConverterManager> ucmgr = 
                    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeEncoder( mFSCharset, &mEncoder);
           NS_ASSERTION((NS_SUCCEEDED(res) && mEncoder), 
                   "cannot find the unicode encoder");
       }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
/* static */ nsresult
nsFSStringConversion::PrepareDecoder()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(! mDecoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           nsCOMPtr<nsICharsetConverterManager> ucmgr = 
                    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeDecoder( mFSCharset, &mDecoder);
           NS_ASSERTION((NS_SUCCEEDED(res) && mDecoder), 
                   "cannot find the unicode decoder");
       }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
/* static */ nsresult
nsFSStringConversion::UCSToNewFS( const PRUnichar* aIn, char** aOut)
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = PrepareEncoder();
   if(NS_SUCCEEDED(res)) 
   {
        PRInt32 inLength = nsCRT::strlen(aIn);
        PRInt32 outLength;
        res= mEncoder->GetMaxLength(aIn, inLength,&outLength);
        if(NS_SUCCEEDED(res)) {
           *aOut = (char*)nsMemory::Alloc(outLength+1);
           if(nsnull != *aOut) {
              res = mEncoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsMemory::Free(*aOut);
                 *aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
/* static */ nsresult
nsFSStringConversion::FSToNewUCS( const char* aIn, PRUnichar** aOut)
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = PrepareDecoder();
   if(NS_SUCCEEDED(res)) 
   {
        PRInt32 inLength = nsCRT::strlen(aIn);
        PRInt32 outLength;
        res= mDecoder->GetMaxLength(aIn, inLength,&outLength);
        if(NS_SUCCEEDED(res)) {
           *aOut = (PRUnichar*)nsMemory::Alloc(2*(outLength+1));
           if(nsnull != *aOut) {
              res = mDecoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsMemory::Free(*aOut);
                 *aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}

void NS_StartupLocalFile()
{
#ifdef XP_WIN
  CoInitialize(NULL);  // FIX: we should probably move somewhere higher up during startup
#endif
}

void NS_ShutdownLocalFile()
{
#ifndef XPCOM_STANDALONE
  nsFSStringConversion::CleanUp();
#endif /* XPCOM_STANDALONE */

#ifdef XP_WIN
  CoUninitialize();
#endif

}

// Unicode interface Wrapper
NS_IMETHODIMP  
nsLocalFile::InitWithUnicodePath(const PRUnichar *filePath)
{
   SET_UCS(InitWithPath, filePath);
}
NS_IMETHODIMP  
nsLocalFile::AppendUnicode(const PRUnichar *node)
{
   SET_UCS( Append , node);
}
NS_IMETHODIMP  
nsLocalFile::AppendRelativeUnicodePath(const PRUnichar *node)
{
   SET_UCS( AppendRelativePath , node);
}
NS_IMETHODIMP  
nsLocalFile::GetUnicodeLeafName(PRUnichar **aLeafName)
{
   GET_UCS(GetLeafName, aLeafName);
}
NS_IMETHODIMP  
nsLocalFile::SetUnicodeLeafName(const PRUnichar * aLeafName)
{
   SET_UCS( SetLeafName , aLeafName);
}
NS_IMETHODIMP  
nsLocalFile::GetUnicodePath(PRUnichar **_retval)
{
   GET_UCS(GetPath, _retval);
}
NS_IMETHODIMP  
nsLocalFile::CopyToUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( CopyTo , newParentDir, newName);
}
NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinksUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( CopyToFollowingLinks , newParentDir, newName);
}
NS_IMETHODIMP  
nsLocalFile::MoveToUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( MoveTo , newParentDir, newName);
}
NS_IMETHODIMP
nsLocalFile::GetUnicodeTarget(PRUnichar **_retval)
{   
   GET_UCS(GetTarget, _retval);
}
nsresult 
NS_NewUnicodeLocalFile(const PRUnichar* path, PRBool followLinks, nsILocalFile* *result)
{
   SET_UCS_2ARGS_1( NS_NewLocalFile, path, followLinks, result)
}
// should work on Macintosh, Unix, and Win32.
#define kMaxFilenameLength 31  


NS_IMETHODIMP
nsLocalFile::CreateUnique(const char* suggestedName, PRUint32 type, PRUint32 attributes)
{
    nsresult rv = Create(type, attributes);
    
    if (NS_SUCCEEDED(rv)) return NS_OK;
    if (rv != NS_ERROR_FILE_ALREADY_EXISTS) return rv;

    char* leafName; 
    rv = GetLeafName(&leafName);

    if (NS_FAILED(rv)) return rv;

    char* lastDot = strrchr(leafName, '.');
    char suffix[kMaxFilenameLength + 1] = "";
    if (lastDot)
    {
        strncpy(suffix, lastDot, kMaxFilenameLength); // include '.'
        suffix[kMaxFilenameLength] = 0; // make sure it's null terminated
        *lastDot = '\0'; // strip suffix and dot.
    }

    // 27 should work on Macintosh, Unix, and Win32. 
    const int maxRootLength = 27 - nsCRT::strlen(suffix) - 1;

    if ((int)nsCRT::strlen(leafName) > (int)maxRootLength)
        leafName[maxRootLength] = '\0';

    for (short indx = 1; indx < 10000; indx++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
        char newName[kMaxFilenameLength + 1];
        sprintf(newName, "%s-%d%s", leafName, indx, suffix);
        SetLeafName(newName);

        rv = Create(type, attributes);
    
        if (NS_SUCCEEDED(rv) || rv != NS_ERROR_FILE_ALREADY_EXISTS) 
        {
            nsMemory::Free(leafName);
            return rv;
        }
    }
 
    nsMemory::Free(leafName);
    // The disk is full, sort of
    return NS_ERROR_FILE_TOO_BIG;
}

nsresult nsLocalFile::ParseURL(const char* inURL, char **outHost, char **outDirectory,
                               char **outFileBaseName, char **outFileExtension)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

#ifndef XPCOM_STANDALONE
    NS_ENSURE_ARG(inURL);
    NS_ENSURE_ARG_POINTER(outHost);
    *outHost = nsnull;
    NS_ENSURE_ARG_POINTER(outDirectory);
    *outDirectory = nsnull;
    NS_ENSURE_ARG_POINTER(outFileBaseName);
    *outFileBaseName = nsnull;
    NS_ENSURE_ARG_POINTER(outFileExtension);
    *outFileExtension = nsnull;
    
    rv = NS_OK;    
    char* eSpec = nsnull;
    eSpec = nsCRT::strdup(inURL);
    if (!eSpec)
        return NS_ERROR_OUT_OF_MEMORY;

    // Skip leading spaces and control-characters
    char* fwdPtr= (char*) eSpec;
    while (fwdPtr && (*fwdPtr > '\0') && (*fwdPtr <= ' '))
        fwdPtr++;
    // Remove trailing spaces and control-characters
    if (fwdPtr) {
        char* bckPtr= (char*)fwdPtr + PL_strlen(fwdPtr) -1;
        if (*bckPtr > '\0' && *bckPtr <= ' ') {
            while ((bckPtr-fwdPtr) >= 0 && (*bckPtr <= ' ')) {
                bckPtr--;
            }
            *(bckPtr+1) = '\0';
        }
    }

    nsCOMPtr<nsIURLParser> parser(do_GetService(kStdURLParserCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString ePath;
    nsXPIDLCString scheme, username, password, host;
    PRInt32 mPort;
    
    // Parse the spec
    rv = parser->ParseAtScheme(eSpec, getter_Copies(scheme), getter_Copies(username), 
                               getter_Copies(password), outHost, &mPort,
                               getter_Copies(ePath));

    // if this isn't a file: URL, then we can't deal                                            
    if (NS_FAILED(rv) || nsCRT::strcasecmp(scheme, "file") != 0) {
        CRTFREEIF(*outHost);
        return NS_ERROR_FAILURE;
    }
    
    nsXPIDLCString param, query, ref;

    // Now parse the path
    rv = parser->ParseAtDirectory(ePath, outDirectory, outFileBaseName, outFileExtension,
                                  getter_Copies(param), getter_Copies(query), getter_Copies(ref));
    if (NS_FAILED(rv)) {
        CRTFREEIF(*outDirectory);
        CRTFREEIF(*outFileBaseName);
        CRTFREEIF(*outFileExtension);
        return rv;
    }

    // If any of the components are non-NULL but empty, free them
    if (*outHost && !nsCRT::strlen(*outHost))
        CRTFREEIF(*outHost);  
    if (*outDirectory && !nsCRT::strlen(*outDirectory))
        CRTFREEIF(*outDirectory);  
    if (*outFileBaseName && !nsCRT::strlen(*outFileBaseName))
        CRTFREEIF(*outFileBaseName);  
    if (*outFileExtension && !nsCRT::strlen(*outFileExtension))
        CRTFREEIF(*outFileExtension);  
#endif /* XPCOM_STANDALONE */
                                      
    return NS_OK;
}
 

// E_V_I_L Below! E_V_I_L Below! E_V_I_L Below! E_V_I_L Below!


// FTANG need to move this crap out of here.  Mixing nsFileSpec here is
// E_V_I_L

// ==================================================================
//  nsFileSpec stuff . put here untill nsFileSpec get obsoleted     
// ==================================================================

void nsFileSpec::operator = (const nsString& inNativePath)
{
   char* tmp;                                                   
   nsresult res;                                                
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((inNativePath.get()), &tmp))) { 
     *this = tmp;
     nsMemory::Free(tmp);                                    
   }                                                            
   mError = res;
   NS_ASSERTION(NS_SUCCEEDED(res), "nsFileSpec = filed");
}
nsFileSpec nsFileSpec::operator + (const nsString& inRelativeUnixPath) const
{
   nsFileSpec resultSpec;
   char* tmp;                                                   
   nsresult res;                                                
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((inRelativeUnixPath.get()), &tmp))) { 
     resultSpec = *this;
     resultSpec += tmp;
     nsMemory::Free(tmp);                                    
   }                                                            
   NS_ASSERTION(NS_SUCCEEDED(res), "nsFileSpec + filed");
   return resultSpec;
}
void nsFileSpec::operator += (const nsString& inRelativeUnixPath)
{
   char* tmp;                                                   
   nsresult res;                                                
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((inRelativeUnixPath.get()), &tmp))) { 
     *this += tmp;
     nsMemory::Free(tmp);                                    
   }                                                            
   mError = res;
   NS_ASSERTION(NS_SUCCEEDED(res), "nsFileSpec + filed");
}

void nsFileSpec::SetLeafName (const nsString& inLeafName)
{
  VOID_SET_UCS( SetLeafName , inLeafName.get(),"nsFileSpec::SetLeafName failed");
}
void nsFileSpec::MakeUnique(const nsString& inSuggestedLeafName)
{
  VOID_SET_UCS( MakeUnique,inSuggestedLeafName.get(),"nsFileSpec::MakeUnique failed");
}
nsresult nsFileSpec::Rename(const nsString& inNewName)
{
  SET_UCS( Rename , inNewName.get());
}
nsresult nsFileSpec::Execute(const nsString& args) const
{
  SET_UCS( Execute , args.get());
}
