/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define MOZILLA_INTERNAL_API

#include <ole2.h>
#include <shlobj.h>

#include "TestHarness.h"
#include "nsIArray.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsIFileURL.h"
#include "nsITransferable.h"
#include "nsClipboard.h"
#include "nsDataObjCollection.h"

nsIFile* xferFile;

nsresult CheckValidHDROP(STGMEDIUM* pSTG)
{
  if (pSTG->tymed != TYMED_HGLOBAL) {
    fail("Received data is not in an HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  HGLOBAL hGlobal = pSTG->hGlobal;
  DROPFILES* pDropFiles;
  pDropFiles = (DROPFILES*)GlobalLock(hGlobal);
  if (!pDropFiles) {
    fail("There is no data at the given HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  if (pDropFiles->pFiles != sizeof(DROPFILES))
    fail("DROPFILES struct has wrong size");

  if (pDropFiles->fWide != true) {
    fail("Received data is not Unicode");
    return NS_ERROR_UNEXPECTED;
  }
  nsString s;
  unsigned long offset = 0;
  while (1) {
    s = (PRUnichar*)((char*)pDropFiles + pDropFiles->pFiles + offset);
    if (s.IsEmpty())
      break;
    nsresult rv;
    nsCOMPtr<nsIFile> localFile(
               do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    rv = localFile->InitWithPath(s);
    if (NS_FAILED(rv)) {
      fail("File could not be opened");
      return NS_ERROR_UNEXPECTED;
    }
    offset += sizeof(PRUnichar) * (s.Length() + 1);
  }
  return NS_OK;
}

nsresult CheckValidTEXT(STGMEDIUM* pSTG)
{
  if (pSTG->tymed != TYMED_HGLOBAL) {
    fail("Received data is not in an HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  HGLOBAL hGlobal = pSTG->hGlobal;
  char* pText;
  pText = (char*)GlobalLock(hGlobal);
  if (!pText) {
    fail("There is no data at the given HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  nsCString string;
  string = pText;

  if (!string.Equals(NS_LITERAL_CSTRING("Mozilla can drag and drop"))) {
    fail("Text passed through drop object wrong");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult CheckValidTEXTTwo(STGMEDIUM* pSTG)
{
  if (pSTG->tymed != TYMED_HGLOBAL) {
    fail("Received data is not in an HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  HGLOBAL hGlobal = pSTG->hGlobal;
  char* pText;
  pText = (char*)GlobalLock(hGlobal);
  if (!pText) {
    fail("There is no data at the given HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  nsCString string;
  string = pText;

  if (!string.Equals(NS_LITERAL_CSTRING("Mozilla can drag and drop twice over"))) {
    fail("Text passed through drop object wrong");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult CheckValidUNICODE(STGMEDIUM* pSTG)
{
  if (pSTG->tymed != TYMED_HGLOBAL) {
    fail("Received data is not in an HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  HGLOBAL hGlobal = pSTG->hGlobal;
  PRUnichar* pText;
  pText = (PRUnichar*)GlobalLock(hGlobal);
  if (!pText) {
    fail("There is no data at the given HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  nsString string;
  string = pText;

  if (!string.Equals(NS_LITERAL_STRING("Mozilla can drag and drop"))) {
    fail("Text passed through drop object wrong");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult CheckValidUNICODETwo(STGMEDIUM* pSTG)
{
  if (pSTG->tymed != TYMED_HGLOBAL) {
    fail("Received data is not in an HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  HGLOBAL hGlobal = pSTG->hGlobal;
  PRUnichar* pText;
  pText = (PRUnichar*)GlobalLock(hGlobal);
  if (!pText) {
    fail("There is no data at the given HGLOBAL");
    return NS_ERROR_UNEXPECTED;
  }

  nsString string;
  string = pText;

  if (!string.Equals(NS_LITERAL_STRING("Mozilla can drag and drop twice over"))) {
    fail("Text passed through drop object wrong");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult GetTransferableFile(nsCOMPtr<nsITransferable>& pTransferable)
{
  nsresult rv;

  nsCOMPtr<nsISupports> genericWrapper = do_QueryInterface(xferFile);

  pTransferable = do_CreateInstance("@mozilla.org/widget/transferable;1");
  pTransferable->Init(nullptr);
  rv = pTransferable->SetTransferData("application/x-moz-file", genericWrapper,
                                      0);
  return rv;
}

nsresult GetTransferableText(nsCOMPtr<nsITransferable>& pTransferable)
{
  nsresult rv;
  NS_NAMED_LITERAL_STRING(mozString, "Mozilla can drag and drop");
  nsCOMPtr<nsISupportsString> xferString =
                               do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
  rv = xferString->SetData(mozString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> genericWrapper = do_QueryInterface(xferString);

  pTransferable = do_CreateInstance("@mozilla.org/widget/transferable;1");
  pTransferable->Init(nullptr);
  rv = pTransferable->SetTransferData("text/unicode", genericWrapper,
                                      mozString.Length() * sizeof(PRUnichar));
  return rv;
}

nsresult GetTransferableTextTwo(nsCOMPtr<nsITransferable>& pTransferable)
{
  nsresult rv;
  NS_NAMED_LITERAL_STRING(mozString, " twice over");
  nsCOMPtr<nsISupportsString> xferString =
                               do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
  rv = xferString->SetData(mozString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> genericWrapper = do_QueryInterface(xferString);

  pTransferable = do_CreateInstance("@mozilla.org/widget/transferable;1");
  pTransferable->Init(nullptr);
  rv = pTransferable->SetTransferData("text/unicode", genericWrapper,
                                      mozString.Length() * sizeof(PRUnichar));
  return rv;
}

nsresult GetTransferableURI(nsCOMPtr<nsITransferable>& pTransferable)
{
  nsresult rv;

  nsCOMPtr<nsIURI> xferURI;

  rv = NS_NewURI(getter_AddRefs(xferURI), "http://www.mozilla.org");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> genericWrapper = do_QueryInterface(xferURI);

  pTransferable = do_CreateInstance("@mozilla.org/widget/transferable;1");
  pTransferable->Init(nullptr);
  rv = pTransferable->SetTransferData("text/x-moz-url", genericWrapper, 0);
  return rv;
}

nsresult MakeDataObject(nsISupportsArray* transferableArray,
                        nsRefPtr<IDataObject>& itemToDrag)
{
  nsresult rv;
  uint32_t itemCount = 0;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), "http://www.mozilla.org");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transferableArray->Count(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copied more or less exactly from nsDragService::InvokeDragSession
  // This is what lets us play fake Drag Service for the test
  if (itemCount > 1) {
    nsDataObjCollection * dataObjCollection = new nsDataObjCollection();
    if (!dataObjCollection)
      return NS_ERROR_OUT_OF_MEMORY;
    itemToDrag = dataObjCollection;
    for (uint32_t i=0; i<itemCount; ++i) {
      nsCOMPtr<nsISupports> supports;
      transferableArray->GetElementAt(i, getter_AddRefs(supports));
      nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
      if (trans) {
        nsRefPtr<IDataObject> dataObj;
        rv = nsClipboard::CreateNativeDataObject(trans,
                                                 getter_AddRefs(dataObj), uri);
        NS_ENSURE_SUCCESS(rv, rv);
        // Add the flavors to the collection object too
        rv = nsClipboard::SetupNativeDataObject(trans, dataObjCollection);
        NS_ENSURE_SUCCESS(rv, rv);

        dataObjCollection->AddDataObject(dataObj);
      }
    }
  } // if dragging multiple items
  else {
    nsCOMPtr<nsISupports> supports;
    transferableArray->GetElementAt(0, getter_AddRefs(supports));
    nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
    if (trans) {
      rv = nsClipboard::CreateNativeDataObject(trans,
                                               getter_AddRefs(itemToDrag),
                                               uri);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } // else dragging a single object
  return rv;
}

nsresult Do_CheckOneFile()
{
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable;
  nsCOMPtr<nsISupportsArray> transferableArray;
  nsCOMPtr<nsISupports> genericWrapper;
  nsRefPtr<IDataObject> dataObj;
  rv = NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (NS_FAILED(rv)) {
    fail("Could not create the necessary nsISupportsArray");
    return rv;
  }

  rv = GetTransferableFile(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = MakeDataObject(transferableArray, dataObj);
  if (NS_FAILED(rv)) {
    fail("Could not create data object");
    return rv;
  }

  FORMATETC fe;
  SET_FORMATETC(fe, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("File data object does not support the file data type!");
    return NS_ERROR_UNEXPECTED;
  }

  STGMEDIUM* stg;
  stg = (STGMEDIUM*)CoTaskMemAlloc(sizeof(STGMEDIUM));
  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("File data object did not provide data on request");
    return NS_ERROR_UNEXPECTED;
  }

  rv = CheckValidHDROP(stg);
  if (NS_FAILED(rv)) {
    fail("HDROP was invalid");
    return rv;
  }

  ReleaseStgMedium(stg);

  return NS_OK;
}

nsresult Do_CheckTwoFiles()
{
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable;
  nsCOMPtr<nsISupportsArray> transferableArray;
  nsCOMPtr<nsISupports> genericWrapper;
  nsRefPtr<IDataObject> dataObj;
  rv = NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (NS_FAILED(rv)) {
    fail("Could not create the necessary nsISupportsArray");
    return rv;
  }

  rv = GetTransferableFile(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = GetTransferableFile(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = MakeDataObject(transferableArray, dataObj);
  if (NS_FAILED(rv)) {
    fail("Could not create data object");
    return rv;
  }

  FORMATETC fe;
  SET_FORMATETC(fe, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("File data object does not support the file data type!");
    return NS_ERROR_UNEXPECTED;
  }

  STGMEDIUM* stg;
  stg = (STGMEDIUM*)CoTaskMemAlloc(sizeof(STGMEDIUM));
  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("File data object did not provide data on request");
    return NS_ERROR_UNEXPECTED;
  }

  rv = CheckValidHDROP(stg);
  if (NS_FAILED(rv)) {
    fail("HDROP was invalid");
    return rv;
  }

  ReleaseStgMedium(stg);

  return NS_OK;
}

nsresult Do_CheckOneString()
{
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable;
  nsCOMPtr<nsISupportsArray> transferableArray;
  nsCOMPtr<nsISupports> genericWrapper;
  nsRefPtr<IDataObject> dataObj;
  rv = NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (NS_FAILED(rv)) {
    fail("Could not create the necessary nsISupportsArray");
    return rv;
  }

  rv = GetTransferableText(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = MakeDataObject(transferableArray, dataObj);
  if (NS_FAILED(rv)) {
    fail("Could not create data object");
    return rv;
  }

  FORMATETC fe;
  SET_FORMATETC(fe, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("String data object does not support the ASCII text data type!");
    return NS_ERROR_UNEXPECTED;
  }

  STGMEDIUM* stg;
  stg = (STGMEDIUM*)CoTaskMemAlloc(sizeof(STGMEDIUM));
  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("String data object did not provide ASCII data on request");
    return NS_ERROR_UNEXPECTED;
  }

  rv = CheckValidTEXT(stg);
  if (NS_FAILED(rv)) {
    fail("TEXT was invalid");
    return rv;
  }

  ReleaseStgMedium(stg);

  SET_FORMATETC(fe, CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("String data object does not support the wide text data type!");
    return NS_ERROR_UNEXPECTED;
  }

  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("String data object did not provide wide data on request");
    return NS_ERROR_UNEXPECTED;
  }
  
  rv = CheckValidUNICODE(stg);
  if (NS_FAILED(rv)) {
    fail("UNICODE was invalid");
    return rv;
  }
  
  return NS_OK;
}

nsresult Do_CheckTwoStrings()
{
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable;
  nsCOMPtr<nsISupportsArray> transferableArray;
  nsCOMPtr<nsISupports> genericWrapper;
  nsRefPtr<IDataObject> dataObj;
  rv = NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (NS_FAILED(rv)) {
    fail("Could not create the necessary nsISupportsArray");
    return rv;
  }

  rv = GetTransferableText(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = GetTransferableTextTwo(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }

  rv = MakeDataObject(transferableArray, dataObj);
  if (NS_FAILED(rv)) {
    fail("Could not create data object");
    return rv;
  }

  FORMATETC fe;
  SET_FORMATETC(fe, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("String data object does not support the ASCII text data type!");
    return NS_ERROR_UNEXPECTED;
  }

  STGMEDIUM* stg;
  stg = (STGMEDIUM*)CoTaskMemAlloc(sizeof(STGMEDIUM));
  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("String data object did not provide ASCII data on request");
    return NS_ERROR_UNEXPECTED;
  }

  rv = CheckValidTEXTTwo(stg);
  if (NS_FAILED(rv)) {
    fail("TEXT was invalid");
    return rv;
  }

  ReleaseStgMedium(stg);

  SET_FORMATETC(fe, CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("String data object does not support the wide text data type!");
    return NS_ERROR_UNEXPECTED;
  }

  if (dataObj->GetData(&fe, stg) != S_OK) {
    fail("String data object did not provide wide data on request");
    return NS_ERROR_UNEXPECTED;
  }
  
  rv = CheckValidUNICODETwo(stg);
  if (NS_FAILED(rv)) {
    fail("UNICODE was invalid");
    return rv;
  }
  
  return NS_OK;
}

nsresult Do_CheckSetArbitraryData(bool aMultiple)
{
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable;
  nsCOMPtr<nsISupportsArray> transferableArray;
  nsCOMPtr<nsISupports> genericWrapper;
  nsRefPtr<IDataObject> dataObj;
  rv = NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (NS_FAILED(rv)) {
    fail("Could not create the necessary nsISupportsArray");
    return rv;
  }

  rv = GetTransferableText(transferable);
  if (NS_FAILED(rv)) {
    fail("Could not create the proper nsITransferable!");
    return rv;
  }
  genericWrapper = do_QueryInterface(transferable);
  rv = transferableArray->AppendElement(genericWrapper);
  if (NS_FAILED(rv)) {
    fail("Could not append element to transferable array");
    return rv;
  }
  
  if (aMultiple) {
    rv = GetTransferableText(transferable);
    if (NS_FAILED(rv)) {
      fail("Could not create the proper nsITransferable!");
      return rv;
    }
    genericWrapper = do_QueryInterface(transferable);
    rv = transferableArray->AppendElement(genericWrapper);
    if (NS_FAILED(rv)) {
      fail("Could not append element to transferable array");
      return rv;
    }
  }

  rv = MakeDataObject(transferableArray, dataObj);
  if (NS_FAILED(rv)) {
    fail("Could not create data object");
    return rv;
  }

  static CLIPFORMAT mozArbitraryFormat =
                               ::RegisterClipboardFormatW(L"MozillaTestFormat");
  FORMATETC fe;
  STGMEDIUM stg;
  SET_FORMATETC(fe, mozArbitraryFormat, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

  HGLOBAL hg = GlobalAlloc(GPTR, 1024);
  stg.tymed = TYMED_HGLOBAL;
  stg.hGlobal = hg;
  stg.pUnkForRelease = nullptr;

  if (dataObj->SetData(&fe, &stg, true) != S_OK) {
    if (aMultiple) {
      fail("Unable to set arbitrary data type on data object collection!");
    } else {
      fail("Unable to set arbitrary data type on data object!");
    }
    return NS_ERROR_UNEXPECTED;
  }

  if (dataObj->QueryGetData(&fe) != S_OK) {
    fail("Arbitrary data set on data object is not advertised!");
    return NS_ERROR_UNEXPECTED;
  }

  STGMEDIUM* stg2;
  stg2 = (STGMEDIUM*)CoTaskMemAlloc(sizeof(STGMEDIUM));
  if (dataObj->GetData(&fe, stg2) != S_OK) {
    fail("Data object did not provide arbitrary data upon request!");
    return NS_ERROR_UNEXPECTED;
  }

  if (stg2->hGlobal != hg) {
    fail("Arbitrary data was not returned properly!");
    return rv;
  }
  ReleaseStgMedium(stg2);

  return NS_OK;
}

// This function performs basic drop tests, testing a data object consisting
// of one transferable
nsresult Do_Test1()
{
  nsresult rv = NS_OK;
  nsresult workingrv;

  workingrv = Do_CheckOneFile();
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on a single file");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully created a working file drag object!");
  }

  workingrv = Do_CheckOneString();
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on a single string");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully created a working string drag object!");
  }

  workingrv = Do_CheckSetArbitraryData(false);
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on setting arbitrary data");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully set arbitrary data on a drag object");
  }

  return rv;
}

// This function performs basic drop tests, testing a data object consisting of
// two transferables.
nsresult Do_Test2()
{
  nsresult rv = NS_OK;
  nsresult workingrv;

  workingrv = Do_CheckTwoFiles();
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on multiple files");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully created a working multiple file drag object!");
  }

  workingrv = Do_CheckTwoStrings();
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on multiple strings");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully created a working multiple string drag object!");
  }

  workingrv = Do_CheckSetArbitraryData(true);
  if (NS_FAILED(workingrv)) {
    fail("Drag object tests failed on setting arbitrary data");
    rv = NS_ERROR_UNEXPECTED;
  } else {
    passed("Successfully set arbitrary data on a drag object");
  }

  return rv;
}

// This function performs advanced drag and drop tests, testing a data object
// consisting of multiple transferables that have different data types
nsresult Do_Test3()
{
  nsresult rv = NS_OK;
  nsresult workingrv;

  // XXX TODO Write more advanced tests in Bug 535860
  return rv;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("Test Windows Drag and Drop");

  nsCOMPtr<nsIFile> file;
  file = xpcom.GetProfileDirectory();
  xferFile = file;

  if (NS_SUCCEEDED(Do_Test1()))
    passed("Basic Drag and Drop data type tests (single transferable) succeeded!");

  if (NS_SUCCEEDED(Do_Test2()))
    passed("Basic Drag and Drop data type tests (multiple transferables) succeeded!");

//if (NS_SUCCEEDED(Do_Test3()))
//  passed("Advanced Drag and Drop data type tests succeeded!");

  return gFailCount;
}
