/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Mike Pinkerton
 */


//
// Part of the reason these routines are all in once place is so that as new
// data flavors are added that are known to be one-byte or two-byte strings, or even
// raw binary data, then we just have to go to one place to change how the data
// moves into/out of the primitives and native line endings.
//
// If you add new flavors that have special consideration (binary data or one-byte
// char* strings), please update all the helper classes in this file.
//
// For now, this is the assumption that we are making:
//  - text/plain is always a char*
//  - anything else is a PRUnichar*
//


#include "nsPrimitiveHelpers.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIComponentManager.h"
#include "nsLinebreakConverter.h"


//
// CreatePrimitiveForData
//
// Given some data and the flavor it corresponds to, creates the appropriate
// nsISupports* wrapper for passing across IDL boundaries. Right now, everything
// creates a two-byte |nsISupportsWString| unless the flavor is "text/plain", in which
// case it creates a one-byte |nsISupportsString|.
//
void
nsPrimitiveHelpers :: CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, 
                                                 PRUint32 aDataLen, nsISupports** aPrimitive )
{
  if ( !aPrimitive )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> primitive;
    nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                       NS_GET_IID(nsISupportsString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetDataWithLength ( aDataLen, NS_STATIC_CAST(char*, aDataBuff) );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }
  }
  else {
    nsCOMPtr<nsISupportsWString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsWString), getter_AddRefs(primitive));
    if (NS_SUCCEEDED(rv) && primitive ) {
      // recall that SetDataWithLength() takes length as characters, not bytes
      primitive->SetDataWithLength ( aDataLen / 2, NS_STATIC_CAST(PRUnichar*, aDataBuff) );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }  
  }

} // CreatePrimitiveForData


//
// CreateDataFromPrimitive
//
// Given a nsISupports* primitive and the flavor it represents, creates a new data
// buffer with the data in it. This data will be null terminated, but the length
// parameter does not reflect that.
//
void
nsPrimitiveHelpers :: CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, 
                                                   void** aDataBuff, PRUint32 aDataLen )
{
  if ( !aDataBuff )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> plainText ( do_QueryInterface(aPrimitive) );
    if ( plainText )
      plainText->GetData ( NS_REINTERPRET_CAST(char**, aDataBuff) );
  }
  else {
    nsCOMPtr<nsISupportsWString> doubleByteText ( do_QueryInterface(aPrimitive) );
    if ( doubleByteText )
      doubleByteText->GetData ( NS_REINTERPRET_CAST(unsigned short**, aDataBuff) );
  }

}


#ifdef XP_MAC
#pragma mark -
#endif


//
// ConvertPlatformToDOMLinebreaks
//
// Given some data, convert from the platform linebreaks into the LF expected by the
// DOM. This will attempt to convert the data in place, but the buffer may still need to
// be reallocated regardless (disposing the old buffer is taken care of internally, see
// the note below).
//
// NOTE: this assumes that it can use nsAllocator to dispose of the old buffer.
//
nsresult
nsLinebreakHelpers :: ConvertPlatformToDOMLinebreaks ( const char* inFlavor, void** ioData, 
                                                          PRInt32* ioLengthInBytes )
{
  NS_ASSERTION ( ioData && *ioData && ioLengthInBytes, "Bad Params");
  if ( !(ioData && *ioData && ioLengthInBytes) )
    return NS_ERROR_INVALID_ARG;
    
  nsresult retVal = NS_OK;
  
  if ( strcmp(inFlavor, "text/plain") == 0 ) {
    char* buffAsChars = NS_REINTERPRET_CAST(char*, *ioData);
    char* oldBuffer = buffAsChars;
    retVal = nsLinebreakConverter::ConvertLineBreaksInSitu ( &buffAsChars, nsLinebreakConverter::eLinebreakPlatform, 
                                                              nsLinebreakConverter::eLinebreakContent, 
                                                              *ioLengthInBytes, ioLengthInBytes );
    if ( NS_SUCCEEDED(retVal) ) {
      if ( buffAsChars != oldBuffer )             // check if buffer was reallocated
        nsAllocator::Free ( oldBuffer );
      *ioData = buffAsChars;
    }
  }
  else if ( strcmp(inFlavor, "image/jpeg") == 0 ) {
    // I'd assume we don't want to do anything for binary data....
  }
  else {       
    PRUnichar* buffAsUnichar = NS_REINTERPRET_CAST(PRUnichar*, *ioData);
    PRUnichar* oldBuffer = buffAsUnichar;
    PRInt32 newLengthInChars;
    retVal = nsLinebreakConverter::ConvertUnicharLineBreaksInSitu ( &buffAsUnichar, nsLinebreakConverter::eLinebreakPlatform, 
                                                                     nsLinebreakConverter::eLinebreakContent, 
                                                                     *ioLengthInBytes / sizeof(PRUnichar), &newLengthInChars );
    if ( NS_SUCCEEDED(retVal) ) {
      if ( buffAsUnichar != oldBuffer )           // check if buffer was reallocated
        nsAllocator::Free ( oldBuffer );
      *ioData = buffAsUnichar;
      *ioLengthInBytes = newLengthInChars * sizeof(PRUnichar);
    }
  }
  
  return retVal;

} // ConvertPlatformToDOMLinebreaks
