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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   2000/10/02     IBM Corp.                          Sync-up to M18 level
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

#ifndef _nsClipboard_h
#define _nsClipboard_h

#include "nsWidgetDefs.h"
#include "nsBaseClipboard.h"
#include "nsIObserver.h"

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;
struct IDataObject;

/**
 * Native OS/2 Clipboard wrapper
 */

struct FormatRecord;

class nsClipboard : public nsBaseClipboard,
		    public nsIObserver
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // nsIClipboard
  NS_IMETHOD ForceDataToClipboard(PRInt32 aWhichClipboard);
  NS_IMETHOD HasDataMatchingFlavors(nsISupportsArray *aFlavorList, PRInt32 aWhichClipboard, PRBool *_retval);

protected:
  NS_IMETHOD SetNativeClipboardData(PRInt32 aWhichClipboard);
  NS_IMETHOD GetNativeClipboardData(nsITransferable *aTransferable, PRInt32 aWhichClipboard);

  enum ClipboardAction
  {
    Read,
    Write
  };

  ULONG    GetFormatID(const char *aMimeStr);
  PRBool   GetClipboardData(const char *aFlavour);
  PRBool   GetClipboardDataByID(ULONG ulFormatID, const char *aFlavor);
  void     SetClipboardData(const char *aFlavour);
  nsresult DoClipboardAction(ClipboardAction aAction);
};

#endif
