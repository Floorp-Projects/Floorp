/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
