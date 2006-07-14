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
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Mark Hammond (MarkH@ActiveState.com)
 *   David Gardiner <david.gardiner@unisa.edu.au>
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

#include <ole2.h>
#include <oleidl.h>
#include <shlobj.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsDataObj.h"

#include "nsWidgetsCID.h"
#include "nsNativeDragTarget.h"
#include "nsNativeDragSource.h"
#include "nsClipboard.h"
#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsDataObjCollection.h"

#include "nsAutoPtr.h"

#include "nsString.h"
#include "nsEscape.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsCWebBrowserPersist.h"
#include "nsIDownload.h"
#include "nsToolkit.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsUnicharUtils.h"

// Static member declaration
PRUnichar *nsDragService::mDropPath;
PRUnichar *nsDragService::mFileName;

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
  : mNativeDragSrc(nsnull), mNativeDragTarget(nsnull), mDataObject(nsnull)
{
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
  if (mFileName)
    NS_Free(mFileName);
  if (mDropPath)
    NS_Free(mDropPath);

  NS_IF_RELEASE(mNativeDragSrc);
  NS_IF_RELEASE(mNativeDragTarget);
  NS_IF_RELEASE(mDataObject);
}


//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                 nsISupportsArray *anArrayTransferables,
                                 nsIScriptableRegion *aRegion,
                                 PRUint32 aActionType)
{
  nsBaseDragService::InvokeDragSession(aDOMNode, anArrayTransferables, aRegion,
                                       aActionType);
  nsresult rv;

  // Try and get source URI of the items that are being dragged
  nsIURI *uri = nsnull;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mSourceDocument));
  if (doc) {
    uri = doc->GetDocumentURI();
  }

  PRUint32 numItemsToDrag = 0;
  rv = anArrayTransferables->Count(&numItemsToDrag);
  if (!numItemsToDrag)
    return NS_ERROR_FAILURE;

  // The clipboard class contains some static utility methods that we
  // can use to create an IDataObject from the transferable

  // if we're dragging more than one item, we need to create a
  // "collection" object to fake out the OS. This collection contains
  // one |IDataObject| for each transerable. If there is just the one
  // (most cases), only pass around the native |IDataObject|.
  nsRefPtr<IDataObject> itemToDrag;
  if (numItemsToDrag > 1) {
    nsDataObjCollection * dataObjCollection = new nsDataObjCollection();
    if (!dataObjCollection)
      return NS_ERROR_OUT_OF_MEMORY;
    itemToDrag = dataObjCollection;
    for (PRUint32 i=0; i<numItemsToDrag; ++i) {
      nsCOMPtr<nsISupports> supports;
      anArrayTransferables->GetElementAt(i, getter_AddRefs(supports));
      nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
      if (trans) {
        nsRefPtr<IDataObject> dataObj;
        rv = nsClipboard::CreateNativeDataObject(trans,
                                                 getter_AddRefs(dataObj), uri);
        NS_ENSURE_SUCCESS(rv, rv);

        dataObjCollection->AddDataObject(dataObj);
      }
    }
  } // if dragging multiple items
  else {
    nsCOMPtr<nsISupports> supports;
    anArrayTransferables->GetElementAt(0, getter_AddRefs(supports));
    nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
    if (trans) {
      rv = nsClipboard::CreateNativeDataObject(trans,
                                               getter_AddRefs(itemToDrag),
                                               uri);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } // else dragging a single object

  // Before starting get the file name to monitor if there is any
  nsAutoString strData;         // holds kFilePromiseURLMime flavour data
  PRBool bDownload = PR_FALSE;  // Used after drag ends to fugure if the download should start

  nsCOMPtr<nsIURI> sourceURI;
  nsCOMPtr<nsISupports> urlPrimitive;
  nsCOMPtr<nsISupports> transSupports;
  anArrayTransferables->GetElementAt(0, getter_AddRefs(transSupports));
  nsCOMPtr<nsITransferable> trans = (do_QueryInterface(transSupports));
  if (trans) {
    // Get the filename form the kFilePromiseURLFlavour
    nsAutoString wideFileName;
    // if this fails there is no kFilePromiseMime flavour or no filename
    rv = nsDataObj::GetDownloadDetails(trans,
                                       getter_AddRefs(sourceURI),
                                       wideFileName);
    if (SUCCEEDED(rv))
    {
      // Start listening to the shell notifications
      if (StartWatchingShell(wideFileName)) {
        // Set download flag if all went ok so far
        bDownload = PR_TRUE;
      }
    }
  }

  // Kick off the native drag session
  rv = StartInvokingDragSession(itemToDrag, aActionType);

  // Check if the download flag was set
  // if not then we are done
  if (!bDownload)
    return rv;

  if (NS_FAILED(rv))  // See if dragsession failed
  {
    ::DestroyWindow(mHiddenWnd);
    return rv;
  }

  // Construct target URI from nsILocalFile
  // Init file for the download
  nsCOMPtr<nsILocalFile> dropLocalFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  if (!dropLocalFile)
  {
    ::DestroyWindow(mHiddenWnd);
    return NS_ERROR_FAILURE;
  }

  // init with path we get from GetDropPath
  // if it fails then there is no target path.
  // This could happen if the drag operation was cancelled.
  nsAutoString fileName;
  if (!GetDropPath(fileName))
    return NS_OK;

  // hidden window is destroyed by now
  rv = dropLocalFile->InitWithPath(fileName);
  if (NS_FAILED(rv))
    return rv;

  // set the directory promise flavour
  // if this fails it won't affect the drag so we can just continue
  trans->SetTransferData(kFilePromiseDirectoryMime,
                         dropLocalFile,
                         sizeof(nsILocalFile*));

  // Create a new FileURI to pass to the nsIDownload
  nsCOMPtr<nsIURI> targetURI;

  nsCOMPtr<nsIFile> file = do_QueryInterface(dropLocalFile);
  if (!file)
    return NS_ERROR_FAILURE;

  rv = NS_NewFileURI(getter_AddRefs(targetURI), file);
  if (NS_FAILED(rv))
    return rv;

  // Start download
  nsCOMPtr<nsISupports> fileAsSupports = do_QueryInterface(dropLocalFile);

  nsCOMPtr<nsIWebBrowserPersist> persist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsITransfer> transfer = do_CreateInstance("@mozilla.org/transfer;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(transfer);
  rv = persist->SetProgressListener(listener);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsICancelable> cancelable = do_QueryInterface(persist);
  rv = transfer->Init(sourceURI, targetURI, NS_LITERAL_STRING(""), nsnull, PR_Now(), nsnull, cancelable);
  if (NS_FAILED(rv))
    return rv;

  rv = persist->SaveURI(sourceURI, nsnull, nsnull, nsnull, nsnull, fileAsSupports);
  if (NS_FAILED(rv))
    return rv;

  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::StartInvokingDragSession(IDataObject * aDataObj,
                                        PRUint32 aActionType)
{
  // To do the drag we need to create an object that
  // implements the IDataObject interface (for OLE)
  NS_IF_RELEASE(mNativeDragSrc);
  mNativeDragSrc = (IDropSource *)new nsNativeDragSource();
  if (!mNativeDragSrc)
    return NS_ERROR_OUT_OF_MEMORY;

  mNativeDragSrc->AddRef();

  // Now figure out what the native drag effect should be
  DWORD dropRes;
  DWORD effects = DROPEFFECT_SCROLL;
  if (aActionType & DRAGDROP_ACTION_COPY) {
    effects |= DROPEFFECT_COPY;
  }
  if (aActionType & DRAGDROP_ACTION_MOVE) {
    effects |= DROPEFFECT_MOVE;
  }
  if (aActionType & DRAGDROP_ACTION_LINK) {
    effects |= DROPEFFECT_LINK;
  }

  // XXX not sure why we bother to cache this, it can change during
  // the drag
  mDragAction = aActionType;
  mDoingDrag  = PR_TRUE;

  // Start dragging
  StartDragSession();

  // Call the native D&D method
  HRESULT res = ::DoDragDrop(aDataObj, mNativeDragSrc, effects, &dropRes);

  // We're done dragging
  EndDragSession();

  // For some drag/drop interactions, IDataObject::SetData doesn't get
  // called with a CFSTR_PERFORMEDDROPEFFECT format and the
  // intermediate file (if it was created) isn't deleted.  See
  // http://bugzilla.mozilla.org/show_bug.cgi?id=203847#c4 for a
  // detailed description of the different cases.  Now that we know
  // that the drag/drop operation has ended, call SetData() so that
  // the intermediate file is deleted.
  static CLIPFORMAT PerformedDropEffect =
    ::RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);

  FORMATETC fmte =
    {
      (CLIPFORMAT)PerformedDropEffect,
      NULL,
      DVASPECT_CONTENT,
      -1,
      TYMED_NULL
    };

  STGMEDIUM medium;
  medium.tymed = TYMED_NULL;
  medium.pUnkForRelease = NULL;
  aDataObj->SetData(&fmte, &medium, FALSE);

  mDoingDrag = PR_FALSE;

  return DRAGDROP_S_DROP == res ? NS_OK : NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
// Make Sure we have the right kind of object
nsDataObjCollection*
nsDragService::GetDataObjCollection(IDataObject* aDataObj)
{
  nsDataObjCollection * dataObjCol = nsnull;
  if (aDataObj) {
    nsIDataObjCollection* dataObj;
    if (aDataObj->QueryInterface(IID_IDataObjCollection,
                                 (void**)&dataObj) == S_OK) {
      dataObjCol = NS_STATIC_CAST(nsDataObjCollection*, aDataObj);
      dataObj->Release();
    }
  }

  return dataObjCol;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::GetNumDropItems(PRUint32 * aNumItems)
{
  if (!mDataObject) {
    *aNumItems = 0;
    return NS_OK;
  }

  if (IsCollectionObject(mDataObject)) {
    nsDataObjCollection * dataObjCol = GetDataObjCollection(mDataObject);
    if (dataObjCol)
      *aNumItems = dataObjCol->GetNumDataObjects();
  }
  else {
    // Next check if we have a file drop. Return the number of files in
    // the file drop as the number of items we have, pretending like we
    // actually have > 1 drag item.
    FORMATETC fe2;
    SET_FORMATETC(fe2, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
    if (mDataObject->QueryGetData(&fe2) == S_OK) {
      STGMEDIUM stm;
      if (mDataObject->GetData(&fe2, &stm) == S_OK) {
        HDROP hdrop = (HDROP)GlobalLock(stm.hGlobal);
        *aNumItems = ::DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
        ::GlobalUnlock(stm.hGlobal);
        ::ReleaseStgMedium(&stm);
      }
    }
    else
      *aNumItems = 1;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::GetData(nsITransferable * aTransferable, PRUint32 anItem)
{
  // This typcially happens on a drop, the target would be asking
  // for it's transferable to be filled in
  // Use a static clipboard utility method for this
  if (!mDataObject)
    return NS_ERROR_FAILURE;

  nsresult dataFound = NS_ERROR_FAILURE;

  if (IsCollectionObject(mDataObject)) {
    // multiple items, use |anItem| as an index into our collection
    nsDataObjCollection * dataObjCol = GetDataObjCollection(mDataObject);
    PRUint32 cnt = dataObjCol->GetNumDataObjects();
    if (anItem >= 0 && anItem < cnt) {
      IDataObject * dataObj = dataObjCol->GetDataObjectAt(anItem);
      dataFound = nsClipboard::GetDataFromDataObject(dataObj, 0, nsnull,
                                                     aTransferable);
    }
    else
      NS_WARNING("Index out of range!");
  }
  else {
    // If they are asking for item "0", we can just get it...
    if (anItem == 0) {
       dataFound = nsClipboard::GetDataFromDataObject(mDataObject, anItem,
                                                      nsnull, aTransferable);
    } else {
      // It better be a file drop, or else non-zero indexes are invalid!
      FORMATETC fe2;
      SET_FORMATETC(fe2, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
      if (mDataObject->QueryGetData(&fe2) == S_OK)
        dataFound = nsClipboard::GetDataFromDataObject(mDataObject, anItem,
                                                       nsnull, aTransferable);
      else
        NS_WARNING("Reqesting non-zero index, but clipboard data is not a collection!");
    }
  }
  return dataFound;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsDragService::SetIDataObject(IDataObject * aDataObj)
{
  // When the native drag starts the DragService gets
  // the IDataObject that is being dragged
  NS_IF_RELEASE(mDataObject);
  mDataObject = aDataObj;
  NS_IF_ADDREF(mDataObject);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if (!aDataFlavor || !mDataObject || !_retval)
    return NS_ERROR_FAILURE;

#ifdef NS_DEBUG
  if (strcmp(aDataFlavor, kTextMime) == 0)
    NS_WARNING("DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD");
#endif

  *_retval = PR_FALSE;

  FORMATETC fe;
  UINT format = 0;

  if (IsCollectionObject(mDataObject)) {
    // We know we have one of our special collection objects.
    format = nsClipboard::GetFormat(aDataFlavor);
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1,
                  TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);

    // See if any one of the IDataObjects in the collection supports
    // this data type
    nsDataObjCollection* dataObjCol = GetDataObjCollection(mDataObject);
    if (dataObjCol) {
      PRUint32 cnt = dataObjCol->GetNumDataObjects();
      for (PRUint32 i=0;i<cnt;++i) {
        IDataObject * dataObj = dataObjCol->GetDataObjectAt(i);
        if (S_OK == dataObj->QueryGetData(&fe))
          *_retval = PR_TRUE;             // found it!
      }
    }
  } // if special collection object
  else {
    // Ok, so we have a single object. Check to see if has the correct
    // data type. Since this can come from an outside app, we also
    // need to see if we need to perform text->unicode conversion if
    // the client asked for unicode and it wasn't available.
    format = nsClipboard::GetFormat(aDataFlavor);
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1,
                  TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
    if (mDataObject->QueryGetData(&fe) == S_OK)
      *_retval = PR_TRUE;                 // found it!
    else {
      // We haven't found the exact flavor the client asked for, but
      // maybe we can still find it from something else that's on the
      // clipboard
      if (strcmp(aDataFlavor, kUnicodeMime) == 0) {
        // client asked for unicode and it wasn't present, check if we
        // have CF_TEXT.  We'll handle the actual data substitution in
        // the data object.
        format = nsClipboard::GetFormat(kTextMime);
        SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1,
                      TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
        if (mDataObject->QueryGetData(&fe) == S_OK)
          *_retval = PR_TRUE;                 // found it!
      }
      else if (strcmp(aDataFlavor, kURLMime) == 0) {
        // client asked for a url and it wasn't present, but if we
        // have a file, then we have a URL to give them (the path, or
        // the internal URL if an InternetShortcut).
        format = nsClipboard::GetFormat(kFileMime);
        SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1,
                      TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
        if (mDataObject->QueryGetData(&fe) == S_OK)
          *_retval = PR_TRUE;                 // found it!
      }
    } // else try again
  }

  return NS_OK;
}


//
// IsCollectionObject
//
// Determine if this is a single |IDataObject| or one of our private
// collection objects. We know the difference because our collection
// object will respond to supporting the private |MULTI_MIME| format.
//
PRBool
nsDragService::IsCollectionObject(IDataObject* inDataObj)
{
  PRBool isCollection = PR_FALSE;

  // setup the format object to ask for the MULTI_MIME format. We only
  // need to do this once
  static UINT sFormat = 0;
  static FORMATETC sFE;
  if (!sFormat) {
    sFormat = nsClipboard::GetFormat(MULTI_MIME);
    SET_FORMATETC(sFE, sFormat, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  }

  // ask the object if it supports it. If yes, we have a collection
  // object
  if (inDataObj->QueryGetData(&sFE) == S_OK)
    isCollection = PR_TRUE;

  return isCollection;

} // IsCollectionObject


//
// EndDragSession
//
// Override the default to make sure that we release the data object
// when the drag ends. It seems that OLE doesn't like to let apps quit
// w/out crashing when we're still holding onto their data
//
NS_IMETHODIMP
nsDragService::EndDragSession()
{
  nsBaseDragService::EndDragSession();
  NS_IF_RELEASE(mDataObject);

  return NS_OK;
}

// Start monitoring shell events - when user drops we'll get a notification from shell
// containing drop location
// aFileName is a filename to monitor
// returns true if all went ok 
// if hidden window was created and shell is watching out for files being created
PRBool nsDragService::StartWatchingShell(const nsAString& aFileName)
{
  // init member varable with the file name to watch
  if (mFileName)
    NS_Free(mFileName);

  mFileName = ToNewUnicode(aFileName);

  // Create hidden window to process shell notifications
  WNDCLASS wc;

  wc.style          = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc    = (WNDPROC)HiddenWndProc;
  wc.cbClsExtra     = 0;
  wc.cbWndExtra     = 0;
  wc.hInstance      = NULL;
  wc.hIcon          = NULL;
  wc.hCursor        = NULL;
  wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszMenuName   = NULL;
  wc.lpszClassName  = TEXT("DHHidden");

  unsigned short res = RegisterClass(&wc);

  mHiddenWnd = CreateWindowEx(WS_EX_TOOLWINDOW,
                              TEXT("DHHidden"),
                              TEXT("DHHidden"),
                              WS_POPUP,
                              0,
                              0,
                              100,
                              100,
                              NULL,
                              NULL,
                              NULL,
                              NULL);
  if (mHiddenWnd == NULL)
    return PR_FALSE;

  // Now let the explorer know what we want
  // Get My Computer PIDL
  LPITEMIDLIST pidl;
  HRESULT hr = ::SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &pidl);

  if (SUCCEEDED(hr)) {
    SHChangeNotifyStruct notify;
    notify.pidl = pidl;
    notify.fRecursive = TRUE;

    HMODULE hShell32 = ::GetModuleHandleA("SHELL32.DLL");

    if (hShell32 == NULL) {
      ::DestroyWindow(mHiddenWnd);
      return PR_FALSE;
    }

  // Have to use ordinals in the call to GetProcAddress instead of function names.
  // The reason for this is because on some older systems (prior to WinXP)
  // this function has no name in shell32.dll.
  DWORD dwOrdinal = 2;
  // get reg func
  SHCNRegPtr reg;
  reg = (SHCNRegPtr)::GetProcAddress(hShell32, (LPCSTR)dwOrdinal);
  if (reg == NULL) {
    ::DestroyWindow(mHiddenWnd);
    return PR_FALSE;
  }

  mNotifyHandle = (reg)(mHiddenWnd,
                        0x2,
                        0x1,
                        WM_USER,
                        1,
                        &notify);
  }

  // destroy hidden window if failed to register notification handle
  if (mNotifyHandle == 0) {
    ::DestroyWindow(mHiddenWnd);
  }

  return (mNotifyHandle != 0);
}

// Get the drop path if there is one:)
// returns true if there is a drop path
PRBool nsDragService::GetDropPath(nsAString& aDropPath) const
{
  // we failed to create window so there is no drop path
  if (mHiddenWnd == NULL)
    return PR_FALSE;

  // If we get 0 for this that means we failed to register notification callback
  if (mNotifyHandle == 0)
    return PR_FALSE;

  // Do clean up stuff (reverses all that's been done in Start)
  HMODULE hShell32 = ::GetModuleHandleA("SHELL32.DLL");

  if (hShell32 == NULL)
    return PR_FALSE;

  // Have to use ordinals in the call to GetProcAddress instead of function names.
  // The reason for this is because on some older systems (prior to WinXP)
  // this function has no name in shell32.dll.
  DWORD dwOrdinal = 4;
  SHCNDeregPtr dereg;
  dereg = (SHCNDeregPtr)::GetProcAddress(hShell32, (LPCSTR)dwOrdinal);
  if (dereg == NULL)
      return PR_FALSE;
  (dereg)(mNotifyHandle);

  ::DestroyWindow(mHiddenWnd);

  // if drop path is too short then there is no drop location
  if (!mDropPath)
    return PR_FALSE;

  aDropPath.Assign(mDropPath);

  return PR_TRUE;
}

// Hidden window procedure - this is where shell sends notifications
// When notification is received, perform some checking and copy path to the member variable
LRESULT WINAPI nsDragService::HiddenWndProc(HWND aWnd, UINT aMsg, WPARAM awParam, LPARAM alParam)
{
  switch (aMsg) {
  case WM_USER:
    if (alParam == SHCNE_RENAMEITEM) {
      // we got notification from shell
      // as wParam we get 2 PIDL
      LPCITEMIDLIST* ppidl = (LPCITEMIDLIST*)awParam;
      // Get From path (where the file is comming from)
      WCHAR szPathFrom[MAX_PATH + 1];
      WCHAR szPathTo[MAX_PATH + 1];
      ::SHGetPathFromIDListW(ppidl[0], szPathFrom);
      // Get To path (where the file is going to)
      ::SHGetPathFromIDListW(ppidl[1], szPathTo);

      // first is from where the file is coming
      // and the second is where the file is going
      nsAutoString pathFrom(szPathFrom);  // where the file is comming from
      nsAutoString pathTo(szPathTo);      // where the file is going to
      // Get OS Temp directory
      // Get system temp directory
      nsresult rv;
      nsCOMPtr<nsILocalFile> tempDir;
      nsCOMPtr<nsIProperties> directoryService = 
          do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
      if (!directoryService || NS_FAILED(rv))
          return ResultFromScode(E_FAIL);
      directoryService->Get(NS_OS_TEMP_DIR, 
                            NS_GET_IID(nsIFile), 
                            getter_AddRefs(tempDir));

      // append file name to Temp directory string
      nsAutoString tempPath;
      tempDir->Append(nsDependentString(mFileName));
      tempDir->GetPath(tempPath);

      // Now check if there is our filename in the path
      // and also check for the source directory - it should be OS Temp dir
      // this way we can ensure that this is the file that we need
      PRInt32 pathToLength = pathTo.Length();
      if (Substring(pathTo, pathToLength - NS_strlen(mFileName), pathToLength).Equals(mFileName) &&
          tempPath.Equals(pathFrom, nsCaseInsensitiveStringComparator()))
      {
        // This is what we wanted to get
        if (mDropPath)
          NS_Free(mDropPath);

        mDropPath = ToNewUnicode(pathTo);
      }
      return 0;
    }
    break;
  }

  return ::DefWindowProc(aWnd, aMsg, awParam, alParam);
}
