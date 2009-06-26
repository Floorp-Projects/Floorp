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
  NS_IMETHOD EndDragSession(PRBool aDoneDrag);

    // nsIDragSession
  NS_IMETHOD GetNumDropItems(PRUint32* aNumDropItems);
  NS_IMETHOD GetData(nsITransferable* aTransferable, PRUint32 aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor, PRBool* _retval);

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
  NS_IMETHOD NativeDrop(PDRAGINFO pdinfo, HWND hwnd, PRBool* rendering);
  NS_IMETHOD NativeRenderComplete(PDRAGTRANSFER pdxfer, USHORT usResult);
  NS_IMETHOD NativeDataToTransferable( PCSZ pszText, PCSZ pszTitle,
                                       PRBool isUrl);

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
