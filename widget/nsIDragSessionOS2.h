/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is 
 * Richard L. Walsh <dragtext@e-vertise.com>
 * Portions created by the Initial Developer are Copyright (C) 2003
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

/**
 * nsIDragSessionOS2 encapsulates support for OS/2 dragover/drop events,
 * i.e. operations where Mozilla is the target of a drag (nsIDragService
 * handles source operations).  Its public methods correspond to the d&d
 * messages a PM window receives.  Its protected methods recast native
 * data as nsITransferable flavors during drag-enter and drop events.
 *
 * nsIDragSessionOS2 supports these native formats:  WPS Url objects,
 * files, the contents of files (Alt-drop), and text from DragText.
 * Data is presented to Mozilla as URI-text, HTML, and/or plain-text.
 * Only single-item native drops are supported.  If the dropped file
 * or text does not exist yet, nsIDragSessionOS2 will have the source
 * render the data asynchronously.
 */

#ifndef nsIDragSessionOS2_h__
#define nsIDragSessionOS2_h__

#include "nsISupports.h"

#define INCL_PM
#include <os2.h>

#define NS_IDRAGSESSIONOS2_IID_STR "bc4258b8-33ce-4624-adcb-4b62bb5164c0"
#define NS_IDRAGSESSIONOS2_IID \
  { 0xbc4258b8, 0x33ce, 0x4624, { 0xad, 0xcb, 0x4b, 0x62, 0xbb, 0x51, 0x64, 0xc0 } }

class nsIDragSessionOS2 : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONOS2_IID)

  /** the dragFlags returned by most public methods fall into two groups */

  /** Mozilla's dragover status */
  enum { DND_NONE       = 0 };                                     
  enum { DND_NATIVEDRAG = 1 };                                     
  enum { DND_MOZDRAG    = 2 };                                     
  enum { DND_INDROP     = 4 };                                     
  enum { DND_DRAGSTATUS = DND_NATIVEDRAG | DND_MOZDRAG | DND_INDROP };

  /** tasks the caller should perform */
  enum { DND_DISPATCHENTEREVENT = 16 };
  enum { DND_DISPATCHEVENT      = 32 };
  enum { DND_GETDRAGOVERRESULT  = 64 };
  enum { DND_EXITSESSION        = 128 };

  NS_IMETHOD DragOverMsg(PDRAGINFO pdinfo, MRESULT &mr, PRUint32* dragFlags) = 0;
  NS_IMETHOD GetDragoverResult(MRESULT& mr) = 0;
  NS_IMETHOD DragLeaveMsg(PDRAGINFO pdinfo, PRUint32* dragFlags) = 0;
  NS_IMETHOD DropHelpMsg(PDRAGINFO pdinfo, PRUint32* dragFlags) = 0;
  NS_IMETHOD ExitSession(PRUint32* dragFlags) = 0;
  NS_IMETHOD DropMsg(PDRAGINFO pdinfo, HWND hwnd, PRUint32* dragFlags) = 0;
  NS_IMETHOD RenderCompleteMsg(PDRAGTRANSFER pdxfer, USHORT usResult, PRUint32* dragFlags) = 0;

protected:
  NS_IMETHOD NativeDragEnter(PDRAGINFO pdinfo) = 0;
  NS_IMETHOD NativeDrop(PDRAGINFO pdinfo, HWND hwnd, bool* rendering) = 0;
  NS_IMETHOD NativeRenderComplete(PDRAGTRANSFER pdxfer, USHORT usResult) = 0;
  NS_IMETHOD NativeDataToTransferable( PCSZ pszText, PCSZ pszTitle, bool isUrl) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDragSessionOS2, NS_IDRAGSESSIONOS2_IID)

#endif  // nsIDragSessionOS2_h__

