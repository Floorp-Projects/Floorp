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


void WriteTypeEA(const char* filename, const char* type);
BOOL GetURLObjectContents(PDRAGITEM pDragItem);

nsDragService::nsDragService()
{
  /* member initializers and constructor code */
   mDragWnd = WinCreateWindow( HWND_DESKTOP, WC_STATIC, 0, 0, 0, 0, 0, 0,
                               HWND_DESKTOP, HWND_BOTTOM, 0, 0, 0);
   WinSubclassWindow( mDragWnd, nsDragWindowProc);
}

MRESULT EXPENTRY nsDragWindowProc(HWND hWnd, ULONG msg, MPARAM mp1,
                                  MPARAM mp2)
{
   PDRAGTRANSFER pdragtransfer;
   PDRAGITEM pdragitem;
   FILE *fp;
   ULONG ulLength;
   PSZ pszURL;
   CHAR chPath[CCHMAXPATH];
   switch (msg) {
   case DM_RENDER:
      pdragtransfer = (PDRAGTRANSFER)mp1;
      DrgQueryStrName(pdragtransfer->hstrRenderToName, CCHMAXPATH, chPath);
      ulLength = DrgQueryStrNameLen(pdragtransfer->pditem->hstrSourceName);
      pszURL = (PSZ)nsMemory::Alloc(ulLength+1);
      DrgQueryStrName(pdragtransfer->pditem->hstrSourceName, ulLength+1, pszURL);
      fp = fopen(chPath, "wb+");
      fwrite(pszURL, ulLength, 1, fp);
      fclose(fp);
      WriteTypeEA(chPath, "UniformResourceLocator");
      nsMemory::Free(pszURL);
      DrgPostTransferMsg(pdragtransfer->hwndClient, DM_RENDERCOMPLETE, pdragtransfer, DMFL_RENDEROK,0,TRUE);
      DrgFreeDragtransfer(pdragtransfer);
      return (MRESULT)TRUE;
      break;
   default:
     break;
   }
  return ::WinDefWindowProc(hWnd, msg, mp1, mp2);
}

nsDragService::~nsDragService()
{
  /* destructor code */
  WinDestroyWindow(mDragWnd);
}

NS_IMETHODIMP nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode, nsISupportsArray *aTransferables, nsIScriptableRegion *aRegion, PRUint32 aActionType)
{
  nsBaseDragService::InvokeDragSession ( aDOMNode, aTransferables, aRegion, aActionType );

  // set our reference to the transferables.  this will also addref
  // the transferables since we're going to hang onto this beyond the
  // length of this call
  mSourceDataItems = aTransferables;

  WinSetCapture(HWND_DESKTOP, NULLHANDLE);

  PDRAGINFO pDragInfo = DrgAllocDraginfo(1); /* Assume we are only dragging one thing for now */
  APIRET rc;

  if(pDragInfo)
  {
    pDragInfo->usOperation = DO_DEFAULT;
    DRAGITEM dragitem;
    dragitem.hwndItem            = mDragWnd;
    dragitem.ulItemID            = (ULONG)0;
    dragitem.hstrType            = DrgAddStrHandle("UniformResourceLocator");
    dragitem.hstrRMF             = DrgAddStrHandle("<DRM_OS2FILE,DRF_UNKNOWN>");
    dragitem.hstrContainerName   = DrgAddStrHandle("");
    dragitem.hstrSourceName      = NULLHANDLE;
    dragitem.hstrTargetName      = NULLHANDLE;
    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item (do_QueryInterface(genericItem));
    PRUint32 len = 0;
    nsCOMPtr<nsISupports> genericURL;
    if ( NS_SUCCEEDED(item->GetTransferData(kURLMime, getter_AddRefs(genericURL), &len)) )
    {
      nsCOMPtr<nsISupportsString> urlObject ( do_QueryInterface(genericURL) );
      if( urlObject )
      {
        nsAutoString urlInfo;
        nsAutoString linkName, url, holder;
        urlObject->GetData ( urlInfo );
        holder = urlInfo;
        PRInt32 lineIndex = holder.FindChar ('\n');
        if ( lineIndex != -1 )
        {
          holder.Left(url, lineIndex);
          holder.Mid ( linkName, lineIndex + 1, (len/2) - (lineIndex + 1) );
          if (linkName.Length() > 0)
            dragitem.hstrTargetName = DrgAddStrHandle(ToNewCString(linkName));
          else
            dragitem.hstrTargetName = DrgAddStrHandle(ToNewCString(url));
          dragitem.hstrSourceName = DrgAddStrHandle(ToNewCString(url)); 
        }
      }
    }
    if (dragitem.hstrSourceName && dragitem.hstrTargetName) {
       dragitem.hstrRMF             = DrgAddStrHandle("<DRM_OS2FILE,DRF_UNKNOWN>");
    } else {
       dragitem.hstrRMF             = DrgAddStrHandle("<DRM_UNKNOWN,DRF_UNKNOWN>"); /* Moz only drag */
    }
    dragitem.fsControl           = DC_OPEN;
    dragitem.cxOffset            = 2;
    dragitem.cyOffset            = 2;
    dragitem.fsSupportedOps      = DO_COPYABLE|DO_MOVEABLE|DO_LINKABLE;
    rc = DrgSetDragitem(pDragInfo, &dragitem, sizeof(DRAGITEM), 0);
    DRAGIMAGE dragimage;
    memset(&dragimage, 0, sizeof(DRAGIMAGE));
    dragimage.cb = sizeof(DRAGIMAGE);
    dragimage.hImage = WinQuerySysPointer(HWND_DESKTOP, SPTR_FILE, FALSE);
    dragimage.fl = DRG_ICON;
    
    mDoingDrag = PR_TRUE;
    HWND hwndDest = DrgDrag(mDragWnd, pDragInfo, &dragimage, 1, VK_BUTTON2,
                 (void*)0x80000000L); // Don't lock the desktop PS
    mDoingDrag = PR_FALSE;
#ifdef DEBUG
    if (!hwndDest) {
      ERRORID eid = WinGetLastError((HAB)0);
      printf("Drag did not finish - error = %x error=%x\n", eid);
    }
#endif

    // Clean up everything here; no async. case to consider.
    DrgDeleteDraginfoStrHandles(pDragInfo);
    DrgFreeDraginfo(pDragInfo);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetNumDropItems(PRUint32 *aNumDropItems)
{
  mSourceDataItems->Count(aNumDropItems);
  return NS_OK;
}

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

/* Helper functions */
void WriteTypeEA(const char* filename, const char* type)
{
   const unsigned fea2listsize = 6000;
   const char TYPE[] = ".TYPE";
   EAOP2 eaop2;
   eaop2.fpGEA2List = 0;
   eaop2.fpFEA2List = PFEA2LIST(new char[fea2listsize]);
   PFEA2 pFEA2 = &eaop2.fpFEA2List->list[0];

   // create .TYPE EA
   pFEA2->fEA = 0; // .LONGNAME is not needed
   pFEA2->cbName = sizeof(TYPE)-1; // skip \0 terminator

   pFEA2->cbValue = strlen(type)+2*sizeof(USHORT);
   //                                      ^
   //                           space for the type and length field.
   //

   strcpy(pFEA2->szName, TYPE);
   char* pData = pFEA2->szName+pFEA2->cbName+1; // data begins at
                                                // first byte after
                                                // the name
   *(USHORT*)pData = EAT_ASCII;             // type
   *((USHORT*)pData+1) = strlen(type);  // length
   strcpy(pData+2*sizeof(USHORT), type);// content

   pFEA2->oNextEntryOffset = 0; // no more EAs to write.

   eaop2.fpFEA2List->cbList = PCHAR(pData+2*sizeof(USHORT)+
                                    pFEA2->cbValue)-PCHAR(eaop2.fpFEA2List);
   APIRET rc = DosSetPathInfo(filename,
                              FIL_QUERYEASIZE,
                              &eaop2,
                              sizeof(eaop2),
                              0);
}
/* GetURLObjectContents -- This method gets the contents of a WPSH URL
 * object by reading the file name specified in the dragitem and
 * replacing it with the URL contained in the file.  This is necessary
 * so that dropping URL objects onto the browser displays the page at
 * the URL, rather than displaying the URL itself.
 */
BOOL GetURLObjectContents(PDRAGITEM pDragItem)
{
   char szURLFileName[CCHMAXPATH] = {0};
   char szURL[CCHMAXPATH] = {0};
   char szTemp[CCHMAXPATH] = {0};
   char szProtocol[15] = {0};
   char szResource[CCHMAXPATH-15]= {0};
   BOOL rc = FALSE;

   // Get Drive and subdirectory name from hstrContainerName.
   if (!DrgQueryStrName(pDragItem->hstrContainerName,
                        CCHMAXPATH,
                        szURLFileName))
   {
      return(FALSE);
   }

   // Get file name from hstrSourceName.
   if (!DrgQueryStrName(pDragItem->hstrSourceName,
                        CCHMAXPATH,
                        szTemp))
   {
      return(FALSE);
   }

   // Concatenate hstrContainerName and hstrSourceName to get fully
   // qualified name of the URL file.
   strcat(szURLFileName, szTemp);

   // Open the file specified by szURLFileName and read its contents
   // into buffer szURL.
   FILE *fp = fopen(szURLFileName, "r");
   if (fp)
   {
      size_t bytes_read = fread((void *)szURL, 1, CCHMAXPATH, fp);
      if (bytes_read > 0)
      {
         // Delete the container name and source name hstrs.
         DrgDeleteStrHandle(pDragItem->hstrContainerName);
         DrgDeleteStrHandle(pDragItem->hstrSourceName);
         // Replace container name with protocol part of URL.
         char *pStart = szURL;
         char *pProtocol = strchr(szURL, ':');
         if (pProtocol)
         {
            // Bump the pointer to the end of the protocol spec. (ie. '//')
            pProtocol += 3;
         }
         strncpy(szProtocol, szURL, pProtocol- pStart);
         HSTR hstrContainerName = DrgAddStrHandle(szProtocol);
         // Replace source name with resource part of the URL.
         char *pResource = pProtocol;
         strcpy(szResource, pResource);
         HSTR hstrSourceName = DrgAddStrHandle(szResource);
         // Add the new hstr's to the dragitem.
         pDragItem->hstrContainerName = hstrContainerName;
         pDragItem->hstrSourceName = hstrSourceName;

         fclose(fp);

         rc = TRUE;
      }
   }
   return(rc);
}
