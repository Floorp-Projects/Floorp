/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rich Walsh <dragtext@e-vertise.com>
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

#include "nsDragService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIWebBrowserPersist.h"
#include "nsILocalFile.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsOS2Uni.h"

// --------------------------------------------------------------------------
// helper functions

void    WriteTypeEA(const char *filename, const char *type);
int     UnicodeToCodepage( const nsAString& inString, char **outText);

// --------------------------------------------------------------------------

// limit URL object titles to a reasonable length
#define MAXTITLELTH 31
#define TITLESEPARATOR (L' ')

#ifndef DC_PREPAREITEM
#define DC_PREPAREITEM 0x0040;
#endif

// --------------------------------------------------------------------------

nsDragService::nsDragService()
{
  /* member initializers and constructor code */
   mDragWnd = WinCreateWindow( HWND_DESKTOP, WC_STATIC, 0, 0, 0, 0, 0, 0,
                               HWND_DESKTOP, HWND_BOTTOM, 0, 0, 0);
   WinSubclassWindow( mDragWnd, nsDragWindowProc);
}

// --------------------------------------------------------------------------

MRESULT EXPENTRY nsDragWindowProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  PDRAGTRANSFER pdragtransfer;
  FILE *fp;
  CHAR chPath[CCHMAXPATH];
  nsDragService* dragservice;

  switch (msg) {

    // if the user requests the contents of a URL be rendered (vs the URL
    // itself), change the suggested target name from the URL's title to
    // the name of the file that will be retrieved
    case DM_RENDERPREPARE: {
      pdragtransfer = (PDRAGTRANSFER)mp1;
      dragservice = (nsDragService*)pdragtransfer->pditem->ulItemID;
  
      if (pdragtransfer->usOperation == DO_COPY &&
          !strcmp(dragservice->mMimeType, kURLMime)) {
        nsCOMPtr<nsIURL> urlObject(do_QueryInterface(dragservice->mSourceData));
        if (urlObject) {
          nsCAutoString filename;
          urlObject->GetFileName(filename);
          if (filename.IsEmpty()) {
            urlObject->GetHost(filename);
            filename.Append("/file");
          }
          DrgDeleteStrHandle(pdragtransfer->pditem->hstrTargetName);
          pdragtransfer->pditem->hstrTargetName = DrgAddStrHandle(ToNewCString(filename));
        }
      }
      return (MRESULT)TRUE;
    }
  
    case DM_RENDER: {
      pdragtransfer = (PDRAGTRANSFER)mp1;
      dragservice = (nsDragService*)pdragtransfer->pditem->ulItemID;
      DrgQueryStrName(pdragtransfer->hstrRenderToName, CCHMAXPATH, chPath);
      BOOL ok = FALSE;
  
      // if we're dragging a URL, render either the URL itself or the file
      // it references if the user Ctrl-dropped;  use the nsIURL interface
      // to determine if the latter is even possible - if it fails (as it
      // will for mailto:), drop into the code that uses nsIURI to render
      // a URL object
      
      if (!strcmp(dragservice->mMimeType, kURLMime)) {
        if (pdragtransfer->usOperation == DO_COPY) {
          nsCOMPtr<nsIURL> urlObject(do_QueryInterface(dragservice->mSourceData));
          if (urlObject) {
            nsCAutoString strUrl;
            urlObject->GetSpec(strUrl);
            dragservice->WriteData(chPath, strUrl.get());
            ok = TRUE;
          }
        }
        if (!ok) {
          nsCOMPtr<nsIURI> uriObject(do_QueryInterface(dragservice->mSourceData));
          if (uriObject) {
            nsCAutoString strUri;
            uriObject->GetSpec(strUri);
            fp = fopen(chPath, "wb+");
            fwrite(strUri.get(), strUri.Length(), 1, fp);
            fclose(fp);
            WriteTypeEA(chPath, "UniformResourceLocator");
            ok = TRUE;
          }
        }
      }
      else
        // if we're dragging text, do NLS conversion then write it to file
        if (!strcmp(dragservice->mMimeType, kUnicodeMime)) {
          nsCOMPtr<nsISupportsString> strObject(do_QueryInterface(dragservice->mSourceData));
          if (strObject) {
            nsAutoString strData;
            strObject->GetData(strData);
            char * pText;
            int cnt = UnicodeToCodepage( strData, &pText);
            if (cnt) {
              fp = fopen(chPath, "wb+");
              fwrite(pText, cnt, 1, fp);
              fclose(fp);
              ok = TRUE;
            }
          }
        }
  
      DrgPostTransferMsg(pdragtransfer->hwndClient, DM_RENDERCOMPLETE, pdragtransfer,
                         (ok ? DMFL_RENDEROK : DMFL_RENDERFAIL), 0, TRUE);
      DrgFreeDragtransfer(pdragtransfer);
      return (MRESULT)TRUE;
    }
  
    default:
      break;
  }

  return ::WinDefWindowProc(hWnd, msg, mp1, mp2);
}

// --------------------------------------------------------------------------

nsDragService::~nsDragService()
{
  /* destructor code */
  WinDestroyWindow(mDragWnd);
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode, nsISupportsArray *aTransferables, nsIScriptableRegion *aRegion, PRUint32 aActionType)
{
  // Utter hack for drag drop - provide a way for nsWindow to set
  // mSourceDataItems & to clear mSourceNode so we can identify native drags
  if (!aDOMNode && !aRegion && !aActionType) {
    mSourceDataItems = aTransferables;
    mSourceNode = 0;
    return NS_OK;
  }
  nsBaseDragService::InvokeDragSession ( aDOMNode, aTransferables, aRegion, aActionType );

  // set our reference to the transferables.  this will also addref
  // the transferables since we're going to hang onto this beyond the
  // length of this call
  mSourceDataItems = aTransferables;

  WinSetCapture(HWND_DESKTOP, NULLHANDLE);

  // Assume we are only dragging one thing for now
  PDRAGINFO pDragInfo = DrgAllocDraginfo(1);
  if (!pDragInfo)
    return NS_ERROR_UNEXPECTED;

  pDragInfo->usOperation = DO_DEFAULT;

  DRAGITEM dragitem;
  dragitem.hwndItem            = mDragWnd;
  dragitem.ulItemID            = (ULONG)this;
  dragitem.fsControl           = DC_OPEN;
  dragitem.cxOffset            = 2;
  dragitem.cyOffset            = 2;
  dragitem.fsSupportedOps      = DO_COPYABLE|DO_MOVEABLE|DO_LINKABLE;

  // since there is no source file, leave these "blank"
  dragitem.hstrContainerName   = NULLHANDLE;
  dragitem.hstrSourceName      = NULLHANDLE;

  nsresult rv = NS_ERROR_FAILURE;

  // reduce our footprint by ensuring any intermediate objects
  // go out of scope before the drag begins
  {
    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> transItem (do_QueryInterface(genericItem));

    nsCOMPtr<nsISupports> genericData;
    PRUint32 len = 0;
    char * targetName = 0;

    // see if we have a URL or text;  if so, the title method
    // will save the data and mimetype for use with a native drop

    if (NS_SUCCEEDED(transItem->GetTransferData(kURLMime,
                              getter_AddRefs(genericData), &len))) {
      rv = GetUrlAndTitle( genericData, &targetName);
      if (NS_SUCCEEDED(rv)) {
        // advise PM that we need a DM_RENDERPREPARE msg
        // *before* it composes a render-to filename
        dragitem.fsControl     |= DC_PREPAREITEM;
        dragitem.hstrType       = DrgAddStrHandle("UniformResourceLocator");
        dragitem.hstrRMF        = DrgAddStrHandle("<DRM_OS2FILE,DRF_UNKNOWN>");
        dragitem.hstrTargetName = DrgAddStrHandle(targetName);
        nsMemory::Free(targetName);
      }
    }
    else
    if (NS_SUCCEEDED(transItem->GetTransferData(kUnicodeMime,
                                getter_AddRefs(genericData), &len))) {
      rv = GetUniTextTitle( genericData, &targetName);
      if (NS_SUCCEEDED(rv)) {
        dragitem.hstrType       = DrgAddStrHandle("Plain Text");
        dragitem.hstrRMF        = DrgAddStrHandle("<DRM_OS2FILE,DRF_TEXT>");
        dragitem.hstrTargetName = DrgAddStrHandle(targetName);
        nsMemory::Free(targetName);
      }
    }
  }

  // if neither URL nor text are available, make this a Moz-only drag
  // by making it unidentifiable to native apps
  if (NS_FAILED(rv)) {
    mMimeType = 0;
    dragitem.hstrType       = DrgAddStrHandle("Unknown");
    dragitem.hstrRMF        = DrgAddStrHandle("<DRM_UNKNOWN,DRF_UNKNOWN>");
    dragitem.hstrTargetName = NULLHANDLE;
  }
  DrgSetDragitem(pDragInfo, &dragitem, sizeof(DRAGITEM), 0);

  DRAGIMAGE dragimage;
  memset(&dragimage, 0, sizeof(DRAGIMAGE));
  dragimage.cb = sizeof(DRAGIMAGE);
  dragimage.hImage = WinQuerySysPointer(HWND_DESKTOP, SPTR_FILE, FALSE);
  dragimage.fl = DRG_ICON;
    
  mDoingDrag = PR_TRUE;
  HWND hwndDest = DrgDrag(mDragWnd, pDragInfo, &dragimage, 1, VK_BUTTON2,
                  (void*)0x80000000L); // Don't lock the desktop PS
  mDoingDrag = PR_FALSE;

  // do clean up;  if the drop completed, the target will delete
  // the string handles
  if (hwndDest == 0)
      DrgDeleteDraginfoStrHandles(pDragInfo);
  DrgFreeDraginfo(pDragInfo);

  mSourceNode = 0;
  mSourceDataItems = 0;
  mSourceData = 0;
  mMimeType = 0;

  return NS_OK;
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::GetNumDropItems(PRUint32 *aNumDropItems)
{
  mSourceDataItems->Count(aNumDropItems);
  return NS_OK;
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::GetData(nsITransferable *aTransferable, PRUint32 aItemIndex)
{
  // make sure that we have a transferable
  if (!aTransferable)
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including
  // ones obtained through conversion). Flavors are nsISupportsCStrings
  // so that they can be seen from JS.
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> flavorList;
  rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return rv;

  // count the number of flavors
  PRUint32 cnt;
  flavorList->Count (&cnt);

  for (unsigned int i= 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericWrapper;
    flavorList->GetElementAt(i, getter_AddRefs(genericWrapper));
    nsCOMPtr<nsISupportsCString> currentFlavor;
    currentFlavor = do_QueryInterface(genericWrapper);
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
  
      nsCOMPtr<nsISupports> genericItem;
  
      mSourceDataItems->GetElementAt(aItemIndex, getter_AddRefs(genericItem));
      nsCOMPtr<nsITransferable> item (do_QueryInterface(genericItem));
      if (item) {
        nsCOMPtr<nsISupports> data;
        PRUint32 tmpDataLen = 0;
        rv = item->GetTransferData(flavorStr, getter_AddRefs(data), &tmpDataLen);
        if (NS_SUCCEEDED(rv)) {
          rv = aTransferable->SetTransferData(flavorStr, data, tmpDataLen);
          break;
        }
      }
    }
  }
  return rv;
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  // set this to no by default
  *_retval = PR_FALSE;

  PRUint32 numDragItems = 0;
  // if we don't have mDataItems we didn't start this drag so it's
  // an external client trying to fool us.
  if (!mSourceDataItems)
    return NS_OK;
  mSourceDataItems->Count(&numDragItems);
  for (PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
    if (currItem) {
      nsCOMPtr <nsISupportsArray> flavorList;
      currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
      if (flavorList) {
        PRUint32 numFlavors;
        flavorList->Count( &numFlavors );
        for ( PRUint32 flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex ) {
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt (flavorIndex, getter_AddRefs(genericWrapper));
          nsCOMPtr<nsISupportsCString> currentFlavor;
          currentFlavor = do_QueryInterface(genericWrapper);
          if (currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString ( getter_Copies(flavorStr) );
            if (strcmp(flavorStr, aDataFlavor) == 0) {
              *_retval = PR_TRUE;
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

// --------------------------------------------------------------------------

BOOL nsDragService::WriteData(PSZ szDest, PCSZ szURL)
{
  FILE *fp;
  nsresult rv;
  nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv));
  nsCOMPtr<nsIURI> linkURI;
  rv = NS_NewURI(getter_AddRefs(linkURI), szURL);
  nsCOMPtr<nsILocalFile> file;
  NS_NewNativeLocalFile(nsDependentCString(szDest), TRUE, getter_AddRefs(file));

  nsCAutoString temp;
  file->GetNativePath(temp);
  fp = fopen(temp.get(), "wb+");
  fwrite("", 0, 1, fp);
  fclose(fp);
  nsCAutoString filename;
  nsCOMPtr<nsIURL> linkURL(do_QueryInterface(linkURI));
  linkURL->GetFileName(filename);
  if (filename.IsEmpty()) {
    /* If we don't have a filename, just mark it text/html */
    /* This can only be fixed if we write mime type on putting */
    /* any file to disk */
    WriteTypeEA(temp.get(), "text/html");
  }
  webPersist->SaveURI(linkURI, nsnull, nsnull, nsnull, nsnull, file);

  return TRUE;
}

// --------------------------------------------------------------------------

// Split a Moz Url/Title into its components, save the Url for use by
// a native drop, then compose a title.

nsresult  nsDragService::GetUrlAndTitle(nsISupports *aGenericData, char **aTargetName)
{
  // get the URL/title string
  nsCOMPtr<nsISupportsString> strObject ( do_QueryInterface(aGenericData));
  if (!strObject)
    return NS_ERROR_FAILURE;
  nsAutoString strData;
  strObject->GetData(strData);

  // split string into URL and Title - 
  // if there's a title but no URL, there's no reason to continue
  PRInt32 lineIndex = strData.FindChar ('\n');
  if (lineIndex == 0)
    return NS_ERROR_FAILURE;

  // get the URL portion of the text
  nsAutoString strUrl;
  if (lineIndex == -1)
    strUrl = strData;
  else
    strData.Left(strUrl, lineIndex);

  // save the URL for later use
  nsCOMPtr<nsIURI> saveURI;
  NS_NewURI(getter_AddRefs(saveURI), strUrl);
  if (!saveURI)
    return NS_ERROR_FAILURE;

  // if there's a bona-fide title & it isn't just a copy of the URL,
  // limit it to a reasonable length, perform NLS conversion, then return

  if (++lineIndex && lineIndex != (int)strData.Length() &&
      !strUrl.Equals(Substring(strData, lineIndex, strData.Length()))) {
    PRUint32 strLth = NS_MIN((int)strData.Length()-lineIndex, MAXTITLELTH);
    nsAutoString strTitle;
    strData.Mid(strTitle, lineIndex, strLth);
    if (!UnicodeToCodepage(strTitle, aTargetName))
      return NS_ERROR_FAILURE;

    mSourceData = do_QueryInterface(saveURI);
    mMimeType = kURLMime;
    return NS_OK;
  }

  // construct a title from the hostname & filename
  // (or the last directory in the path if no filename)

  nsCOMPtr<nsIURL> urlObj( do_QueryInterface(saveURI));
  if (!urlObj)
    return NS_ERROR_FAILURE;

  nsCAutoString strTitle;
  nsCAutoString strFile;

  urlObj->GetHost(strTitle);
  urlObj->GetFileName(strFile);
  if (!strFile.IsEmpty()) {
    strTitle.Append(NS_LITERAL_CSTRING("/"));
    strTitle.Append(strFile);
  }
  else {
    urlObj->GetDirectory(strFile);
    if (strFile.Length() > 1) {
      nsCAutoString::const_iterator start, end, curr;
      strFile.BeginReading(start);
      strFile.EndReading(end);
      strFile.EndReading(curr);
      for (curr.advance(-2); curr != start; --curr)
        if (*curr == '/')
          break;
      strTitle.Append(Substring(curr, end));
    }
  }
  *aTargetName = ToNewCString(strTitle);

  mSourceData = do_QueryInterface(saveURI);
  mMimeType = kURLMime;
  return NS_OK;
}

// --------------------------------------------------------------------------

// Construct a title for text drops from the leading words of the text.
// Alphanumeric characters are copied to the title;  sequences of
// non-alphanums are replaced by a single space

nsresult  nsDragService::GetUniTextTitle(nsISupports *aGenericData, char **aTargetName)
{
  // get the string
  nsCOMPtr<nsISupportsString> strObject ( do_QueryInterface(aGenericData));
  if (!strObject)
    return NS_ERROR_FAILURE;

  // alloc a buffer to hold the unicode title text
  int bufsize = (MAXTITLELTH+1)*2;
  PRUnichar * buffer = (PRUnichar*)nsMemory::Alloc(bufsize);
  if (!buffer)
    return NS_ERROR_FAILURE;

  nsAutoString strData;
  strObject->GetData(strData);
  nsAutoString::const_iterator start, end;
  strData.BeginReading(start);
  strData.EndReading(end);

  // skip over leading non-alphanumerics
  for( ; start != end; ++start)
    if (UniQueryChar( *start, CT_ALNUM))
      break;

  // move alphanumerics into the buffer & replace contiguous
  // non-alnums with a single separator character
  int ctr, sep;
  for (ctr=0, sep=0; start != end && ctr < MAXTITLELTH; ++start) {
    if (UniQueryChar( *start, CT_ALNUM)) {
      buffer[ctr] = *start;
      ctr++;
      sep = 0;
    }
    else
      if (!sep) {
        buffer[ctr] = TITLESEPARATOR;
        ctr++;
        sep = 1;
      }
  }
  // eliminate trailing separators & lone characters
  // orphaned when the title is truncated
  if (sep)
    ctr--;
  if (ctr >= MAXTITLELTH - sep && buffer[ctr-2] == TITLESEPARATOR)
    ctr -= 2;
  buffer[ctr] = 0;

  // if we ended up with no alnums, call the result "text";
  // otherwise, do NLS conversion
  if (!ctr) {
    *aTargetName = ToNewCString(NS_LITERAL_CSTRING("text"));
    ctr = 1;
  }
  else
    ctr = UnicodeToCodepage( nsDependentString(buffer), aTargetName);

  // free our buffer, then exit
  nsMemory::Free(buffer);

  if (!ctr)
  return NS_ERROR_FAILURE;

  mSourceData = aGenericData;
  mMimeType = kUnicodeMime;
  return NS_OK;
}

// --------------------------------------------------------------------------
// Helper functions
// --------------------------------------------------------------------------

void WriteTypeEA(const char *filename, const char *type)
{
// this struct combines an FEA2LIST, an FEA2, plus a custom struct
// needed to write a single .TYPE EA in the correct MVMT format;
// it may not be clever but it's easy on the eyes...

#pragma pack(1)
  struct {
    struct {
      ULONG   ulcbList;
      ULONG   uloNextEntryOffset;
      BYTE    bfEA;
      BYTE    bcbName;
      USHORT  uscbValue;
      char    chszName[sizeof(".TYPE")];
    } hdr;
    struct {
      USHORT  usEAType;
      USHORT  usCodePage;
      USHORT  usNumEntries;
      USHORT  usDataType;
      USHORT  usDataLth;
    } info;
    char    data[64];
  } ea;  
#pragma pack()

  USHORT lth = (USHORT)strlen(type);
  if (lth >= sizeof(ea.data))
    return;

  ea.hdr.ulcbList = sizeof(ea.hdr) + sizeof(ea.info) + lth;
  ea.hdr.uloNextEntryOffset = 0;
  ea.hdr.bfEA = 0;
  ea.hdr.bcbName = sizeof(".TYPE") - 1;
  ea.hdr.uscbValue = sizeof(ea.info) + lth;
  strcpy(ea.hdr.chszName, ".TYPE");

  ea.info.usEAType = EAT_MVMT;
  ea.info.usCodePage = 0;
  ea.info.usNumEntries = 1;
  ea.info.usDataType = EAT_ASCII;
  ea.info.usDataLth = lth;
  strcpy(ea.data, type);

  EAOP2 eaop2;
  eaop2.fpGEA2List = 0;
  eaop2.fpFEA2List = (PFEA2LIST)&ea;

  DosSetPathInfo(filename, FIL_QUERYEASIZE, &eaop2, sizeof(eaop2), 0);
  return;
}

// --------------------------------------------------------------------------

// to do:  this needs to take into account the current page's encoding
// if it is different than the PM codepage

int UnicodeToCodepage( const nsAString& inString, char **outText)
{

  int outlen = 0;
  int bufsize = inString.Length()*2 + 1;

  *outText = (char*)nsMemory::Alloc(bufsize);
  if (*outText) {
    outlen = ::WideCharToMultiByte( 0, PromiseFlatString(inString).get(),
                                    inString.Length(), *outText, bufsize);

    if (outlen)
      (*outText)[outlen] = '\0';
    else
      nsMemory::Free(*outText);
  }
  return outlen;
}

// --------------------------------------------------------------------------

