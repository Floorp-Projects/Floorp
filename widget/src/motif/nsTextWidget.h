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
 */

#ifndef nsTextWidget_h__
#define nsTextWidget_h__

#include "nsWindow.h"
#include "nsTextHelper.h"

#include "nsITextWidget.h"

typedef struct _PasswordData {
  nsString mPassword;
  Boolean  mIgnore;
} PasswordData;

/**
 * Native Motif single line edit control wrapper. 
 */

class nsTextWidget : public nsTextHelper
{

public:
  nsTextWidget();
  virtual ~nsTextWidget();

   // nsISupports. Forware to the nsObject base class
  NS_DECL_ISUPPORTS


  NS_IMETHOD Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD SetPassword(PRBool aIsPassword);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
  virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
  virtual PRBool OnResize(nsSizeEvent &aEvent) { return PR_FALSE; }

protected:
  PRBool mIsPasswordCallBacksInstalled;

private:
  PRBool mMakeReadOnly;
  PRBool mMakePassword;

};

#endif // nsTextWidget_h__
