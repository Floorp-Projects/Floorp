/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Hartshorn <peter@igelaus.com.au>
 */

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include <nsCOMPtr.h>
#include "nsWidget.h"

#include "X11/X.h"
#include "X11/Xlib.h"

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Xlib Clipboard wrapper
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
  void Init(void);

  static nsEventStatus PR_CALLBACK Callback(nsGUIEvent *event);
  PRBool  mIgnoreEmptyNotification;

private:
  static nsCOMPtr<nsITransferable>   mTransferable;


  // Used for communicating pasted data
  // from the asynchronous X routines back to a blocking paste:
  PRBool mBlocking;

  static Window sWindow;
  nsWidget *sWidget;
  static Display *sDisplay;

};

#endif // nsClipboard_h__
