/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Mike Pinkerton
// Netscape Communications
//
// See header file for details.
//
// Remaining Work:
// * only convert data to clipboard on an app switch.
//

#include "nsCOMPtr.h"
#include "nsClipboard.h"

#include "nsVoidArray.h"
#include "nsIClipboardOwner.h"
#include "nsString.h"
#include "nsIFormatConverter.h"
#include "nsMimeMapper.h"

#include "nsIComponentManager.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsMemory.h"

#include <Scrap.h>


//
// nsClipboard constructor
//
nsClipboard::nsClipboard() : nsBaseClipboard()
{
}

//
// nsClipboard destructor
//
nsClipboard::~nsClipboard()
{
}


//
// SetNativeClipboardData
//
// Take data off the transferrable and put it on the clipboard in as many formats
// as are registered.
//
// NOTE: This code could all live in ForceDataToClipboard() and this could be a NOOP. 
// If speed and large data sizes are an issue, we should move that code there and only
// do it on an app switch.
//
NS_IMETHODIMP
nsClipboard :: SetNativeClipboardData ( PRInt32 aWhichClipboard )
{
  if ( aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  nsresult errCode = NS_OK;
  
  mIgnoreEmptyNotification = PR_TRUE;

  // make sure we have a good transferable
  if ( !mTransferable )
    return NS_ERROR_INVALID_ARG;
  
  nsMimeMapperMac theMapper;

#if TARGET_CARBON
  ::ClearCurrentScrap();
#else
  ::ZeroScrap();
#endif

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  errCode = mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  // For each flavor present (either directly in the transferable or that its
  // converter knows about) put it on the clipboard. Luckily, GetTransferData() 
  // handles conversions for us, so we really don't need to know if a conversion
  // is required or not.
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for ( PRUint32 i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString( getter_Copies(flavorStr) );
      
      // find MacOS flavor
      ResType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr);
    
      // get data. This takes converters into account.
      void* data = nsnull;
      PRUint32 dataSize = 0;
      nsCOMPtr<nsISupports> genericDataWrapper;
      errCode = mTransferable->GetTransferData ( flavorStr, getter_AddRefs(genericDataWrapper), &dataSize );
      nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataSize );
#ifdef NS_DEBUG
        if ( NS_FAILED(errCode) ) printf("nsClipboard:: Error getting data from transferable\n");
#endif
      
      // stash on clipboard
#if TARGET_CARBON
      ScrapRef scrap;
      ::GetCurrentScrap(&scrap);
      ::PutScrapFlavor(scrap, macOSFlavor, 0L/*???*/, dataSize, data);
#else
      long numBytes = ::PutScrap ( dataSize, macOSFlavor, data );
      if ( numBytes != noErr )
        errCode = NS_ERROR_FAILURE;
#endif
        
      // if the flavor was unicode, then we also need to put it on as 'TEXT' after
      // doing the conversion to the platform charset.
      if ( strcmp(flavorStr,kUnicodeMime) == 0 ) {
        char* plainTextData = nsnull;
        PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, data);
        PRInt32 plainTextLen = 0;
        nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, dataSize / 2, &plainTextData, &plainTextLen );
        if ( plainTextData ) {
#if TARGET_CARBON
          ScrapRef scrap;
          ::GetCurrentScrap(&scrap);
          ::PutScrapFlavor( scrap, 'TEXT', 0L/*???*/, plainTextLen, plainTextData );
#else
          long numTextBytes = ::PutScrap ( plainTextLen, 'TEXT', plainTextData );
          if ( numTextBytes != noErr )
            errCode = NS_ERROR_FAILURE;
#endif
          nsMemory::Free ( plainTextData ); 
        }      
      } // if unicode
        
      nsMemory::Free ( data );
    }
  } // foreach flavor in transferable

  // write out the mapping data in a special flavor on the clipboard. |mappingLen|
  // includes the NULL terminator.
  short mappingLen = 0;
  const char* mapping = theMapper.ExportMapping(&mappingLen);
  if ( mapping && mappingLen ) {
#if TARGET_CARBON
    ScrapRef scrap;
    ::GetCurrentScrap(&scrap);
    ::PutScrapFlavor(scrap, nsMimeMapperMac::MappingFlavor(), 0L/*???*/, mappingLen, mapping);
#else
    long numBytes = ::PutScrap ( mappingLen, nsMimeMapperMac::MappingFlavor(), mapping );
    if ( numBytes != noErr )
      errCode = NS_ERROR_FAILURE;
#endif
    nsCRT::free ( NS_CONST_CAST(char*, mapping) );
  }
  
  return errCode;
  
} // SetNativeClipboardData


//
// GetNativeClipboardData
//
// Take data off the native clip and put it on the transferable.
//
NS_IMETHODIMP
nsClipboard :: GetNativeClipboardData ( nsITransferable * aTransferable, PRInt32 aWhichClipboard )
{
  if ( aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  nsresult errCode = NS_OK;

  // make sure we have a good transferable
  if ( !aTransferable )
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  // create a mime mapper. It's ok for this to fail because the data may come from
  // another app which obviously wouldn't put our mime mapping data on the clipboard.
  char* mimeMapperData = nsnull;
  errCode = GetDataOffClipboard ( nsMimeMapperMac::MappingFlavor(), (void**)&mimeMapperData, 0 );
  nsMimeMapperMac theMapper ( mimeMapperData );
  if (mimeMapperData)
    nsCRT::free ( mimeMapperData );
 
  // Now walk down the list of flavors. When we find one that is actually on the
  // clipboard, copy out the data into the transferable in that format. SetTransferData()
  // implicitly handles conversions.
  PRBool dataFound = PR_FALSE;
  PRUint32 cnt;
  flavorList->Count(&cnt);
  for ( PRUint32 i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString ( getter_Copies(flavorStr) );
      
      // find MacOS flavor (don't add if not present)
      ResType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr, PR_FALSE);
    
      void* clipboardData = nsnull;
      PRInt32 dataSize = 0L;
      nsresult loadResult = GetDataOffClipboard ( macOSFlavor, &clipboardData, &dataSize );
      if ( NS_SUCCEEDED(loadResult) && clipboardData )
        dataFound = PR_TRUE;
      else {
        // if we are looking for text/unicode and we fail to find it on the clipboard first,
        // try again with text/plain. If that is present, convert it to unicode.
        if ( strcmp(flavorStr, kUnicodeMime) == 0 ) {
          loadResult = GetDataOffClipboard ( 'TEXT', &clipboardData, &dataSize );
          if ( NS_SUCCEEDED(loadResult) && clipboardData ) {
            const char* castedText = NS_REINTERPRET_CAST(char*, clipboardData);          
            PRUnichar* convertedText = nsnull;
            PRInt32 convertedTextLen = 0;
            nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, dataSize, 
                                                                      &convertedText, &convertedTextLen );
            if ( convertedText ) {
              // out with the old, in with the new 
              nsMemory::Free(clipboardData);
              clipboardData = convertedText;
              dataSize = convertedTextLen * 2;
              dataFound = PR_TRUE;
            }
          } // if plain text data on clipboard
        } // if looking for text/unicode   
      } // else we try one last ditch effort to find our data
      
      if ( dataFound ) {       
        // the DOM only wants LF, so convert from MacOS line endings to DOM line
        // endings.
        nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr, &clipboardData, &dataSize );
        
        // put it into the transferable
        nsCOMPtr<nsISupports> genericDataWrapper;
        nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, clipboardData, dataSize, getter_AddRefs(genericDataWrapper) );        
        errCode = aTransferable->SetTransferData ( flavorStr, genericDataWrapper, dataSize );
#ifdef NS_DEBUG
          if ( errCode != NS_OK ) printf("nsClipboard:: Error setting data into transferable\n");
#endif
        nsMemory::Free ( clipboardData );
        
        // we found one, get out of this loop!
        break;        
      } // if flavor found on clipboard
    }
  } // foreach flavor
  
  return errCode;
}


//
// GetDataOffClipboard
//
// 
nsresult
nsClipboard :: GetDataOffClipboard ( ResType inMacFlavor, void** outData, PRInt32* outDataSize )
{
  if ( !outData || !inMacFlavor )
    return NS_ERROR_FAILURE;

  // set up default results.
  *outData = nsnull;
  if ( outDataSize )
      *outDataSize = 0;

  // check if it is on the clipboard
  long offsetUnused = 0;

#if TARGET_CARBON
  ScrapRef scrap;
  long dataSize;
  OSStatus err;

  err = ::GetCurrentScrap(&scrap);
  if (err != noErr) return NS_ERROR_FAILURE;
  err = ::GetScrapFlavorSize(scrap, inMacFlavor, &dataSize);
  if (err != noErr) return NS_ERROR_FAILURE;
  // check err??
  if (dataSize > 0) {
    char* dataBuff = (char*) nsMemory::Alloc(dataSize);
    if ( !dataBuff )
      return NS_ERROR_OUT_OF_MEMORY;
    err = ::GetScrapFlavorData(scrap, inMacFlavor, &dataSize, dataBuff);
    if (err != noErr) return NS_ERROR_FAILURE;

    // put it into the transferable
#ifdef NS_DEBUG
    if ( err != NS_OK ) printf("nsClipboard:: Error setting data into transferable\n");
#endif

    if ( outDataSize )
      *outDataSize = dataSize;
    *outData = dataBuff;
  }
#else
  long clipResult = ::GetScrap(NULL, inMacFlavor, &offsetUnused);
  if ( clipResult > 0 ) {
    Handle dataHand = ::NewHandle(0);
    if ( !dataHand )
      return NS_ERROR_OUT_OF_MEMORY;
    long dataSize = ::GetScrap ( dataHand, inMacFlavor, &offsetUnused );
    if ( dataSize > 0 ) {
      char* dataBuff = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(dataSize));
      if ( !dataBuff )
        return NS_ERROR_OUT_OF_MEMORY;
      ::HLock(dataHand);
      ::BlockMoveData ( *dataHand, dataBuff, dataSize );
      ::HUnlock(dataHand);
      
      ::DisposeHandle(dataHand);
      
      if ( outDataSize )
        *outDataSize = dataSize;
      *outData = dataBuff;
    } 
    else {
#ifdef NS_DEBUG
         printf("nsClipboard: Error getting data off the clipboard, #%d\n", dataSize);
#endif
       return NS_ERROR_FAILURE;
    }
  }
#endif /* TARGET_CARBON */
  return NS_OK;
  
} // GetDataOffClipboard


//
// HasDataMatchingFlavors
//
// Check the clipboard to see if we have any data that matches the given flavors. This
// does NOT actually fetch the data. The items in the flavor list are nsISupportsString's.
// 
// Handle the case where we ask for unicode and it's not there, but plain text is. We 
// say "yes" in that case, knowing that we will have to perform a conversion when we actually
// pull the data off the clipboard.
//
NS_IMETHODIMP
nsClipboard :: HasDataMatchingFlavors ( nsISupportsArray* aFlavorList, PRInt32 aWhichClipboard, PRBool * outResult ) 
{
  nsresult rv = NS_OK;
  *outResult = PR_FALSE;  // assume there is nothing there we want.
  if ( aWhichClipboard != kGlobalClipboard )
    return NS_OK;
  
  // create a mime mapper. It's ok for this to fail because the data may come from
  // another app which obviously wouldn't put our mime mapping data on the clipboard.
  char* mimeMapperData = nsnull;
  rv = GetDataOffClipboard ( nsMimeMapperMac::MappingFlavor(), (void**)&mimeMapperData, 0 );
  nsMimeMapperMac theMapper ( mimeMapperData );
  nsMemory::Free ( mimeMapperData );
  
  PRUint32 length;
  aFlavorList->Count(&length);
  for ( PRUint32 i = 0; i < length; ++i ) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsString> flavorWrapper ( do_QueryInterface(genericFlavor) );
    if ( flavorWrapper ) {
      nsXPIDLCString flavor;
      flavorWrapper->ToString ( getter_Copies(flavor) );
      
#ifdef NS_DEBUG
      if ( strcmp(flavor, kTextMime) == 0 )
        NS_WARNING ( "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD" );
#endif

      // now that we have the flavor (whew!), run it through the mime mapper. If
      // the mapper returns a null flavor, then it ain't there.
      ResType macFlavor = theMapper.MapMimeTypeToMacOSType ( flavor, PR_FALSE );
      if ( macFlavor ) {
        if ( CheckIfFlavorPresent(macFlavor) )   
          *outResult = PR_TRUE;   // we found one!
        break;
      }
      else {
        // if the client asked for unicode and it wasn't present, check if we have TEXT.
        // We'll handle the actual data substitution in GetNativeClipboardData().
        if ( strcmp(flavor, kUnicodeMime) == 0 ) {
          if ( CheckIfFlavorPresent('TEXT') )
            *outResult = PR_TRUE;
        }
      }
    }  
  } // foreach flavor
  
  return NS_OK;
}


//
// CheckIfFlavorPresent
//
// A little utility routine for derminining if a given flavor is really there
//
PRBool
nsClipboard :: CheckIfFlavorPresent ( ResType inMacFlavor )
{
  PRBool retval = PR_FALSE;
  long offsetUnused = 0;

#if TARGET_CARBON
  ScrapRef scrap;
  long dataSize;
  OSStatus err;

  err = ::GetCurrentScrap(&scrap);
  if (err != noErr) return NS_ERROR_FAILURE;
  err = ::GetScrapFlavorSize(scrap, inMacFlavor, &dataSize);
  // XXX ?
  if ( dataSize > 0 )
    retval = PR_TRUE;
#else
  long clipResult = ::GetScrap(NULL, inMacFlavor, &offsetUnused);
  if ( clipResult > 0 )   
    retval = PR_TRUE;   // we found one!
#endif

  return retval;
} // CheckIfFlavorPresent
