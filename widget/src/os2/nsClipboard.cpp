/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*
 *
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
 *   2000/08/04     Henry Sobotka <sobotka@axess.com>  Update from M7
 *   2000/10/02     IBM Corp.                          Sync-up to M18 level
 *
 */

#include "nsClipboard.h"
#include "nsVoidArray.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsPrimitiveHelpers.h"
#include "nsXPIDLString.h"
#include "prmem.h"

#include <unidef.h>     // for UniStrlen

// The relation between mozilla's mime formats and those understood by the
// clipboard is a little hazy, mostly in the areas of images.
//
// Could be a lot cleverer & use delayed rendering or something to provide
// a consumer with text when what we've got is unicode, or whatever.

// Okay, I'm not entirely sure why I did this in this way; it just sortof grew
// like this...

typedef PRUint32 (*ImportDataFn)(void *pData, void **pDataOut);
typedef PRUint32 (*ExportDataFn)(void *pDataIn, PRUint32 cData, void *pBuffer);

struct FormatRecord
{
  const char   *szMimeType;
  ULONG         ulClipboardFmt;
  const char   *szFmtName;
  ImportDataFn  pfnImport;
  ExportDataFn  pfnExport;
};

// scaffolding
nsClipboard::nsClipboard() : nsBaseClipboard()
{}

nsClipboard::~nsClipboard()
{}

nsresult nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  return DoClipboardAction(Write);
}

nsresult nsClipboard::GetNativeClipboardData(nsITransferable *aTransferable, PRInt32 aWhichClipboard)
{
  // make sure we have a good transferable
  if (!aTransferable || aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  nsITransferable *tmp = mTransferable;
  mTransferable = aTransferable;
  nsresult rc = DoClipboardAction(Read);
  mTransferable = tmp;
  return rc;
}

// Get some data from the clipboard
PRBool nsClipboard::GetClipboardData(const char *aFlavour)
{
  PRBool dataFound = PR_FALSE;
  FormatRecord *pRecord;
  ULONG ulFmt = GetFormatID(aFlavour, &pRecord);

  // XXX OS2TODO - images: just handle text until then
  if (strstr(pRecord->szMimeType, "text") == NULL)
    return PR_FALSE;

  void* pData = (void*) WinQueryClipbrdData(0/*hab*/, ulFmt);

  if (pData) {

    PRUint32 cbData = pRecord->pfnImport(pData, (void**) &pData);

    // XXX HTML gets found but always seems to end up a 1-byte empty string
    if (strcmp(pRecord->szMimeType, kHTMLMime))
      dataFound = PR_TRUE;

    if (!strcmp(pRecord->szMimeType, kTextMime)) {

      const char* castText = NS_REINTERPRET_CAST(char*, pData);
      ULONG txtLen = strlen(castText) + 1;
      PRUnichar* uniBuf = (UniChar*)PR_Malloc(txtLen);
      PRUnichar* convertedText = gModuleData.ConvertToUcs(castText, uniBuf, txtLen);

      if (convertedText) {
        pData = NS_REINTERPRET_CAST(void*, convertedText);
        cbData = txtLen * 2;
      }
    }

    if (dataFound == PR_TRUE) {

      // Set converted text as unicode
      const char* flavorStr = (aFlavour == kTextMime) ? kUnicodeMime : aFlavour;

      // XXX OS2TODO: why does conversion SIGSEGV???
#if 0
      // DOM wants LF only, so convert from CRLF
      PRInt32 signedLen = NS_STATIC_CAST(PRInt32, cbData);
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, &pData, &signedLen);
      cbData = NS_STATIC_CAST(PRUint32, signedLen);
#endif

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, pData, cbData, getter_AddRefs(genericDataWrapper));
      nsresult errCode = mTransferable->SetTransferData(flavorStr, genericDataWrapper, cbData);
#ifdef NS_DEBUG
      if (errCode != NS_OK) printf("nsClipboard:: Error setting data into transferable\n");
#endif
    }
  }

  // check for text which, if found, we convert to unicode
  if (!strcmp(pRecord->szMimeType, kUnicodeMime) && (dataFound == PR_FALSE))
    GetClipboardData(kTextMime);

  return dataFound;
}

// Set some data onto the clipboard
void nsClipboard::SetClipboardData(const char *aFlavour)
{
  void *pMozData = nsnull;
  void *pData = nsnull;
  PRUint32  cbMozData = 0, cbData = 0;

  // Get the data from the transferable
  nsCOMPtr<nsISupports> genericDataWrapper;
  nsresult errCode = mTransferable->GetTransferData(aFlavour, getter_AddRefs(genericDataWrapper), &cbMozData);
#ifdef NS_DEBUG
  if (NS_FAILED(errCode)) printf("nsClipboard:: Error getting data from transferable\n");
#endif
  nsPrimitiveHelpers::CreateDataFromPrimitive(aFlavour, genericDataWrapper, &pMozData, cbMozData);

  // Figure out how much memory we need to store the native version
  FormatRecord *pRecord;
  GetFormatID(aFlavour, &pRecord);
  cbData = pRecord->pfnExport(pMozData, cbMozData, 0);

  // allocate some memory to put the data in
  APIRET rc = DosAllocSharedMem(&pData, nsnull, cbData,
                                 PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE);

  if (!rc) {

    // copy across & pin up.
    pRecord->pfnExport(pMozData, cbMozData, pData);

    if (!WinSetClipbrdData(0/*hab*/, (ULONG)pData, pRecord->ulClipboardFmt, CFI_POINTER))
      printf("ERROR: nsClipboard setting %s transfer data\n", pRecord->szMimeType);
  }

  // If the flavor is unicode, we also put it on the clipboard as CF_TEXT
  // after conversion to locale charset.
  if (!strcmp(pRecord->szMimeType, kUnicodeMime)) {

    const PRUnichar* uniData = (PRUnichar*)pMozData;
    ULONG uniLen = UniStrlen(uniData) + 1;
    char* uniBuf = (char*)PR_Malloc(uniLen);
    const char* plainTextData = gModuleData.ConvertFromUcs(uniData, uniBuf, uniLen);

    if (plainTextData) {

      ULONG plainTextLen = strlen(plainTextData) + 1;

      rc = DosAllocSharedMem(&pData, nsnull, plainTextLen,
                              PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE);

      if (!rc) {

        pRecord->pfnExport((void*)plainTextData, plainTextLen, pData);

        if (!WinSetClipbrdData(0/*hab*/, (ULONG)pData, CF_TEXT, CFI_POINTER))
          printf("ERROR: nsClipboard setting CF_TEXT\n");
      }
    }
    PR_Free(uniBuf);
  }
}

// Go through the flavors in the transferable and either get or set them
nsresult nsClipboard::DoClipboardAction(ClipboardAction aAction)
{
  nsresult rc = NS_ERROR_FAILURE;

  if (WinOpenClipbrd(0/*hab*/)) {

    if (aAction == Write)
      WinEmptyClipbrd(0/*hab*/);

    // Get the list of formats the transferable can handle
    nsCOMPtr<nsISupportsArray> pFormats;
    if(aAction == Read)
      rc = mTransferable->FlavorsTransferableCanImport(getter_AddRefs(pFormats));
    else
      rc = mTransferable->FlavorsTransferableCanExport(getter_AddRefs(pFormats));

    if (NS_FAILED(rc))
      return NS_ERROR_FAILURE;

    PRUint32 cFormats = 0;
    pFormats->Count(&cFormats);

    for (PRUint32 i = 0; i < cFormats; i++) {

      nsCOMPtr<nsISupports> genericFlavor;
      pFormats->GetElementAt(i, getter_AddRefs(genericFlavor));
      nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));

      if (currentFlavor) {
        nsXPIDLCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));

        if (aAction == Read) {
          if (GetClipboardData(flavorStr))
            break;
        }
        else
          SetClipboardData(flavorStr);
      }
    }
    WinCloseClipbrd(0/*hab*/);
    rc = NS_OK;
  }
  return rc;
}

// Converters for various types of data
static PRUint32 Import8Bit(void *pString, void **pOut)
{
  *pOut = pString;
  return strlen((char*) pString);
}

static PRUint32 Export8Bit(void *pDataIn, PRUint32 cData, void *pBuffer)
{
  if (pBuffer) {
    memcpy(pBuffer, pDataIn, cData);
    *(((char*)pBuffer) + cData) = '\0';
  }
  return cData + 1;
}

static PRUint32 Import16Bit(void *pString, void **pOut)
{
  *pOut = pString;
  return UniStrlen((UniChar*) pString) * 2;
}

static PRUint32 Export16Bit(void *pDataIn, PRUint32 cData, void *pBuffer)
{
  if (pBuffer) {
    memcpy(pBuffer, pDataIn, cData);
    *((UniChar*)pBuffer + (cData>>1)) = 0;
  }
  return cData + sizeof(UniChar);
}

static PRUint32 ImportBin(void *pData, void **pOut)
{
  *pOut = ((PRUint32*)pData) + 1;
  return *(PRUint32*)pData;
}

static PRUint32 ExportBin(void *pDataIn, PRUint32 cData, void *pBuffer)
{
  if (pBuffer) {
    *((PRUint32*)pBuffer) = cData;
    memcpy((PRUint32*)pBuffer + 1, pDataIn, cData);
  }
  return cData + sizeof(PRUint32);
}

static FormatRecord records[] = 
{
  { kTextMime,      CF_TEXT, 0,      Import8Bit,  Export8Bit  },
  { kUnicodeMime,   0, "CF_UNICODE", Import16Bit, Export16Bit },
  { kHTMLMime,      0, "CF_HTML",    Import8Bit,  Export8Bit  },
  { kPNGImageMime,  0, "CF_PNG",     ImportBin,   ExportBin   },
  { kGIFImageMime,  0, "CF_GIF",     ImportBin,   ExportBin   },
  { kJPEGImageMime, 0, "CF_JPEG",    ImportBin,   ExportBin   },
  { kAOLMailMime,   0, "CF_AOLMAIL", Import8Bit,  Export8Bit  },
  { kFileMime,      0, "CF_FILE",    ImportBin,   ExportBin   },
  { kURLMime,       0, "CF_URL",     Import16Bit, Export16Bit },
  { 0, 0, 0, 0, 0 }
};

// get the format ID for a given mimetype
ULONG nsClipboard::GetFormatID(const char *aMimeStr, FormatRecord **pFmtRec)
{
  ULONG ulFormat = 0;
  const char *pszFmt = aMimeStr;

  for (FormatRecord *pRecord = records; pRecord->szMimeType; pRecord++)
    if (!strcmp(pRecord->szMimeType, pszFmt)) {
      if (!pRecord->ulClipboardFmt)
        // create an atom for the format
        pRecord->ulClipboardFmt = gModuleData.GetAtom(pRecord->szFmtName);
      ulFormat = pRecord->ulClipboardFmt;
      *pFmtRec = pRecord;
      break;
    }

  NS_ASSERTION(ulFormat, "Clipboard besieged by unknown mimetype");

  return ulFormat;
}

NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  // make sure we have a good transferable
  if (!mTransferable || aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  if (WinOpenClipbrd(0/*hab*/)) {
    WinEmptyClipbrd(0/*hab*/);

    // get flavor list that includes all flavors that can be written (including ones
    // obtained through conversion)
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult errCode = mTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
    if (NS_FAILED(errCode))
      return NS_ERROR_FAILURE;

    // Walk through flavors and put data on to clipboard
    PRUint32 i;
    PRUint32 cnt;
    flavorList->Count(&cnt);
    for (i = 0; i < cnt; i++) {
      nsCOMPtr<nsISupports> genericFlavor;
      flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
      nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));
      if (currentFlavor) {
        nsXPIDLCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));
        SetClipboardData(flavorStr);
      }
    }
    WinCloseClipbrd(0/*hab*/);
  }
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(nsISupportsArray *aFlavorList, PRInt32 aWhichClipboard,
                                                  PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (aWhichClipboard != kGlobalClipboard)
    return NS_OK;

  PRUint32 cnt;
  aFlavorList->Count(&cnt);
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      FormatRecord *pRecord;
      ULONG fmtInfo = 0;
      ULONG format = GetFormatID(flavorStr, &pRecord);

      if (WinQueryClipbrdFmtInfo(0/*hab*/, format, &fmtInfo)) {
        *_retval = PR_TRUE;
        break;
      }
      else {
        // if the client asked for unicode and it wasn't present, check if we have CF_TEXT.
        if (!strcmp(flavorStr, kUnicodeMime)) {
          if (WinQueryClipbrdFmtInfo(0/*hab*/, CF_TEXT, &fmtInfo))
            *_retval = PR_TRUE;
        }
      }
    }
  }
  return NS_OK;
}
