/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "nsClipboard.h"
#include "nsVoidArray.h"

#include <unidef.h>     // for UniStrlen

// The relation between mozilla's mime formats and those understood by the
// clipboard is a little hazy, mostly in the areas of images.
//
// Could be a lot cleverer & use delayed rendering or something to provide
// a consumer with text when what we've got is unicode, or whatever.

// Okay, I'm not entirely sure why I did this in this way; it just sortof grew
// like this...

typedef PRUint32 (*ImportDataFn)( void *pData, void **pDataOut);
typedef PRUint32 (*ExportDataFn)( void *pDataIn, PRUint32 cData, void *pBuffer);

struct FormatRecord
{
   const char   *szMimeType;
   ULONG         ulClipboardFmt;
   const char   *szFmtName;
   ImportDataFn  pfnImport;
   ExportDataFn  pfnExport;
};

// scaffolding
nsClipboard::nsClipboard()
{}

nsClipboard::~nsClipboard()
{}

nsresult nsClipboard::SetNativeClipboardData()
{
   return DoClipboardAction( nsClipboard::Write);
}

nsresult nsClipboard::GetNativeClipboardData( nsITransferable *aTransferable)
{
   nsITransferable *tmp = mTransferable;
   mTransferable = aTransferable;
   nsresult rc = DoClipboardAction( nsClipboard::Read);
   mTransferable = tmp;
   return rc;
}

// Native clipboard.
//
// We do slightly odd things in order to store the length of the data
// in the object: for non textual data, the first 4 bytes is an ULONG
// containing the length of the REST of the data.  May change this.
//


// Get some data from the clipboard
void nsClipboard::GetClipboardData( nsString *aFlavour)
{
   FormatRecord *pRecord;
   ULONG         ulFmt = GetFormatID( aFlavour, &pRecord);
   PULONG        pData = (PULONG) WinQueryClipbrdData( 0/*hab*/, ulFmt);

   if( pData)
   {
      PRUint32 cbData = pRecord->pfnImport( pData, (void**) &pData);

      // XXX any guesses how we could decode a HBITMAP into gif/png/jpeg data??
      //
      //     We have libpng ready to use, which is well documented & configurable.
      //     It should be easy to write a routine to convert a bitmap directly to
      //     an in-memory buffer as required here.
      //
      // http://www.physik.fu-berlin.de/edv_docu/documentation/libpng/libpng.txt

      // unfortunately we need to copy the data because the transferable
      // needs to be able to delete [] it.
      PRUint8 *pMozData = new PRUint8 [ cbData ];
      memcpy( pMozData, pData, cbData);
      mTransferable->SetTransferData( aFlavour, pMozData, cbData);
   }
}

// Set some data onto the clipboard
void nsClipboard::SetClipboardData( nsString *aFlavour)
{
   void     *pMozData = nsnull, *pData = nsnull;
   PRUint32  cbMozData = 0, cbData = 0;

   // Get the data from the transferable
   mTransferable->GetTransferData( aFlavour, &pMozData, &cbMozData);

   // Figure out how much memory we need to store the native version
   FormatRecord *pRecord;
   GetFormatID( aFlavour, &pRecord);
   cbData = pRecord->pfnExport( pMozData, cbMozData, 0);

   // allocatate some memory to put the data in
   APIRET rc = DosAllocSharedMem( &pData, nsnull, cbData, 
                                  PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE);
   if( !rc)
   {
      // copy across & pin up.
      pRecord->pfnExport( pMozData, cbMozData, pData);
      WinSetClipbrdData( 0/*hab*/, (ULONG) pData,
                         pRecord->ulClipboardFmt, CFI_POINTER);
   }
#ifdef DEBUG
   else
      printf( "DosAllocSharedMem failed, rc %d\n", (int)rc);
#endif
}

// Go through the flavors in the transferable and either get or set them
nsresult nsClipboard::DoClipboardAction( nsClipboard::ClipboardAction aAction)
{
   nsresult rc = NS_ERROR_FAILURE;

   if( WinOpenClipbrd( 0/*hab*/))
   {
      if( aAction == nsClipboard::Write)
         WinEmptyClipbrd( 0/*hab*/);

      // Get the list of formats the transferable can handle
      nsVoidArray *pFormats = nsnull;
      if( aAction == nsClipboard::Read)
         mTransferable->FlavorsTransferableCanImport( &pFormats);
      else
         mTransferable->FlavorsTransferableCanExport( &pFormats);

      PRUint32 cFormats = pFormats->Count();

      for( PRUint32 i = 0; i < cFormats; i++)
      {
         nsString *pFlavour = (nsString*) pFormats->ElementAt( i);
         if( pFlavour) // just in case
         {
            if( aAction == nsClipboard::Read) GetClipboardData( pFlavour);
            else                              SetClipboardData( pFlavour);
         }
      }

      WinCloseClipbrd( 0/*hab*/);
      rc = NS_OK;
      delete pFormats;
   }

   return rc;
}

// Converters for various types of data

static PRUint32 Import8Bit( void *pString, void **pOut)
{
   *pOut = pString;
   return strlen( (char*) pString);
}

static PRUint32 Export8Bit( void *pDataIn, PRUint32 cData, void *pBuffer)
{
   if( pBuffer)
   {
      memcpy( pBuffer, pDataIn, cData);
      *(((char*)pBuffer) + cData) = '\0';
   }
   return cData + 1;
}

static PRUint32 Import16Bit( void *pString, void **pOut)
{
   *pOut = pString;
   return UniStrlen( (UniChar*) pString) * 2;
}

static PRUint32 Export16Bit( void *pDataIn, PRUint32 cData, void *pBuffer)
{
   if( pBuffer)
   {
      memcpy( pBuffer, pDataIn, cData);
      *((UniChar*)pBuffer + (cData>>1)) = 0;
   }
   return cData + sizeof(UniChar);
}

static PRUint32 ImportBin( void *pData, void **pOut)
{
   *pOut = ((PRUint32*)pData) + 1;
   return *(PRUint32*)pData;
}

static PRUint32 ExportBin( void *pDataIn, PRUint32 cData, void *pBuffer)
{
   if( pBuffer)
   {
      *((PRUint32*)pBuffer) = cData;
      memcpy( (PRUint32*)pBuffer + 1, pDataIn, cData);
   }
   return cData + sizeof(PRUint32);
}

static FormatRecord records[] = 
{
   { kTextMime,      CF_TEXT, 0,      Import8Bit, Export8Bit },
   { kUnicodeMime,   0, "CF_UNICODE", Import16Bit, Export16Bit },
   { kHTMLMime,      0, "CF_HTML",    Import8Bit, Export8Bit },
   { kXIFMime,       0, "CF_XIF",     Import8Bit, Export8Bit },
   { kPNGImageMime,  0, "CF_PNG",     ImportBin, ExportBin },
   { kGIFImageMime,  0, "CF_GIF",     ImportBin, ExportBin },
   { kJPEGImageMime, 0, "CF_JPEG",    ImportBin, ExportBin },
   { kAOLMailMime,   0, "CF_AOLMAIL", Import8Bit, Export8Bit },
   { 0, 0, 0, 0, 0 }
};

// get the format ID for a given mimetype
ULONG nsClipboard::GetFormatID( nsString *aMimeStr, FormatRecord **pFmtRec)
{
   ULONG ulFormat = 0;

   const char *pszFmt = gModuleData.ConvertFromUcs( *aMimeStr);

   for( FormatRecord *pRecord = records; pRecord->szMimeType; pRecord++)
      if( !strcmp( pRecord->szMimeType, pszFmt))
      {
         if( !pRecord->ulClipboardFmt)
            // create an atom for the format
            pRecord->ulClipboardFmt = gModuleData.GetAtom( pRecord->szFmtName);
         ulFormat = pRecord->ulClipboardFmt;
         *pFmtRec = pRecord;
         break;
      }

   NS_ASSERTION(ulFormat, "Clipboard beseiged by unknown mimetype");

   return ulFormat;
}
