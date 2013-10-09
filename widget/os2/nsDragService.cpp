/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INCL_DOSMISC
#define INCL_DOSERRORS

#include "nsDragService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIWebBrowserPersist.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsOS2Uni.h"
#include "wdgtos2rc.h"
#include "nsILocalFileOS2.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include <algorithm>

// --------------------------------------------------------------------------
// Local defines

// undocumented(?)
#ifndef DC_PREPAREITEM
  #define DC_PREPAREITEM  0x0040
#endif

// limit URL object titles to a reasonable length
#define MAXTITLELTH 31
#define TITLESEPARATOR (L' ')

#define DTSHARE_NAME    "\\SHAREMEM\\MOZ_DND"
#define DTSHARE_RMF     "<DRM_DTSHARE, DRF_TEXT>"

#define OS2FILE_NAME    "MOZ_TGT.TMP"
#define OS2FILE_TXTRMF  "<DRM_OS2FILE, DRF_TEXT>"
#define OS2FILE_UNKRMF  "<DRM_OS2FILE, DRF_UNKNOWN>"

// not defined in the OS/2 toolkit headers
extern "C" {
APIRET APIENTRY DosQueryModFromEIP(HMODULE *phMod, ULONG *pObjNum,
                                   ULONG BuffLen,  PCHAR pBuff,
                                   ULONG *pOffset, ULONG Address);
}

// --------------------------------------------------------------------------
// Helper functions

nsresult RenderToOS2File( PDRAGITEM pditem, HWND hwnd);
nsresult RenderToOS2FileComplete(PDRAGTRANSFER pdxfer, USHORT usResult,
                                 bool content, char** outText);
nsresult RenderToDTShare( PDRAGITEM pditem, HWND hwnd);
nsresult RenderToDTShareComplete(PDRAGTRANSFER pdxfer, USHORT usResult,
                                 char** outText);
nsresult RequestRendering( PDRAGITEM pditem, HWND hwnd, PCSZ pRMF, PCSZ pName);
nsresult GetAtom( ATOM aAtom, char** outText);
nsresult GetFileName(PDRAGITEM pditem, char** outText);
nsresult GetFileContents(PCSZ pszPath, char** outText);
nsresult GetTempFileName(char** outText);
void     SaveTypeAndSource(nsIFile *file, nsIDOMDocument *domDoc,
                           PCSZ pszType);
int      UnicodeToCodepage( const nsAString& inString, char **outText);
int      CodepageToUnicode( const nsACString& inString, PRUnichar **outText);
void     RemoveCarriageReturns(char * pszText);
MRESULT EXPENTRY nsDragWindowProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// --------------------------------------------------------------------------
// Global data

static HPOINTER gPtrArray[IDC_DNDCOUNT];
static char *   gTempFile = 0;

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

nsDragService::nsDragService()
{
    // member initializers and constructor code
  mDragWnd = WinCreateWindow( HWND_DESKTOP, WC_STATIC, 0, 0, 0, 0, 0, 0,
                              HWND_DESKTOP, HWND_BOTTOM, 0, 0, 0);
  WinSubclassWindow( mDragWnd, nsDragWindowProc);

  HMODULE hModResources = NULLHANDLE;
  DosQueryModFromEIP(&hModResources, nullptr, 0, nullptr, nullptr,
                     (ULONG)&gPtrArray);
  for (int i = 0; i < IDC_DNDCOUNT; i++)
    gPtrArray[i] = ::WinLoadPointer(HWND_DESKTOP, hModResources, i+IDC_DNDBASE);
}

// --------------------------------------------------------------------------

nsDragService::~nsDragService()
{
    // destructor code
  WinDestroyWindow(mDragWnd);

  for (int i = 0; i < IDC_DNDCOUNT; i++) {
    WinDestroyPointer(gPtrArray[i]);
    gPtrArray[i] = 0;
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDragService, nsBaseDragService, nsIDragSessionOS2)

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                            nsISupportsArray *aTransferables, 
                                            nsIScriptableRegion *aRegion,
                                            uint32_t aActionType)
{
  if (mDoingDrag)
    return NS_ERROR_UNEXPECTED;

  nsresult rv = nsBaseDragService::InvokeDragSession(aDOMNode, aTransferables,
                                                     aRegion, aActionType);
  NS_ENSURE_SUCCESS(rv, rv);

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

  rv = NS_ERROR_FAILURE;
  ULONG idIcon = 0;

    // bracket this to reduce our footprint before the drag begins
  {
    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> transItem (do_QueryInterface(genericItem));

    nsCOMPtr<nsISupports> genericData;
    uint32_t len = 0;

      // see if we have a URL or text;  if so, the title method
      // will save the data and mimetype for use with a native drop

    if (NS_SUCCEEDED(transItem->GetTransferData(kURLMime,
                              getter_AddRefs(genericData), &len))) {
      nsXPIDLCString targetName;
      rv = GetUrlAndTitle( genericData, getter_Copies(targetName));
      if (NS_SUCCEEDED(rv)) {
        // advise PM that we need a DM_RENDERPREPARE msg
        // *before* it composes a render-to filename
        dragitem.fsControl     |= DC_PREPAREITEM;
        dragitem.hstrType       = DrgAddStrHandle("UniformResourceLocator");
        dragitem.hstrRMF        = DrgAddStrHandle("<DRM_OS2FILE,DRF_TEXT>");
        dragitem.hstrTargetName = DrgAddStrHandle(targetName.get());
        idIcon = IDC_DNDURL;
      }
    }
    else
    if (NS_SUCCEEDED(transItem->GetTransferData(kUnicodeMime,
                                getter_AddRefs(genericData), &len))) {
      nsXPIDLCString targetName;
      rv = GetUniTextTitle( genericData, getter_Copies(targetName));
      if (NS_SUCCEEDED(rv)) {
        dragitem.hstrType       = DrgAddStrHandle("Plain Text");
        dragitem.hstrRMF        = DrgAddStrHandle("<DRM_OS2FILE,DRF_TEXT>");
        dragitem.hstrTargetName = DrgAddStrHandle(targetName.get());
        idIcon = IDC_DNDTEXT;
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
  dragimage.fl = DRG_ICON;
  if (idIcon)
    dragimage.hImage = gPtrArray[idIcon-IDC_DNDBASE];
  if (dragimage.hImage) {
    dragimage.cyOffset = 8;
    dragimage.cxOffset = 2;
  }
  else
    dragimage.hImage  = WinQuerySysPointer(HWND_DESKTOP, SPTR_FILE, FALSE);
    
  mDoingDrag = true;
  LONG escState = WinGetKeyState(HWND_DESKTOP, VK_ESC) & 0x01;
  HWND hwndDest = DrgDrag(mDragWnd, pDragInfo, &dragimage, 1, VK_BUTTON2,
                  (void*)0x80000000L); // Don't lock the desktop PS

    // determine whether the drag ended because Escape was pressed
  if (hwndDest == 0 && (WinGetKeyState(HWND_DESKTOP, VK_ESC) & 0x01) != escState)
    mUserCancelled = true;
  FireDragEventAtSource(NS_DRAGDROP_END);
  mDoingDrag = false;

    // do clean up;  if the drop completed,
    // the target will delete the string handles
  if (hwndDest == 0)
      DrgDeleteDraginfoStrHandles(pDragInfo);
  DrgFreeDraginfo(pDragInfo);

    // reset nsDragService's members
  mSourceDataItems = 0;
  mSourceData = 0;
  mMimeType = 0;

    // reset nsBaseDragService's members
  mSourceDocument = nullptr;
  mSourceNode = nullptr;
  mSelection = nullptr;
  mDataTransfer = nullptr;
  mUserCancelled = false;
  mHasImage = false;
  mImage = nullptr;
  mImageX = 0;
  mImageY = 0;
  mScreenX = -1;
  mScreenY = -1;

  return NS_OK;
}

// --------------------------------------------------------------------------

MRESULT EXPENTRY nsDragWindowProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg) {

      // if the user requests the contents of a URL be rendered (vs the URL
      // itself), change the suggested target name from the URL's title to
      // the name of the file that will be retrieved
    case DM_RENDERPREPARE: {
      PDRAGTRANSFER  pdxfer = (PDRAGTRANSFER)mp1;
      nsDragService* dragservice = (nsDragService*)pdxfer->pditem->ulItemID;
  
      if (pdxfer->usOperation == DO_COPY &&
          (WinGetKeyState(HWND_DESKTOP, VK_CTRL) & 0x8000) &&
          !strcmp(dragservice->mMimeType, kURLMime)) {
          // QI'ing nsIURL will fail for mailto: and the like
        nsCOMPtr<nsIURL> urlObject(do_QueryInterface(dragservice->mSourceData));
        if (urlObject) {
          nsAutoCString filename;
          urlObject->GetFileName(filename);
          if (filename.IsEmpty()) {
            urlObject->GetHost(filename);
            filename.Append("/file");
          }
          DrgDeleteStrHandle(pdxfer->pditem->hstrTargetName);
          pdxfer->pditem->hstrTargetName = DrgAddStrHandle(filename.get());
        }
      }
      return (MRESULT)TRUE;
    }
  
    case DM_RENDER: {
      nsresult       rv = NS_ERROR_FAILURE;
      PDRAGTRANSFER  pdxfer = (PDRAGTRANSFER)mp1;
      nsDragService* dragservice = (nsDragService*)pdxfer->pditem->ulItemID;
      char           chPath[CCHMAXPATH];

      DrgQueryStrName(pdxfer->hstrRenderToName, CCHMAXPATH, chPath);

        // if the user Ctrl-dropped a URL, use the nsIURL interface
        // to determine if it points to content (i.e. a file);  if so,
        // fetch its contents; if not (e.g. a 'mailto:' url), drop into
        // the code that uses nsIURI to render a URL object

      if (!strcmp(dragservice->mMimeType, kURLMime)) {
        if (pdxfer->usOperation == DO_COPY &&
            (WinGetKeyState(HWND_DESKTOP, VK_CTRL) & 0x8000)) {
          nsCOMPtr<nsIURL> urlObject(do_QueryInterface(dragservice->mSourceData));
          if (urlObject)
            rv = dragservice->SaveAsContents(chPath, urlObject);
        }
        if (!NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIURI> uriObject(do_QueryInterface(dragservice->mSourceData));
          if (uriObject)
            rv = dragservice->SaveAsURL(chPath, uriObject);
        }
      }
      else
          // if we're dragging text, do NLS conversion then write it to file
        if (!strcmp(dragservice->mMimeType, kUnicodeMime)) {
          nsCOMPtr<nsISupportsString> strObject(
                                 do_QueryInterface(dragservice->mSourceData));
          if (strObject)
            rv = dragservice->SaveAsText(chPath, strObject);
        }
  
      DrgPostTransferMsg(pdxfer->hwndClient, DM_RENDERCOMPLETE, pdxfer,
                         (NS_SUCCEEDED(rv) ? DMFL_RENDEROK : DMFL_RENDERFAIL),
                         0, TRUE);
      DrgFreeDragtransfer(pdxfer);
      return (MRESULT)TRUE;
    }

      // we don't use these msgs but neither does WinDefWindowProc()
    case DM_DRAGOVERNOTIFY:
    case DM_ENDCONVERSATION:
      return 0;
  
    default:
      break;
  }

  return ::WinDefWindowProc(hWnd, msg, mp1, mp2);
}

//-------------------------------------------------------------------------

// if the versions of Start & EndDragSession in nsBaseDragService
// were called (and they shouldn't be), they'd break nsIDragSessionOS2;
// they're overridden here and turned into no-ops to prevent this

NS_IMETHODIMP nsDragService::StartDragSession()
{
  NS_ERROR("OS/2 version of StartDragSession() should never be called!");
  return NS_OK;
}

NS_IMETHODIMP nsDragService::EndDragSession(bool aDragDone)
{
  NS_ERROR("OS/2 version of EndDragSession() should never be called!");
  return NS_OK;
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::GetNumDropItems(uint32_t *aNumDropItems)
{
  if (mSourceDataItems)
    mSourceDataItems->Count(aNumDropItems);
  else
    *aNumDropItems = 0;

  return NS_OK;
}

// --------------------------------------------------------------------------

NS_IMETHODIMP nsDragService::GetData(nsITransferable *aTransferable,
                                     uint32_t aItemIndex)
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
  uint32_t cnt;
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
        uint32_t tmpDataLen = 0;
        rv = item->GetTransferData(flavorStr, getter_AddRefs(data),
                                   &tmpDataLen);
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

// This returns true if any of the dragged items support a specified data
// flavor.  This doesn't make a lot of sense when dragging multiple items:
// all of them ought to match.  OTOH, Moz doesn't support multiple drag
// items so no problems arise.  If they do, use the commented-out code to
// switch from "any" to "all".

NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor,
                                                   bool *_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  *_retval = false;

  uint32_t numDragItems = 0;
  if (mSourceDataItems)
    mSourceDataItems->Count(&numDragItems);
  if (!numDragItems)
    return NS_OK;

// return true if all items support this flavor
//  for (uint32_t itemIndex = 0, *_retval = true;
//       itemIndex < numDragItems && *_retval; ++itemIndex) {
//    *_retval = false;

// return true if any item supports this flavor
  for (uint32_t itemIndex = 0;
       itemIndex < numDragItems && !(*_retval); ++itemIndex) {

    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));

    if (currItem) {
      nsCOMPtr <nsISupportsArray> flavorList;
      currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));

      if (flavorList) {
        uint32_t numFlavors;
        flavorList->Count( &numFlavors );

        for (uint32_t flavorIndex=0; flavorIndex < numFlavors; ++flavorIndex) {
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt(flavorIndex, getter_AddRefs(genericWrapper));
          nsCOMPtr<nsISupportsCString> currentFlavor;
          currentFlavor = do_QueryInterface(genericWrapper);

          if (currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString ( getter_Copies(flavorStr) );
            if (strcmp(flavorStr, aDataFlavor) == 0) {
              *_retval = true;
              break;
            }
          }
        } // for each flavor
      }
    }
  }

  return NS_OK;
}

// --------------------------------------------------------------------------

// use nsIWebBrowserPersist to fetch the contents of a URL

nsresult nsDragService::SaveAsContents(PCSZ pszDest, nsIURL* aURL)
{
  nsCOMPtr<nsIURI> linkURI(do_QueryInterface(aURL));
  if (!linkURI)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebBrowserPersist> webPersist(
    do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1"));
  if (!webPersist)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> file;
  NS_NewNativeLocalFile(nsDependentCString(pszDest), true,
                        getter_AddRefs(file));
  if (!file)
    return NS_ERROR_FAILURE;

  FILE* fp;
  if (NS_FAILED(file->OpenANSIFileDesc("wb+", &fp)))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  GetSourceDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);

  fwrite("", 0, 1, fp);
  fclose(fp);
  webPersist->SaveURI(linkURI, nullptr, nullptr, nullptr, nullptr, file,
                     document->GetLoadContext());

  return NS_OK;
}

// --------------------------------------------------------------------------

// save this URL in a file that the WPS will identify as a WPUrl object

nsresult nsDragService::SaveAsURL(PCSZ pszDest, nsIURI* aURI)
{
  nsAutoCString strUri;
  aURI->GetSpec(strUri);

  if (strUri.IsEmpty())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> file;
  NS_NewNativeLocalFile(nsDependentCString(pszDest), true,
                        getter_AddRefs(file));
  if (!file)
    return NS_ERROR_FAILURE;

  FILE* fp;
  if (NS_FAILED(file->OpenANSIFileDesc("wb+", &fp)))
    return NS_ERROR_FAILURE;

  fwrite(strUri.get(), strUri.Length(), 1, fp);
  fclose(fp);

  nsCOMPtr<nsIDOMDocument> domDoc;
  GetSourceDocument(getter_AddRefs(domDoc));
  SaveTypeAndSource(file, domDoc, "UniformResourceLocator");

  return NS_OK;
}

// --------------------------------------------------------------------------

// save this text to file after conversion to the current codepage

nsresult nsDragService::SaveAsText(PCSZ pszDest, nsISupportsString* aString)
{
  nsAutoString strData;
  aString->GetData(strData);

  if (strData.IsEmpty())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> file;
  NS_NewNativeLocalFile(nsDependentCString(pszDest), true,
                        getter_AddRefs(file));
  if (!file)
    return NS_ERROR_FAILURE;

  nsXPIDLCString textStr;
  int cnt = UnicodeToCodepage(strData, getter_Copies(textStr));
  if (!cnt)
    return NS_ERROR_FAILURE;

  FILE* fp;
  if (NS_FAILED(file->OpenANSIFileDesc("wb+", &fp)))
    return NS_ERROR_FAILURE;

  fwrite(textStr.get(), cnt, 1, fp);
  fclose(fp);

  nsCOMPtr<nsIDOMDocument> domDoc;
  GetSourceDocument(getter_AddRefs(domDoc));
  SaveTypeAndSource(file, domDoc, "Plain Text");

  return NS_OK;
}

// --------------------------------------------------------------------------

// Split a Moz Url/Title into its components, save the Url for use by
// a native drop, then compose a title.

nsresult  nsDragService::GetUrlAndTitle(nsISupports *aGenericData,
                                        char **aTargetName)
{
    // get the URL/title string
  nsCOMPtr<nsISupportsString> strObject ( do_QueryInterface(aGenericData));
  if (!strObject)
    return NS_ERROR_FAILURE;
  nsAutoString strData;
  strObject->GetData(strData);

    // split string into URL and Title - 
    // if there's a title but no URL, there's no reason to continue
  int32_t lineIndex = strData.FindChar ('\n');
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
    uint32_t strLth = std::min((int)strData.Length()-lineIndex, MAXTITLELTH);
    nsAutoString strTitle;
    strData.Mid(strTitle, lineIndex, strLth);
    if (!UnicodeToCodepage(strTitle, aTargetName))
      return NS_ERROR_FAILURE;

    mSourceData = do_QueryInterface(saveURI);
    mMimeType = kURLMime;
    return NS_OK;
  }

    // if the URI can be handled as a URL, construct a title from
    // the hostname & filename;  if not, use the first MAXTITLELTH
    // characters that appear after the scheme name

  nsAutoCString strTitle;

  nsCOMPtr<nsIURL> urlObj( do_QueryInterface(saveURI));
  if (urlObj) {
    nsAutoCString strFile;

    urlObj->GetHost(strTitle);
    urlObj->GetFileName(strFile);
    if (!strFile.IsEmpty()) {
      strTitle.AppendLiteral("/");
      strTitle.Append(strFile);
    }
    else {
      urlObj->GetDirectory(strFile);
      if (strFile.Length() > 1) {
        nsAutoCString::const_iterator start, end, curr;
        strFile.BeginReading(start);
        strFile.EndReading(end);
        strFile.EndReading(curr);
        for (curr.advance(-2); curr != start; --curr)
          if (*curr == '/')
            break;
        strTitle.Append(Substring(curr, end));
      }
    }
  }
  else {
    saveURI->GetSpec(strTitle);
    int32_t index = strTitle.FindChar (':');
    if (index != -1) {
      if ((strTitle.get())[++index] == '/')
        if ((strTitle.get())[++index] == '/')
          ++index;
      strTitle.Cut(0, index);
    }
    if (strTitle.Length() > MAXTITLELTH)
      strTitle.Truncate(MAXTITLELTH);
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

nsresult  nsDragService::GetUniTextTitle(nsISupports *aGenericData,
                                         char **aTargetName)
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
// nsIDragSessionOS2
// --------------------------------------------------------------------------

// DragOverMsg() provides minimal handling if a drag session is already
// in progress.  If not, it assumes this is a native drag that has just
// entered the window and calls NativeDragEnter() to start a session.

NS_IMETHODIMP nsDragService::DragOverMsg(PDRAGINFO pdinfo, MRESULT &mr,
                                         uint32_t* dragFlags)
{
  nsresult  rv = NS_ERROR_FAILURE;

  if (!&mr || !dragFlags || !pdinfo || !DrgAccessDraginfo(pdinfo))
    return rv;

  *dragFlags = 0;
  mr = MRFROM2SHORT(DOR_NEVERDROP, 0);

    // examine the dragged item & "start" a drag session if OK;
    // also, signal the need for a dragenter event
  if (!mDoingDrag)
    if (NS_SUCCEEDED(NativeDragEnter(pdinfo)))
      *dragFlags |= DND_DISPATCHENTEREVENT;

    // if we're in a drag, set it up to be dispatched
  if (mDoingDrag) {
    SetCanDrop(false);
    switch (pdinfo->usOperation) {
      case DO_COPY:
        SetDragAction(DRAGDROP_ACTION_COPY);
        break;
      case DO_LINK:
        SetDragAction(DRAGDROP_ACTION_LINK);
        break;
      default:
        SetDragAction(DRAGDROP_ACTION_MOVE);
        break;
    }
    if (mSourceNode)
      *dragFlags |= DND_DISPATCHEVENT | DND_GETDRAGOVERRESULT | DND_MOZDRAG;
    else
      *dragFlags |= DND_DISPATCHEVENT | DND_GETDRAGOVERRESULT | DND_NATIVEDRAG;
    rv = NS_OK;
  }

  DrgFreeDraginfo(pdinfo);
  return rv;
}

// --------------------------------------------------------------------------

// Evaluates native drag data, and if acceptable, creates & stores
// a transferable with the available flavors (but not the data);
// if successful, it "starts" the session.

NS_IMETHODIMP nsDragService::NativeDragEnter(PDRAGINFO pdinfo)
{
  nsresult  rv = NS_ERROR_FAILURE;
  bool      isFQFile = FALSE;
  bool      isAtom = FALSE;
  PDRAGITEM pditem = 0;

  if (pdinfo->cditem != 1)
    return rv;

  pditem = DrgQueryDragitemPtr(pdinfo, 0);

  if (pditem) {
    if (DrgVerifyRMF(pditem, "DRM_ATOM", 0)) {
      isAtom = TRUE;
      rv = NS_OK;
    }
    else
    if (DrgVerifyRMF(pditem, "DRM_DTSHARE", 0))
      rv = NS_OK;
    else
    if (DrgVerifyRMF(pditem, "DRM_OS2FILE", 0)) {
      rv = NS_OK;
      if (pditem->hstrContainerName && pditem->hstrSourceName)
        isFQFile = TRUE;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsITransferable> trans(
            do_CreateInstance("@mozilla.org/widget/transferable;1", &rv));
    if (trans) {
      trans->Init(nullptr);

      bool isUrl = DrgVerifyType(pditem, "UniformResourceLocator");
      bool isAlt = (WinGetKeyState(HWND_DESKTOP, VK_ALT) & 0x8000);

        // if this is a fully-qualified file or the item claims to be
        // a Url, identify it as a Url & also offer it as html
      if ((isFQFile && !isAlt) || isUrl) {
        trans->AddDataFlavor(kURLMime);
        trans->AddDataFlavor(kHTMLMime);
      }

        // everything is always "text"
      trans->AddDataFlavor(kUnicodeMime);

        // if we can create the array, initialize the session
      nsCOMPtr<nsISupportsArray> transArray(
                    do_CreateInstance("@mozilla.org/supports-array;1", &rv));
      if (transArray) {
        transArray->InsertElementAt(trans, 0);
        mSourceDataItems = transArray;

        // add the dragged data to the transferable if we have easy access
        // to it (i.e. no need to read a file or request rendering);  for
        // URLs, we'll skip creating a title until the drop occurs
        nsXPIDLCString someText;
        if (isAtom) {
          if (NS_SUCCEEDED(GetAtom(pditem->ulItemID, getter_Copies(someText))))
            NativeDataToTransferable( someText.get(), 0, isUrl);
        }
        else
        if (isFQFile && !isAlt &&
            NS_SUCCEEDED(GetFileName(pditem, getter_Copies(someText)))) {
          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(NS_NewNativeLocalFile(someText, true,
                                                 getter_AddRefs(file)))) {
            nsAutoCString textStr;
            NS_GetURLSpecFromFile(file, textStr);
            if (!textStr.IsEmpty()) {
              someText.Assign(ToNewCString(textStr));
              NativeDataToTransferable( someText.get(), 0, TRUE);
            }
          }
        }

        mSourceNode = 0;
        mSourceDocument = 0;
        mDoingDrag = TRUE;
        rv = NS_OK;
      }
    }
  }

  return rv;
}

// --------------------------------------------------------------------------

// Invoked after a dragover event has been dispatched, this constructs
// a reply to DM_DRAGOVER based on the canDrop & dragAction attributes.

NS_IMETHODIMP nsDragService::GetDragoverResult(MRESULT& mr)
{
  nsresult  rv = NS_ERROR_FAILURE;
  if (!&mr)
    return rv;

  if (mDoingDrag) {

    bool canDrop = false;
    USHORT usDrop;
    GetCanDrop(&canDrop);
    if (canDrop)
      usDrop = DOR_DROP;
    else
      usDrop = DOR_NODROP;

    uint32_t action;
    USHORT   usOp;
    GetDragAction(&action);
    if (action & DRAGDROP_ACTION_COPY)
      usOp = DO_COPY;
    else
    if (action & DRAGDROP_ACTION_LINK)
      usOp = DO_LINK;
    else {
      if (mSourceNode)
        usOp = DO_MOVE;
      else
        usOp = DO_UNKNOWN+1;
      if (action == DRAGDROP_ACTION_NONE)
        usDrop = DOR_NODROP;
    }

    mr = MRFROM2SHORT(usDrop, usOp);
    rv = NS_OK;
  }
  else
    mr = MRFROM2SHORT(DOR_NEVERDROP, 0);

  return rv;
}

// --------------------------------------------------------------------------

// have the client dispatch the event, then call ExitSession()

NS_IMETHODIMP nsDragService::DragLeaveMsg(PDRAGINFO pdinfo, uint32_t* dragFlags)
{
  if (!mDoingDrag || !dragFlags)
    return NS_ERROR_FAILURE;

  if (mSourceNode)
    *dragFlags = DND_DISPATCHEVENT | DND_EXITSESSION | DND_MOZDRAG;
  else
    *dragFlags = DND_DISPATCHEVENT | DND_EXITSESSION | DND_NATIVEDRAG;

  return NS_OK;
}

// --------------------------------------------------------------------------

// DropHelp occurs when you press F1 during a drag;  apparently,
// it's like a regular drop in that the target has to do clean up

NS_IMETHODIMP nsDragService::DropHelpMsg(PDRAGINFO pdinfo, uint32_t* dragFlags)
{
  if (!mDoingDrag)
    return NS_ERROR_FAILURE;

  if (pdinfo && DrgAccessDraginfo(pdinfo)) {
    DrgDeleteDraginfoStrHandles(pdinfo);
    DrgFreeDraginfo(pdinfo);
  }

  if (!dragFlags)
    return NS_ERROR_FAILURE;

  if (mSourceNode)
    *dragFlags = DND_DISPATCHEVENT | DND_EXITSESSION | DND_MOZDRAG;
  else
    *dragFlags = DND_DISPATCHEVENT | DND_EXITSESSION | DND_NATIVEDRAG;

  return NS_OK;
}

// --------------------------------------------------------------------------

// for native drags, clean up;
// for all drags, signal that Moz is no longer in d&d mode

NS_IMETHODIMP nsDragService::ExitSession(uint32_t* dragFlags)
{
  if (!mDoingDrag)
    return NS_ERROR_FAILURE;

  if (!mSourceNode) {
    mSourceDataItems = 0;
    mDataTransfer = 0;
    mDoingDrag = FALSE;

      // if we created a temp file, delete it
    if (gTempFile) {
      DosDelete(gTempFile);
      nsMemory::Free(gTempFile);
      gTempFile = 0;
    }
  }

  if (!dragFlags)
    return NS_ERROR_FAILURE;
  *dragFlags = 0;

  return NS_OK;
}

// --------------------------------------------------------------------------

// If DropMsg() is presented with native data that has to be rendered,
// the drop event & cleanup will be defered until the client's window
// has received a render-complete msg.

NS_IMETHODIMP nsDragService::DropMsg(PDRAGINFO pdinfo, HWND hwnd,
                                     uint32_t* dragFlags)
{
  if (!mDoingDrag || !dragFlags || !pdinfo || !DrgAccessDraginfo(pdinfo))
    return NS_ERROR_FAILURE;

  switch (pdinfo->usOperation) {
    case DO_MOVE:
      SetDragAction(DRAGDROP_ACTION_MOVE);
      break;
    case DO_COPY:
      SetDragAction(DRAGDROP_ACTION_COPY);
      break;
    case DO_LINK:
      SetDragAction(DRAGDROP_ACTION_LINK);
      break;
    default:  // avoid "moving" (i.e. deleting) native text/objects
      if (mSourceNode)
        SetDragAction(DRAGDROP_ACTION_MOVE);
      else
        SetDragAction(DRAGDROP_ACTION_COPY);
      break;
  }

    // if this is a native drag, move the source data to a transferable;
    // request rendering if needed
  nsresult rv = NS_OK;
  bool rendering = false;
  if (!mSourceNode)
    rv = NativeDrop( pdinfo, hwnd, &rendering);

    // note: NativeDrop() sends an end-conversation msg to native
    // sources but nothing sends them to Mozilla - however, Mozilla
    // (i.e. nsDragService) doesn't need them, so...

    // if rendering, the action flags are off because we don't want
    // the client to do anything yet;  the status flags are off because
    // we'll be exiting d&d mode before the next screen update occurs
  if (rendering)
    *dragFlags = 0;
  else {
    // otherwise, set the flags & free the native drag structures

    *dragFlags = DND_EXITSESSION;
    if (NS_SUCCEEDED(rv)) {
      if (mSourceNode)
        *dragFlags |= DND_DISPATCHEVENT | DND_INDROP | DND_MOZDRAG;
      else
        *dragFlags |= DND_DISPATCHEVENT | DND_INDROP | DND_NATIVEDRAG;
    }

    DrgDeleteDraginfoStrHandles(pdinfo);
    DrgFreeDraginfo(pdinfo);
    rv = NS_OK;
  }

  return rv;
}

// --------------------------------------------------------------------------

// Invoked by DropMsg to fill a transferable with native data;
// if necessary, requests the source to render it.

NS_IMETHODIMP nsDragService::NativeDrop(PDRAGINFO pdinfo, HWND hwnd,
                                        bool* rendering)
{
  *rendering = false;

  nsresult rv = NS_ERROR_FAILURE;
  PDRAGITEM pditem = DrgQueryDragitemPtr(pdinfo, 0);
  if (!pditem)
    return rv;

  nsXPIDLCString dropText;
  bool isUrl = DrgVerifyType(pditem, "UniformResourceLocator");

    // identify format; the order of evaluation here is important

    // DRM_ATOM - DragText stores up to 255 chars in a Drg API atom
    // DRM_DTSHARE - DragText renders up to 1mb to named shared mem
  if (DrgVerifyRMF(pditem, "DRM_ATOM", 0))
    rv = GetAtom(pditem->ulItemID, getter_Copies(dropText));
  else
  if (DrgVerifyRMF(pditem, "DRM_DTSHARE", 0)) {
    rv = RenderToDTShare( pditem, hwnd);
    if (NS_SUCCEEDED(rv))
      *rendering = true;
  }

    // DRM_OS2FILE - get the file's path or contents if it exists;
    // request rendering if it doesn't
  else
  if (DrgVerifyRMF(pditem, "DRM_OS2FILE", 0)) {
    bool isAlt = (WinGetKeyState(HWND_DESKTOP, VK_ALT) & 0x8000);

      // the file has to be rendered - currently, we only present
      // its content, not its name, to Moz to avoid conflicts
    if (!pditem->hstrContainerName || !pditem->hstrSourceName) {
      rv = RenderToOS2File( pditem, hwnd);
      if (NS_SUCCEEDED(rv))
        *rendering = true;
    }
      // for Url objects and 'Alt+Drop', get the file's contents;
      // otherwise, convert it's path to a Url
    else {
      nsXPIDLCString fileName;
      if (NS_SUCCEEDED(GetFileName(pditem, getter_Copies(fileName)))) {
        if (isUrl || isAlt)
          rv = GetFileContents(fileName.get(), getter_Copies(dropText));
        else {
          isUrl = true;
          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(NS_NewNativeLocalFile(fileName,
                                         true, getter_AddRefs(file)))) {
            nsAutoCString textStr;
            NS_GetURLSpecFromFile(file, textStr);
            if (!textStr.IsEmpty()) {
              dropText.Assign(ToNewCString(textStr));
              rv = NS_OK;
            }
          }
        } // filename as Url
      } // got filename
    } // existing files
  } // DRM_OS2FILE

    // if OK, put what data there is in the transferable;  this could be
    // everything needed or just the title of a Url that needs rendering
  if (NS_SUCCEEDED(rv)) {

      // for Urls, get the title & remove any linefeeds
    nsXPIDLCString titleText;
    if (isUrl &&
        pditem->hstrTargetName &&
        NS_SUCCEEDED(GetAtom(pditem->hstrTargetName, getter_Copies(titleText))))
      for (char* ptr=strchr(titleText.BeginWriting(),'\n'); ptr; ptr=strchr(ptr, '\n'))
        *ptr = ' ';

    rv = NativeDataToTransferable( dropText.get(), titleText.get(), isUrl);
  }

    // except for renderings, tell the source we're done with the data
  if (!*rendering)
    DrgSendTransferMsg( pditem->hwndItem, DM_ENDCONVERSATION,
                        (MPARAM)pditem->ulItemID,
                        (MPARAM)DMFL_TARGETSUCCESSFUL);

  return (rv);
}

// --------------------------------------------------------------------------

// Because RenderCompleteMsg() is called after the native (PM) drag
// session has ended, all of the drag status flags should be off.
//
// FYI... PM's asynchronous rendering mechanism is not compatible with
// nsIDataFlavorProvider which expects data to be rendered synchronously

NS_IMETHODIMP nsDragService::RenderCompleteMsg(PDRAGTRANSFER pdxfer,
                                        USHORT usResult, uint32_t* dragFlags)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (!mDoingDrag || !pdxfer)
    return rv;

    // this msg should never come from Moz - if it does, fail
  if (!mSourceNode)
    rv = NativeRenderComplete(pdxfer, usResult);

    // DrgQueryDraginfoPtrFromDragitem() doesn't work - this does
  PDRAGINFO pdinfo = (PDRAGINFO)MAKEULONG(0x2c, HIUSHORT(pdxfer->pditem));

  DrgDeleteStrHandle(pdxfer->hstrSelectedRMF);
  DrgDeleteStrHandle(pdxfer->hstrRenderToName);
  DrgFreeDragtransfer(pdxfer);

    // if the source is Moz, don't touch pdinfo - it's been freed already
  if (pdinfo && !mSourceNode) {
    DrgDeleteDraginfoStrHandles(pdinfo);
    DrgFreeDraginfo(pdinfo);
  }

    // this shouldn't happen
  if (!dragFlags)
    return (ExitSession(dragFlags));

    // d&d is over, so the DND_DragStatus flags should all be off
  *dragFlags = DND_EXITSESSION;
  if (NS_SUCCEEDED(rv))
    *dragFlags |= DND_DISPATCHEVENT;

    // lie so the client will honor the exit-session flag
  return NS_OK;
}

// --------------------------------------------------------------------------

// this is here to provide a false sense of symmetry with the other
// method-pairs - rendered data always comes from a native source

NS_IMETHODIMP nsDragService::NativeRenderComplete(PDRAGTRANSFER pdxfer,
                                                  USHORT usResult)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsXPIDLCString rmf;

    // identify the rendering mechanism, then get the data
  if (NS_SUCCEEDED(GetAtom(pdxfer->hstrSelectedRMF, getter_Copies(rmf)))) {
    nsXPIDLCString dropText;
    if (!strcmp(rmf.get(), DTSHARE_RMF))
      rv = RenderToDTShareComplete(pdxfer, usResult, getter_Copies(dropText));
    else
    if (!strcmp(rmf.get(), OS2FILE_TXTRMF) ||
        !strcmp(rmf.get(), OS2FILE_UNKRMF))
      rv = RenderToOS2FileComplete(pdxfer, usResult, true,
                                   getter_Copies(dropText));

    if (NS_SUCCEEDED(rv)) {
      bool isUrl = false;
      IsDataFlavorSupported(kURLMime, &isUrl);
      rv = NativeDataToTransferable( dropText.get(), 0, isUrl);
    }
  }

  DrgSendTransferMsg(pdxfer->hwndClient, DM_ENDCONVERSATION,
                     (MPARAM)pdxfer->ulTargetInfo,
                     (MPARAM)DMFL_TARGETSUCCESSFUL);

  return rv;
}

// --------------------------------------------------------------------------

// fills the transferable created by NativeDragEnter with
// the set of flavors and data the target will see onDrop

NS_IMETHODIMP nsDragService::NativeDataToTransferable( PCSZ pszText,
                                                PCSZ pszTitle, bool isUrl)
{
  nsresult rv = NS_ERROR_FAILURE;
    // the transferable should have been created on DragEnter
 if (!mSourceDataItems)
    return rv;

  nsCOMPtr<nsISupports> genericItem;
  mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
  nsCOMPtr<nsITransferable> trans (do_QueryInterface(genericItem));
  if (!trans)
    return rv;

    // remove invalid flavors
  if (!isUrl) {
    trans->RemoveDataFlavor(kURLMime);
    trans->RemoveDataFlavor(kHTMLMime);
  }

    // if there's no text, exit - but first see if we have the title of
    // a Url that needs to be rendered;  if so, stash it for later use
  if (!pszText || !*pszText) {
    if (isUrl && pszTitle && *pszTitle) {
      nsXPIDLString outTitle;
      if (CodepageToUnicode(nsDependentCString(pszTitle),
                                               getter_Copies(outTitle))) {
        nsCOMPtr<nsISupportsString> urlPrimitive(
                        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
        if (urlPrimitive ) {
          urlPrimitive->SetData(outTitle);
          trans->SetTransferData(kURLDescriptionMime, urlPrimitive,
                                 2*outTitle.Length());
        }
      }
    }
    return NS_OK;
  }

  nsXPIDLString outText;
  if (!CodepageToUnicode(nsDependentCString(pszText), getter_Copies(outText)))
    return rv;

  if (isUrl) {

      // if no title was supplied, see if it was stored in the transferable
    nsXPIDLString outTitle;
    if (pszTitle && *pszTitle) {
      if (!CodepageToUnicode(nsDependentCString(pszTitle),
                             getter_Copies(outTitle)))
        return rv;
    }
    else {
      uint32_t len;
      nsCOMPtr<nsISupports> genericData;
      if (NS_SUCCEEDED(trans->GetTransferData(kURLDescriptionMime,
                                   getter_AddRefs(genericData), &len))) {
        nsCOMPtr<nsISupportsString> strObject(do_QueryInterface(genericData));
        if (strObject)
          strObject->GetData(outTitle);
      }
    }

      // construct the Url flavor
    nsCOMPtr<nsISupportsString> urlPrimitive(
                            do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
    if (urlPrimitive ) {
      if (outTitle.IsEmpty()) {
        urlPrimitive->SetData(outText);
        trans->SetTransferData(kURLMime, urlPrimitive, 2*outText.Length());
      }
      else {
        nsString urlStr( outText + NS_LITERAL_STRING("\n") + outTitle);
        urlPrimitive->SetData(urlStr);
        trans->SetTransferData(kURLMime, urlPrimitive, 2*urlStr.Length());
      }
      rv = NS_OK;
    }

      // construct the HTML flavor - for supported graphics,
      // use an IMG tag, for all others create a link
    nsCOMPtr<nsISupportsString> htmlPrimitive(
                            do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
    if (htmlPrimitive ) {
      nsString htmlStr;
      nsCOMPtr<nsIURI> uri;

      rv = NS_ERROR_FAILURE;
      if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), pszText))) {
        nsCOMPtr<nsIURL> url (do_QueryInterface(uri));
        if (url) {
          nsAutoCString extension;
          url->GetFileExtension(extension);
          if (!extension.IsEmpty()) {
            if (extension.LowerCaseEqualsLiteral("gif") ||
                extension.LowerCaseEqualsLiteral("jpg") ||
                extension.LowerCaseEqualsLiteral("png") ||
                extension.LowerCaseEqualsLiteral("jpeg"))
              rv = NS_OK;
          }
        }
      }

      if (NS_SUCCEEDED(rv))
        htmlStr.Assign(NS_LITERAL_STRING("<img src=\"") +
                       outText +
                       NS_LITERAL_STRING("\" alt=\"") +
                       outTitle +
                       NS_LITERAL_STRING("\"/>") );
      else
        htmlStr.Assign(NS_LITERAL_STRING("<a href=\"") +
                       outText +
                       NS_LITERAL_STRING("\">") +
                       (outTitle.IsEmpty() ? outText : outTitle) +
                       NS_LITERAL_STRING("</a>") );

      htmlPrimitive->SetData(htmlStr);
      trans->SetTransferData(kHTMLMime, htmlPrimitive, 2*htmlStr.Length());
      rv = NS_OK;
    }
  }

    // add the Text flavor
  nsCOMPtr<nsISupportsString> textPrimitive(
                            do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (textPrimitive ) {
    textPrimitive->SetData(nsDependentString(outText));
    trans->SetTransferData(kUnicodeMime, textPrimitive, 2*outText.Length());
    rv = NS_OK;
  }

    // return OK if we put anything in the transferable
  return rv;
}

// --------------------------------------------------------------------------
// Helper functions
// --------------------------------------------------------------------------

// currently, the same filename is used for every render request;
// it is deleted when the drag session ends

nsresult RenderToOS2File( PDRAGITEM pditem, HWND hwnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsXPIDLCString fileName;

  if (NS_SUCCEEDED(GetTempFileName(getter_Copies(fileName)))) {
    const char * pszRMF;
    if (DrgVerifyRMF(pditem, "DRM_OS2FILE", "DRF_TEXT"))
      pszRMF = OS2FILE_TXTRMF;
    else
      pszRMF = OS2FILE_UNKRMF;

    rv = RequestRendering( pditem, hwnd, pszRMF, fileName.get());
  }

  return rv;
}

// --------------------------------------------------------------------------

// return a buffer with the rendered file's Url or contents

nsresult RenderToOS2FileComplete(PDRAGTRANSFER pdxfer, USHORT usResult,
                                 bool content, char** outText)
{
  nsresult rv = NS_ERROR_FAILURE;

    // for now, override content flag & always return content
  content = true;

  if (usResult & DMFL_RENDEROK) {
    if (NS_SUCCEEDED(GetAtom( pdxfer->hstrRenderToName, &gTempFile))) {
      if (content)
        rv = GetFileContents(gTempFile, outText);
      else {
        nsCOMPtr<nsIFile> file;
        if (NS_SUCCEEDED(NS_NewNativeLocalFile(nsDependentCString(gTempFile),
                                         true, getter_AddRefs(file)))) {
          nsAutoCString textStr;
          NS_GetURLSpecFromFile(file, textStr);
          if (!textStr.IsEmpty()) {
            *outText = ToNewCString(textStr);
            rv = NS_OK;
          }
        }
      }
    }
  }
    // gTempFile will be deleted when ExitSession() is called

  return rv;
}

// --------------------------------------------------------------------------

// DTShare uses 1mb of uncommitted named-shared memory
// (next time I'll do it differently - rw)

nsresult RenderToDTShare( PDRAGITEM pditem, HWND hwnd)
{
  nsresult rv;
  void *   pMem;

#ifdef MOZ_OS2_HIGH_MEMORY
  APIRET rc = DosAllocSharedMem( &pMem, DTSHARE_NAME, 0x100000,
                                 PAG_WRITE | PAG_READ | OBJ_ANY);
  if (rc != NO_ERROR &&
      rc != ERROR_ALREADY_EXISTS) { // Did the kernel handle OBJ_ANY?
    // Try again without OBJ_ANY and if the first failure was not caused
    // by OBJ_ANY then we will get the same failure, else we have taken
    // care of pre-FP13 systems where the kernel couldn't handle it.
    rc = DosAllocSharedMem( &pMem, DTSHARE_NAME, 0x100000,
                            PAG_WRITE | PAG_READ);
  }
#else
  APIRET rc = DosAllocSharedMem( &pMem, DTSHARE_NAME, 0x100000,
                                 PAG_WRITE | PAG_READ);
#endif

  if (rc == ERROR_ALREADY_EXISTS)
    rc = DosGetNamedSharedMem( &pMem, DTSHARE_NAME,
                               PAG_WRITE | PAG_READ);
  if (rc)
    rv = NS_ERROR_FAILURE;
  else
    rv = RequestRendering( pditem, hwnd, DTSHARE_RMF, DTSHARE_NAME);

  return rv;
}

// --------------------------------------------------------------------------

// return a buffer with the rendered text

nsresult RenderToDTShareComplete(PDRAGTRANSFER pdxfer, USHORT usResult,
                                 char** outText)
{
  nsresult rv = NS_ERROR_FAILURE;
  void * pMem;
  char * pszText = 0;

  APIRET rc = DosGetNamedSharedMem( &pMem, DTSHARE_NAME, PAG_WRITE | PAG_READ);

  if (!rc) {
    if (usResult & DMFL_RENDEROK) {
      pszText = (char*)nsMemory::Alloc( ((ULONG*)pMem)[0] + 1);
      if (pszText) {
        strcpy(pszText, &((char*)pMem)[sizeof(ULONG)] );
        RemoveCarriageReturns(pszText);
        *outText = pszText;
        rv = NS_OK;
      }
    }
      // using DosGetNamedSharedMem() on memory we allocated appears
      // to increment its usage ctr, so we have to free it 2x
    DosFreeMem(pMem);
    DosFreeMem(pMem);
  }

  return rv;
}

// --------------------------------------------------------------------------

// a generic request dispatcher

nsresult RequestRendering( PDRAGITEM pditem, HWND hwnd, PCSZ pRMF, PCSZ pName)
{
  PDRAGTRANSFER pdxfer = DrgAllocDragtransfer( 1);
  if (!pdxfer)
    return NS_ERROR_FAILURE;
 
  pdxfer->cb = sizeof(DRAGTRANSFER);
  pdxfer->hwndClient = hwnd;
  pdxfer->pditem = pditem;
  pdxfer->hstrSelectedRMF = DrgAddStrHandle( pRMF);
  pdxfer->hstrRenderToName = 0;
  pdxfer->ulTargetInfo = pditem->ulItemID;
  pdxfer->usOperation = (USHORT)DO_COPY;
  pdxfer->fsReply = 0;
 
    // send the msg before setting a render-to name
  if (pditem->fsControl & DC_PREPAREITEM)
    DrgSendTransferMsg( pditem->hwndItem, DM_RENDERPREPARE, (MPARAM)pdxfer, 0);
 
  pdxfer->hstrRenderToName = DrgAddStrHandle( pName);
 
    // send the msg after setting a render-to name
  if ((pditem->fsControl & (DC_PREPARE | DC_PREPAREITEM)) == DC_PREPARE)
    DrgSendTransferMsg( pditem->hwndItem, DM_RENDERPREPARE, (MPARAM)pdxfer, 0);
 
    // ask the source to render the selected item
  if (!DrgSendTransferMsg( pditem->hwndItem, DM_RENDER, (MPARAM)pdxfer, 0))
    return NS_ERROR_FAILURE;
 
  return NS_OK;
}

// --------------------------------------------------------------------------

// return a ptr to a buffer containing the text associated
// with a drag atom;  the caller frees the buffer

nsresult GetAtom( ATOM aAtom, char** outText)
{
  nsresult rv = NS_ERROR_FAILURE;

  ULONG ulInLength = DrgQueryStrNameLen(aAtom);
  if (ulInLength) {
    char* pszText = (char*)nsMemory::Alloc(++ulInLength);
    if (pszText) {
      DrgQueryStrName(aAtom, ulInLength, pszText);
      RemoveCarriageReturns(pszText);
      *outText = pszText;
      rv = NS_OK;
    }
  }
  return rv;
}

// --------------------------------------------------------------------------

// return a ptr to a buffer containing the file path specified
// in the dragitem;  the caller frees the buffer

nsresult GetFileName(PDRAGITEM pditem, char** outText)
{
  nsresult rv = NS_ERROR_FAILURE;
  ULONG cntCnr = DrgQueryStrNameLen(pditem->hstrContainerName);
  ULONG cntSrc = DrgQueryStrNameLen(pditem->hstrSourceName);

  char* pszText = (char*)nsMemory::Alloc(cntCnr+cntSrc+1);
  if (pszText) {
    DrgQueryStrName(pditem->hstrContainerName, cntCnr+1, pszText);
    DrgQueryStrName(pditem->hstrSourceName, cntSrc+1, &pszText[cntCnr]);
    pszText[cntCnr+cntSrc] = 0;
    *outText = pszText;
    rv = NS_OK;
  }
  return rv;
}

// --------------------------------------------------------------------------

// read the file;  if successful, return a ptr to its contents;
// the caller frees the buffer

nsresult GetFileContents(PCSZ pszPath, char** outText)
{
  nsresult rv = NS_ERROR_FAILURE;
  char* pszText = 0;

  if (pszPath) {
    FILE *fp = fopen(pszPath, "r");
    if (fp) {
      fseek(fp, 0, SEEK_END);
      ULONG filesize = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      if (filesize > 0) {
        size_t readsize = (size_t)filesize;
        pszText = (char*)nsMemory::Alloc(readsize+1);
        if (pszText) {
          readsize = fread((void *)pszText, 1, readsize, fp);
          if (readsize) {
            pszText[readsize] = '\0';
            RemoveCarriageReturns(pszText);
            *outText = pszText;
            rv = NS_OK;
          }
          else {
            nsMemory::Free(pszText);
            pszText = 0;
          }
        }
      }
      fclose(fp);
    }
  }

  return rv;
}

// --------------------------------------------------------------------------

// currently, this returns the same path & filename every time

nsresult GetTempFileName(char** outText)
{
  char * pszText = (char*)nsMemory::Alloc(CCHMAXPATH);
  if (!pszText)
    return NS_ERROR_FAILURE;

  const char * pszPath;
  if (!DosScanEnv("TEMP", &pszPath) || !DosScanEnv("TMP", &pszPath))
    strcpy(pszText, pszPath);
  else
    if (DosQueryPathInfo(".\\.", FIL_QUERYFULLNAME, pszText, CCHMAXPATH))
      pszText[0] = 0;

  strcat(pszText, "\\");
  strcat(pszText, OS2FILE_NAME);
  *outText = pszText;

  return NS_OK;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

// set the file's .TYPE and .SUBJECT EAs;  since this is non-critical
// (though highly desirable), errors aren't reported

void SaveTypeAndSource(nsIFile *file, nsIDOMDocument *domDoc,
                       PCSZ pszType)
{
  if (!file)
    return;

  nsCOMPtr<nsILocalFileOS2> os2file(do_QueryInterface(file));
  if (!os2file ||
      NS_FAILED(os2file->SetFileTypes(nsDependentCString(pszType))))
    return;

  // since the filetype has to be saved, this function
  // may be called even if there isn't any document
  if (!domDoc)
    return;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return;

  // this gets the top-level content URL when frames are used;
  // when nextDoc is zero, currDoc is the browser window, and
  // prevDoc points to the content;
  // note:  neither GetParentDocument() nor GetDocumentURI()
  // refcount the pointers they return, so nsCOMPtr isn't needed
  nsIDocument *prevDoc;
  nsIDocument *currDoc = doc;
  nsIDocument *nextDoc = doc;
  do {
    prevDoc = currDoc;
    currDoc = nextDoc;
    nextDoc = currDoc->GetParentDocument();
  } while (nextDoc);

  nsIURI* srcUri = prevDoc->GetDocumentURI();
  if (!srcUri)
    return;

  // identifying content as coming from chrome is none too useful...
  bool ignore = false;
  srcUri->SchemeIs("chrome", &ignore);
  if (ignore)
    return;

  nsAutoCString url;
  srcUri->GetSpec(url);
  os2file->SetFileSource(url);

  return;
}

// --------------------------------------------------------------------------

// to do:  this needs to take into account the current page's encoding
// if it is different than the PM codepage

int UnicodeToCodepage(const nsAString& aString, char **aResult)
{
  nsAutoCharBuffer buffer;
  int32_t bufLength;
  WideCharToMultiByte(0, PromiseFlatString(aString).get(), aString.Length(),
                      buffer, bufLength);
  *aResult = ToNewCString(nsDependentCString(buffer.Elements()));
  return bufLength;
}

// --------------------------------------------------------------------------

int CodepageToUnicode(const nsACString& aString, PRUnichar **aResult)
{
  nsAutoChar16Buffer buffer;
  int32_t bufLength;
  MultiByteToWideChar(0, PromiseFlatCString(aString).get(),
                      aString.Length(), buffer, bufLength);
  *aResult = ToNewUnicode(nsDependentString(buffer.Elements()));
  return bufLength;
}

// --------------------------------------------------------------------------

// removes carriage returns in-place;  it should only be used on
// raw text buffers that haven't been assigned to a string object

void RemoveCarriageReturns(char * pszText)
{
  ULONG  cnt;
  char * next;
  char * source;
  char * target;

  target = strchr(pszText, 0x0d);
  if (!target)
    return;

  source = target + 1;

  while ((next = strchr(source, 0x0d)) != 0) {

    cnt = next - source;
    memcpy(target, source, cnt);
    target += cnt;
    source = next + 1;

  }

  strcpy(target, source);
  return;
}

// --------------------------------------------------------------------------
