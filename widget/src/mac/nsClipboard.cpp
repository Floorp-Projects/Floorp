/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsIImageMac.h"
#include "nsMemory.h"
#include "nsMacNativeUnicodeConverter.h"
#include "nsICharsetConverterManager.h"
#include "nsCRT.h"
#include "nsStylClipboardUtils.h"
#include "nsLinebreakConverter.h"

#include <Scrap.h>
#include <Script.h>
#include <TextEdit.h>



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
// NOTE: This code could all live in a shutdown observer and this could be a NOOP. 
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
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString( getter_Copies(flavorStr) );
      
      // find MacOS flavor
      ResType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr);
    
      // get data. This takes converters into account. Different things happen for
      // different flavors, so do some special processing.
      void* data = nsnull;
      PRUint32 dataSize = 0;
      if ( strcmp(flavorStr,kUnicodeMime) == 0 ) {
        // we have unicode, put it on first as unicode
        nsCOMPtr<nsISupports> genericDataWrapper;
        errCode = mTransferable->GetTransferData ( flavorStr, getter_AddRefs(genericDataWrapper), &dataSize );
        nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataSize );

        // Convert unix to mac linebreaks, since mac linebreaks are required for clipboard compatibility.
        // I'm making the assumption here that the substitution will be entirely in-place, since both
        // types of line breaks are 1-byte.

        PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, data);
        nsLinebreakConverter::ConvertUnicharLineBreaksInSitu(&castedUnicode,
                                                             nsLinebreakConverter::eLinebreakUnix,
                                                             nsLinebreakConverter::eLinebreakMac,
                                                             dataSize / sizeof(PRUnichar), nsnull);
          
        errCode = PutOnClipboard ( macOSFlavor, data, dataSize );
        if ( NS_SUCCEEDED(errCode) ) {
          // we also need to put it on as 'TEXT' after doing the conversion to the platform charset.
          char* plainTextData = nsnull;
          PRInt32 plainTextLen = 0;
          errCode = nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, dataSize / 2, &plainTextData, &plainTextLen );
          
          ScriptCodeRun *scriptCodeRuns = nsnull;
          PRInt32 scriptRunOutLen;
          
          // if characters are not mapped from Unicode then try native API to convert to 
          // available script
          if (errCode == NS_ERROR_UENC_NOMAPPING) {
            if (plainTextData) {
              nsMemory::Free(plainTextData);
              plainTextData = nsnull;
            }
            errCode = nsMacNativeUnicodeConverter::ConvertUnicodetoScript(castedUnicode, 
                                                                          dataSize / 2,
                                                                          &plainTextData, 
                                                                          &plainTextLen,
                                                                          &scriptCodeRuns,
                                                                          &scriptRunOutLen);
          }
          else if (NS_SUCCEEDED(errCode)) {
            // create a single run with the default system script
            scriptCodeRuns = NS_REINTERPRET_CAST(ScriptCodeRun*,
                                                 nsMemory::Alloc(sizeof(ScriptCodeRun)));
            if (scriptCodeRuns) {
              scriptCodeRuns[0].offset = 0;
              scriptCodeRuns[0].script = (ScriptCode) ::GetScriptManagerVariable(smSysScript);
              scriptRunOutLen = 1;
            }
          }
          
          if ( NS_SUCCEEDED(errCode) && plainTextData ) {
            errCode = PutOnClipboard ( 'TEXT', plainTextData, plainTextLen );
            nsMemory::Free ( plainTextData ); 
            
            // create 'styl' from the script runs
            if (NS_SUCCEEDED(errCode) && scriptCodeRuns) {
              char *stylData;
              PRInt32 stylLen;
              errCode = CreateStylFromScriptRuns(scriptCodeRuns,
                                                 scriptRunOutLen,
                                                 &stylData,
                                                 &stylLen);
              if (NS_SUCCEEDED(errCode)) {
                errCode = PutOnClipboard ('styl', stylData, stylLen);
                nsMemory::Free(stylData);
              }
            }
          }
          if (scriptCodeRuns)
            nsMemory::Free(scriptCodeRuns);
        }
      } // if unicode
      else if ( strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
                strcmp(flavorStr, kGIFImageMime) == 0 || strcmp(flavorStr, kNativeImageMime) == 0 ) {
        // we have an image, which is in the transferable as an nsIImage. Convert it
        // to PICT (PicHandle) and put those bits on the clipboard. The actual size
        // of the picture is the size of the handle, not sizeof(Picture).
        nsCOMPtr<nsISupports> transferSupports;
        errCode = mTransferable->GetTransferData ( flavorStr, getter_AddRefs(transferSupports), &dataSize );
        nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive(do_QueryInterface(transferSupports));
        nsCOMPtr<nsIImageMac> image;
        if (ptrPrimitive) {
          nsCOMPtr<nsISupports> primitiveData;
          ptrPrimitive->GetData(getter_AddRefs(primitiveData));
          image = do_QueryInterface(primitiveData);
        }
        if ( image ) {
          PicHandle picture = nsnull;
          image->ConvertToPICT ( &picture );
          if ( picture ) {
            errCode = PutOnClipboard ( 'PICT', *picture, ::GetHandleSize((Handle)picture) );
            ::KillPicture ( picture );
          }
        }
        else
          NS_WARNING ( "Image isn't an nsIImageMac in transferable" );
      }
      else {
        // we don't know what we have. let's just assume it's unicode but doesn't need to be
        // translated to TEXT.
        nsCOMPtr<nsISupports> genericDataWrapper;
        errCode = mTransferable->GetTransferData ( flavorStr, getter_AddRefs(genericDataWrapper), &dataSize );
        nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataSize );
        errCode = PutOnClipboard ( macOSFlavor, data, dataSize );
      }
              
      nsMemory::Free ( data );
    }
  } // foreach flavor in transferable

  // write out the mapping data in a special flavor on the clipboard. |mappingLen|
  // includes the NULL terminator.
  short mappingLen = 0;
  const char* mapping = theMapper.ExportMapping(&mappingLen);
  if ( mapping && mappingLen ) {
    errCode = PutOnClipboard ( nsMimeMapperMac::MappingFlavor(), mapping, mappingLen );
    nsCRT::free ( NS_CONST_CAST(char*, mapping) );
  }
  
  return errCode;
  
} // SetNativeClipboardData


//
// PutOnClipboard
//
// Actually does the work of placing the data on the native clipboard 
// in the given flavor
//
nsresult
nsClipboard :: PutOnClipboard ( ResType inFlavor, const void* inData, PRInt32 inLen )
{
  nsresult errCode = NS_OK;
  
#if TARGET_CARBON
  ScrapRef scrap;
  ::GetCurrentScrap(&scrap);
  ::PutScrapFlavor( scrap, inFlavor, kScrapFlavorMaskNone, inLen, inData );
#else
  long numBytes = ::PutScrap ( inLen, inFlavor, inData );
  if ( numBytes != noErr )
    errCode = NS_ERROR_FAILURE;
#endif

  return errCode;
  
} // PutOnClipboard


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
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
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
        
          // if 'styl' is available, we can get a script of the first run
          // and use it for converting 'TEXT'
          loadResult = GetDataOffClipboard ( 'styl', &clipboardData, &dataSize );
          if (NS_SUCCEEDED(loadResult) && 
              clipboardData &&
              ((PRUint32)dataSize >= (sizeof(ScrpSTElement) + 2))) {
            StScrpRec *scrpRecP = (StScrpRec *) clipboardData;
            ScrpSTElement *styl = scrpRecP->scrpStyleTab;
            ScriptCode script = styl ? ::FontToScript(styl->scrpFont) : smCurrentScript;
            
            // free 'styl' and get 'TEXT'
            nsMemory::Free(clipboardData);
            loadResult = GetDataOffClipboard ( 'TEXT', &clipboardData, &dataSize );
            if ( NS_SUCCEEDED(loadResult) && clipboardData ) {
              PRUnichar* convertedText = nsnull;
              PRInt32 convertedTextLen = 0;
              errCode = nsMacNativeUnicodeConverter::ConvertScripttoUnicode(
                                                                    script, 
                                                                    (const char *) clipboardData,
                                                                    dataSize,
                                                                    &convertedText,
                                                                    &convertedTextLen);
              if (NS_SUCCEEDED(errCode) && convertedText) {
                nsMemory::Free(clipboardData);
                clipboardData = convertedText;
                dataSize = convertedTextLen * sizeof(PRUnichar);
                dataFound = PR_TRUE;
              }
            }
          }          
          
          if (!dataFound) {
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
          }
        } // if looking for text/unicode   
      } // else we try one last ditch effort to find our data
      
      if ( dataFound ) {
        if ( strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
               strcmp(flavorStr, kGIFImageMime) == 0 ) {
          // someone asked for an image, so we have to convert from PICT to the desired
          // image format
          
          #ifdef DEBUG
          printf ( "----------- IMAGE REQUESTED ----------" );
          #endif
               
        } // if image requested
        else {
          // Assume that we have some form of textual data. The DOM only wants LF, so convert
          // from MacOS line endings to DOM line endings.
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks ( flavorStr, &clipboardData, &dataSize );
          
          unsigned char *clipboardDataPtr = (unsigned char *) clipboardData;
#if TARGET_CARBON
          // skip BOM (Byte Order Mark to distinguish little or big endian) in 'utxt'
          // 10.2 puts BOM for 'utxt', we need to remove it here
          // for little endian case, we also need to convert the data to big endian
          // but we do not do that currently (need this in case 'utxt' is really in little endian)
          if ( (macOSFlavor == 'utxt') &&
               (dataSize > 2) &&
               ((clipboardDataPtr[0] == 0xFE && clipboardDataPtr[1] == 0xFF) ||
               (clipboardDataPtr[0] == 0xFF && clipboardDataPtr[1] == 0xFE)) ) {
            dataSize -= sizeof(PRUnichar);
            clipboardDataPtr += sizeof(PRUnichar);
          }
#endif

          // put it into the transferable
          nsCOMPtr<nsISupports> genericDataWrapper;
          nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, clipboardDataPtr, dataSize, getter_AddRefs(genericDataWrapper) );        
          errCode = aTransferable->SetTransferData ( flavorStr, genericDataWrapper, dataSize );
        }
        
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
// Actually does the work of retrieving the data from the native clipboard 
// with the given MacOS flavor
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


#if TARGET_CARBON
  ScrapRef scrap;
  long dataSize;
  OSStatus err;

  err = ::GetCurrentScrap(&scrap);
  if (err != noErr) return NS_ERROR_FAILURE;
  err = ::GetScrapFlavorSize(scrap, inMacFlavor, &dataSize);
  if (err != noErr) return NS_ERROR_FAILURE;
  if (dataSize > 0) {
    char* dataBuff = (char*) nsMemory::Alloc(dataSize);
    if ( !dataBuff )
      return NS_ERROR_OUT_OF_MEMORY;
      
    // ::GetScrapFlavorData can be very expensive when a conversion
    // is required (say the OS does the conversion from TEXT to utxt). Be
    // sure to only call this when pasting. We no longer use it in 
    // CheckIfFlavorPresent() for this very reason.
    err = ::GetScrapFlavorData(scrap, inMacFlavor, &dataSize, dataBuff);
    NS_ASSERTION(err == noErr, "nsClipboard:: Error getting data off clipboard");
    if ( err ) {
      nsMemory::Free(dataBuff);
      return NS_ERROR_FAILURE;
    }

    // put it into the transferable
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
    NS_ASSERTION(dataSize > 0, "nsClipboard:: Error getting data off the clipboard, size negative");
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
    else
      return NS_ERROR_FAILURE;
  }
#endif /* TARGET_CARBON */
  return NS_OK;
  
} // GetDataOffClipboard


//
// HasDataMatchingFlavors
//
// Check the clipboard to see if we have any data that matches the given flavors. This
// does NOT actually fetch the data. The items in the flavor list are nsISupportsCString's.
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
    nsCOMPtr<nsISupportsCString> flavorWrapper ( do_QueryInterface(genericFlavor) );
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
        if ( CheckIfFlavorPresent(macFlavor) ) {
          *outResult = PR_TRUE;   // we found one!
          break;
        }
        else {
          // if the client asked for unicode and it wasn't present, check if we have TEXT.
          // We'll handle the actual data substitution in GetNativeClipboardData().
          if ( strcmp(flavor, kUnicodeMime) == 0 ) {
            if ( CheckIfFlavorPresent('TEXT') ) {
              *outResult = PR_TRUE;
              break;
            }
          }
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

#if TARGET_CARBON
  ScrapRef scrap = nsnull;
  OSStatus err = ::GetCurrentScrap(&scrap);
  if ( scrap ) {
  
    // the OS clipboard may contain promises and require conversions. Instead
    // of calling in those promises, we can use ::GetScrapFlavorInfoList() to
    // see the list of what could be there if we asked for it. This is really
    // fast. Iterate over the list, and if we find it, we're good to go.
    UInt32 flavorCount = 0;
    ::GetScrapFlavorCount ( scrap, &flavorCount );    
    ScrapFlavorInfo* flavorList = new ScrapFlavorInfo[flavorCount];
    if ( flavorList ) {
      err = ::GetScrapFlavorInfoList ( scrap, &flavorCount, flavorList );
      if ( !err && flavorList ) {
        for ( unsigned int i = 0; i < flavorCount; ++i ) {
          if ( flavorList[i].flavorType == inMacFlavor )
            retval = PR_TRUE;
        }
        delete flavorList;
      }
    }

  }
#else
  long offsetUnused = 0;
  long clipResult = ::GetScrap(NULL, inMacFlavor, &offsetUnused);
  if ( clipResult > 0 )   
    retval = PR_TRUE;   // we found one!
#endif

  return retval;
} // CheckIfFlavorPresent
