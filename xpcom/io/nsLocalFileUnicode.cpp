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
 
// =========================================================================
//  This file provides implementations of the Unicode versions of nsIFile
//  methods and nsFileSpec. It converts between Unicode and the FS 
//  charset using uconv. As implementations of nsLocalFile implement these 
//  methods themselves using OS facilities, this file should go away. It
//  is here only during that transition.
// =========================================================================

#include "nsIServiceManager.h"
#ifndef XPCOM_STANDALONE
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#endif /* XPCOM_STANDALONE */

#include "nsLocalFile.h" // includes platform-specific headers
#include "nsLocalFileUnicode.h"
#include "nsFileSpec.h"

class nsFSStringConversion {
public:
     static void CleanUp();
     static nsresult UCSToNewFS( const PRUnichar* aIn, char** aOut);  
     static nsresult FSToNewUCS( const char* aIn, PRUnichar** aOut);  
     static const nsAString *FSCharset();

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

void NS_ShutdownLocalFileUnicode()
{
#ifndef XPCOM_STANDALONE
  nsFSStringConversion::CleanUp();
#endif /* XPCOM_STANDALONE */
}

#define GET_UCS( func , arg)                                    \
{                                                               \
   nsCAutoString tmp;                                           \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = (func)(tmp)))                          \
     res = nsFSStringConversion::FSToNewUCS(tmp.get(), (arg));  \
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
     res = (func)(nsDependentCString(tmp));                     \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_2( func , arg1, arg2)                     \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((arg2), &tmp))){ \
     res = (func)(arg1, nsDependentCString(tmp));                                   \
     nsMemory::Free(tmp);                                       \
   }                                                            \
   return res;                                                  \
}
#define SET_UCS_2ARGS_1( func , path, followLinks, result)      \
 {                                                              \
   char* tmp;                                                   \
   nsresult res;                                                \
   if(NS_SUCCEEDED(res = nsFSStringConversion::UCSToNewFS((path), &tmp))){ \
     res = (func)(nsDependentCString(tmp), followLinks, result);                    \
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
        // Need to clear up the buffer because a encoder returning
        // NS_OK_UDEC_MOREINPUT for an unconvertable aIn lead a new
        // file name in aIn to be _appended_ to the end of the encoder buffer.
        // (bug 133216 and bug 131161)
        res= mEncoder->Reset(); 
        if(NS_FAILED(res)) 
           return res;
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
        PRInt32 inLength = strlen(aIn);
        PRInt32 outLength;
        // Need to clear up the buffer because a decoder returning 
        // NS_OK_UDEC_MOREINPUT for an unconvertable aIn lead a new 
        // file name in aIn to be _appended_ to the end of the decoder buffer.  
        // (bug 133216 and bug 131161)
        res= mDecoder->Reset(); 
        if(NS_FAILED(res)) 
           return res;
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

const nsAString *
nsFSStringConversion::FSCharset()
{
#ifndef XPCOM_STANDALONE
    nsresult res = PrepareFSCharset();
    if(NS_SUCCEEDED(res))
        return mFSCharset;
#endif
    return nsnull;
}

PRBool
nsLocalFile::FSCharsetIsUTF8()
{
    // assuming charset is normalized uppercase
    return nsFSStringConversion::FSCharset() &&
           nsFSStringConversion::FSCharset()->Equals(NS_LITERAL_STRING("UTF-8"));
}

// Unicode interface Wrapper
NS_IMETHODIMP  
nsLocalFile::InitWithUnicodePath(const PRUnichar *filePath)
{
   SET_UCS(InitWithNativePath, filePath);
}
NS_IMETHODIMP  
nsLocalFile::AppendUnicode(const PRUnichar *node)
{
   SET_UCS( AppendNative , node);
}
NS_IMETHODIMP  
nsLocalFile::AppendRelativeUnicodePath(const PRUnichar *node)
{
   SET_UCS( AppendRelativeNativePath , node);
}
NS_IMETHODIMP  
nsLocalFile::GetUnicodeLeafName(PRUnichar **aLeafName)
{
   GET_UCS(GetNativeLeafName, aLeafName);
}
NS_IMETHODIMP  
nsLocalFile::SetUnicodeLeafName(const PRUnichar * aLeafName)
{
   SET_UCS( SetNativeLeafName , aLeafName);
}
NS_IMETHODIMP  
nsLocalFile::GetUnicodePath(PRUnichar **_retval)
{
   GET_UCS(GetNativePath, _retval);
}
NS_IMETHODIMP  
nsLocalFile::CopyToUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( CopyToNative , newParentDir, newName);
}
NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinksUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( CopyToFollowingLinksNative , newParentDir, newName);
}
NS_IMETHODIMP  
nsLocalFile::MoveToUnicode(nsIFile *newParentDir, const PRUnichar *newName)
{
   SET_UCS_2ARGS_2( MoveToNative , newParentDir, newName);
}
NS_IMETHODIMP
nsLocalFile::GetUnicodeTarget(PRUnichar **_retval)
{   
   GET_UCS(GetNativeTarget, _retval);
}
nsresult 
NS_NewUnicodeLocalFile(const PRUnichar* path, PRBool followLinks, nsILocalFile* *result)
{
   SET_UCS_2ARGS_1( NS_NewNativeLocalFile, path, followLinks, result)
}
 
