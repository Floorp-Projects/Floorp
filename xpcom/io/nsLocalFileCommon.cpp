/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIServiceManager.h"
#ifndef XPCOM_STANDALONE
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#endif /* XPCOM_STANDALONE */

#include "nsFileSpec.h"  // evil ftang hack

#ifdef XP_PC
#ifdef XP_OS2
#include "nsLocalFileOS2.h"
#else
#include "nsLocalFileWin.h"
#endif
#endif

#ifdef XP_MAC
#include "nsLocalFileMac.h"
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
#include "nsLocalFileUnix.h"
#endif

#include "nsString.h"
#include "nsCOMPtr.h"

class nsFSStringConversion {
public:
     nsFSStringConversion();
     virtual ~nsFSStringConversion(){};
     NS_IMETHOD UCSToNewFS( const PRUnichar* aIn, char** aOut);  
     NS_IMETHOD FSToNewUCS( const char* aIn, PRUnichar** aOut);  

private:
     NS_IMETHOD PrepareFSCharset();
     NS_IMETHOD PrepareEncoder();
     NS_IMETHOD PrepareDecoder();
     nsAutoString mFSCharset;
#ifndef XPCOM_STANDALONE
     nsCOMPtr<nsIUnicodeEncoder> mEncoder;
     nsCOMPtr<nsIUnicodeDecoder> mDecoder;
#endif /* XPCOM_STANDALONE */
};


#define GET_UCS( func , arg)                                    \
{                                                               \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = (func)(&tmp))) {                       \
     if(NS_SUCCEEDED(res = gConverter.FSToNewUCS(tmp, (arg)))){ \
       nsMemory::Free(tmp);                                     \
     }                                                          \
   }                                                            \
   return res;                                                  \
}
#define VOID_SET_UCS( func , arg, assertion_msg)                \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg), &tmp))) { \
     (func)(tmp);                                               \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   NS_ASSERTION(NS_SUCCEEDED(res), assertion_msg);              \
}
#define SET_UCS( func , arg)                                    \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg), &tmp))) { \
     res = (func)(tmp);                                         \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_2( func , arg1, arg2)                     \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg2), &tmp))){ \
     res = (func)(arg1, tmp);                                   \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_1( func , path, followLinks, result)      \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((path), &tmp))){ \
     res = (func)(tmp, followLinks, result);                    \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
nsFSStringConversion gConverter; 

nsFSStringConversion::nsFSStringConversion()
{
#ifndef XPCOM_STANDALONE
   mEncoder = nsnull;
   mDecoder = nsnull;
#endif /* XPCOM_STANDALONE */
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareFSCharset()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(mFSCharset.Length() == 0)
   { 
     // lazy eval of the file system charset
     NS_WITH_SERVICE(nsIPlatformCharset, pcharset, NS_PLATFORMCHARSET_PROGID, &res);
     if (!(NS_SUCCEEDED(res) && pcharset)) {
       NS_WARNING("cannot get platform charset");
     }
     if(NS_SUCCEEDED(res) && pcharset) {
        res = pcharset->GetCharset(kPlatformCharsetSel_FileName, mFSCharset);
     } 
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareEncoder()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(! mEncoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_PROGID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeEncoder( &mFSCharset, getter_AddRefs(mEncoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && mEncoder), 
                   "cannot find the unicode encoder");
       }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareDecoder()
{
   nsresult res = NS_ERROR_NOT_IMPLEMENTED;
#ifndef XPCOM_STANDALONE
   res = NS_OK;
   if(! mDecoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_PROGID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && ucmgr), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && ucmgr) 
               res = ucmgr->GetUnicodeDecoder( &mFSCharset, getter_AddRefs(mDecoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && mDecoder), 
                   "cannot find the unicode decoder");
       }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
NS_IMETHODIMP 
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
           if(nsnull != aOut) {
              res = mEncoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsMemory::Free(*aOut);
                 aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
#endif /* XPCOM_STANDALONE */
   return res;
}
NS_IMETHODIMP 
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
           if(nsnull != aOut) {
              res = mDecoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsMemory::Free(*aOut);
                 aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
#endif /* XPCOM_STANDALONE */
   return res;
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
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((inNativePath.GetUnicode()), &tmp))) { 
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
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((inRelativeUnixPath.GetUnicode()), &tmp))) { 
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
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((inRelativeUnixPath.GetUnicode()), &tmp))) { 
     *this += tmp;
     nsMemory::Free(tmp);                                    
   }                                                            
   mError = res;
   NS_ASSERTION(NS_SUCCEEDED(res), "nsFileSpec + filed");
}

void nsFileSpec::SetLeafName (const nsString& inLeafName)
{
  VOID_SET_UCS( SetLeafName , inLeafName.GetUnicode(),"nsFileSpec::SetLeafName failed");
}
void nsFileSpec::MakeUnique(const nsString& inSuggestedLeafName)
{
  VOID_SET_UCS( MakeUnique,inSuggestedLeafName.GetUnicode(),"nsFileSpec::MakeUnique failed");
}
nsresult nsFileSpec::Rename(const nsString& inNewName)
{
  SET_UCS( Rename , inNewName.GetUnicode());
}
nsresult nsFileSpec::Execute(const nsString& args) const
{
  SET_UCS( Execute , args.GetUnicode());
}
