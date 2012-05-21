/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include "nsIDragSessionOS2.h"

#define INCL_PM
#include <os2.h>

  // forward declarations
class nsIURI;
class nsIURL;
class nsISupportsString;

class nsDragService : public nsBaseDragService, public nsIDragSessionOS2
{
public:
  nsDragService();
  virtual ~nsDragService();

  NS_DECL_ISUPPORTS_INHERITED

    // nsIDragService
  NS_IMETHOD InvokeDragSession (nsIDOMNode* aDOMNode,
                                nsISupportsArray* aTransferables,
                                nsIScriptableRegion* aRegion,
                                PRUint32 aActionType);
  NS_IMETHOD StartDragSession();
  NS_IMETHOD EndDragSession(bool aDoneDrag);

    // nsIDragSession
  NS_IMETHOD GetNumDropItems(PRUint32* aNumDropItems);
  NS_IMETHOD GetData(nsITransferable* aTransferable, PRUint32 aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor, bool* _retval);

    // nsIDragSessionOS2
  NS_IMETHOD DragOverMsg(PDRAGINFO pdinfo, MRESULT& mr, PRUint32* dragFlags);
  NS_IMETHOD GetDragoverResult(MRESULT& mr);
  NS_IMETHOD DragLeaveMsg(PDRAGINFO pdinfo, PRUint32* dragFlags);
  NS_IMETHOD DropHelpMsg(PDRAGINFO pdinfo, PRUint32* dragFlags);
  NS_IMETHOD ExitSession(PRUint32* dragFlags);
  NS_IMETHOD DropMsg(PDRAGINFO pdinfo, HWND hwnd, PRUint32* dragFlags);
  NS_IMETHOD RenderCompleteMsg(PDRAGTRANSFER pdxfer, USHORT usResult,
                               PRUint32* dragFlags);

protected:
    // nsIDragSessionOS2
  NS_IMETHOD NativeDragEnter(PDRAGINFO pdinfo);
  NS_IMETHOD NativeDrop(PDRAGINFO pdinfo, HWND hwnd, bool* rendering);
  NS_IMETHOD NativeRenderComplete(PDRAGTRANSFER pdxfer, USHORT usResult);
  NS_IMETHOD NativeDataToTransferable( PCSZ pszText, PCSZ pszTitle,
                                       bool isUrl);

  nsresult SaveAsContents(PCSZ szDest, nsIURL* aURL);
  nsresult SaveAsURL(PCSZ szDest, nsIURI* aURI);
  nsresult SaveAsText(PCSZ szDest, nsISupportsString* aString);
  nsresult GetUrlAndTitle(nsISupports* aGenericData, char** aTargetName);
  nsresult GetUniTextTitle(nsISupports* aGenericData, char** aTargetName);

  HWND          mDragWnd;
  const char*   mMimeType;
  nsCOMPtr<nsISupportsArray> mSourceDataItems;
  nsCOMPtr<nsISupports>      mSourceData;

  friend MRESULT EXPENTRY nsDragWindowProc( HWND, ULONG, MPARAM, MPARAM);
};

#endif // nsDragService_h__
