/*
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
 *   2000/10/02     IBM Corp.                          Sync-up to M18 level
 *
 */

#ifndef _nsClipboard_h
#define _nsClipboard_h

#include "nsWidgetDefs.h"
#include "nsBaseClipboard.h"

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;
struct IDataObject;

/**
 * Native OS/2 Clipboard wrapper
 */

struct FormatRecord;

class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

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
