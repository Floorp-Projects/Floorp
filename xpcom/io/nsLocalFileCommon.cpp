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
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"

#ifdef XP_PC
#include "nsLocalFileWin.h"
#endif

#ifdef XP_MAC
#include "nsLocalFileMac.h"
#endif

#ifdef XP_UNIX
#include "nsLocalFileUnix.h"
#endif

#include "nsString.h"
#include "nsCOMPtr.h"

class nsFSStringConversion {
public:
     nsFSStringConversion();
     NS_IMETHOD UCSToNewFS( const PRUnichar* aIn, char** aOut);  
     NS_IMETHOD FSToNewUCS( const char* aIn, PRUnichar** aOut);  

private:
     NS_IMETHOD PrepareFSCharset();
     NS_IMETHOD PrepareEncoder();
     NS_IMETHOD PrepareDecoder();
     nsAutoString mFSCharset;
     nsCOMPtr<nsIUnicodeEncoder> mEncoder;
     nsCOMPtr<nsIUnicodeDecoder> mDecoder;
};


#define GET_UCS( func , arg)                                    \
{                                                               \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = (func)(&tmp))) {                       \
     if(NS_SUCCEEDED(res = gConverter.FSToNewUCS(tmp, (arg)))){ \
       nsAllocator::Free(tmp);                                  \
     }                                                          \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS( func , arg)                                    \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg), &tmp))) { \
     res = (func)(tmp);                                         \
     nsAllocator::Free(tmp);                                    \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_2( func , arg1, arg2)                     \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg2), &tmp))){ \
     res = (func)(arg1, tmp);                                   \
     nsAllocator::Free(tmp);                                    \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_1( func , arg1, arg2)                     \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = gConverter.UCSToNewFS((arg1), &tmp))){ \
     res = (func)(tmp, arg2);                                   \
     nsAllocator::Free(tmp);                                    \
   }                                                            \
   return res;                                                  \
}
nsFSStringConversion gConverter; 

nsFSStringConversion::nsFSStringConversion() : mFSCharset("")
{
   mEncoder = nsnull;
   mDecoder = nsnull;
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareFSCharset()
{
   nsresult res = NS_OK;
   if(mFSCharset.Length() == 0)
   { 
     // lazy eval of the file system charset
     NS_WITH_SERVICE(nsIPlatformCharset, pcharset, NS_PLATFORMCHARSET_PROGID, &res);
     NS_ASSERTION((NS_SUCCEEDED(res) && (nsnull != pcharset)), "cannot get platform charset");
     if(NS_SUCCEEDED(res) && (nsnull != pcharset)) {
        res = pcharset->GetCharset(kPlatformCharsetSel_FileName, mFSCharset);
     } 
   }
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareEncoder()
{
   nsresult res = NS_OK;
   if(nsnull == mEncoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_PROGID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && (nsnull != ucmgr)), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && (nsnull != ucmgr)) 
               res = ucmgr->GetUnicodeEncoder( &mFSCharset, getter_AddRefs(mEncoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && (nsnull != mEncoder)), 
                   "cannot find the unicode encoder");
       }
   }
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::PrepareDecoder()
{
   nsresult res = NS_OK;
   if(nsnull == mDecoder)
   {
       res = PrepareFSCharset();
       if(NS_SUCCEEDED(res)) {
           nsresult res;
           NS_WITH_SERVICE(nsICharsetConverterManager,
                ucmgr, NS_CHARSETCONVERTERMANAGER_PROGID, &res);
           NS_ASSERTION((NS_SUCCEEDED(res) && (nsnull != ucmgr)), 
                   "cannot get charset converter manager ");
           if(NS_SUCCEEDED(res) && (nsnull != ucmgr)) 
               res = ucmgr->GetUnicodeDecoder( &mFSCharset, getter_AddRefs(mDecoder));
           NS_ASSERTION((NS_SUCCEEDED(res) && (nsnull != mDecoder)), 
                   "cannot find the unicode decoder");
       }
   }
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::UCSToNewFS( const PRUnichar* aIn, char** aOut)
{
   nsresult res = PrepareEncoder();
   if(NS_SUCCEEDED(res)) 
   {
        PRInt32 inLength = nsCRT::strlen(aIn);
        PRInt32 outLength;
        res= mEncoder->GetMaxLength(aIn, inLength,&outLength);
        if(NS_SUCCEEDED(res)) {
           *aOut = (char*)nsAllocator::Alloc(outLength+1);
           if(nsnull != aOut) {
              res = mEncoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsAllocator::Free(*aOut);
                 aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
   return res;
}
NS_IMETHODIMP 
nsFSStringConversion::FSToNewUCS( const char* aIn, PRUnichar** aOut)
{
   nsresult res = PrepareDecoder();
   if(NS_SUCCEEDED(res)) 
   {
        PRInt32 inLength = nsCRT::strlen(aIn);
        PRInt32 outLength;
        res= mDecoder->GetMaxLength(aIn, inLength,&outLength);
        if(NS_SUCCEEDED(res)) {
           *aOut = (PRUnichar*)nsAllocator::Alloc(2*(outLength+1));
           if(nsnull != aOut) {
              res = mDecoder->Convert(aIn, &inLength, *aOut,  &outLength);
              if(NS_SUCCEEDED(res)) {
                 (*aOut)[outLength] = '\0';
              } else {
                 nsAllocator::Free(*aOut);
                 aOut = nsnull;
              }
           } else {
              res = NS_ERROR_OUT_OF_MEMORY;
           }
        }
   }
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
NS_NewUnicodeLocalFile(const PRUnichar* path, nsILocalFile* *result)
{
   SET_UCS_2ARGS_1( NS_NewLocalFile , path, result)
}
