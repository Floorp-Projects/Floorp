/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
 */

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include <Pt.h>

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include <nsCOMPtr.h>

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Clipboard wrapper
 */

class nsClipboard : public nsIClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS

  // nsIClipboard
  NS_DECL_NSICLIPBOARD

protected:
  NS_IMETHOD SetNativeClipboardData(PRInt32 aWhichClipboard);
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable, 
                                    PRInt32 aWhichClipboard );
nsresult GetFormat(const char* aMimeStr, char *format );

  PRBool  mIgnoreEmptyNotification;
	inline nsITransferable *GetTransferable(PRInt32 aWhichClipboard);

private:
  nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
  nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
  nsCOMPtr<nsITransferable>   mSelectionTransferable;
  nsCOMPtr<nsITransferable>   mGlobalTransferable;

  // Used for communicating pasted data
  // from the asynchronous X routines back to a blocking paste:
  PRBool mBlocking;
};

#endif // nsClipboard_h__
