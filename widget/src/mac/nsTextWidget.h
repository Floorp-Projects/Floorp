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

#include "nsMacControl.h"
#include "nsITextWidget.h"
#include "nsRepeater.h"

class nsTextWidget : public nsMacControl, public nsITextWidget, public Repeater
{
private:
	typedef nsMacControl Inherited;

public:
  nsTextWidget();
  virtual ~nsTextWidget();

	// nsISupports
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD 			Create(nsIWidget *aParent,
				              const nsRect &aRect,
				              EVENT_CALLBACK aHandleEventFunction,
				              nsIDeviceContext *aContext = nsnull,
				              nsIAppShell *aAppShell = nsnull,
				              nsIToolkit *aToolkit = nsnull,
				              nsWidgetInitData *aInitData = nsnull);

  // nsWindow Interface
  virtual PRBool	DispatchMouseEvent(nsMouseEvent &aEvent);
  virtual PRBool	DispatchWindowEvent(nsGUIEvent& aEvent);
	virtual PRBool 	OnPaint(nsPaintEvent &aEvent);

	// nsITextWidget interface
  NS_IMETHOD        SetPassword(PRBool aIsPassword);
  NS_IMETHOD        SetReadOnly(PRBool aNewReadOnlyFlag, PRBool& aOldReadOnlyFlag);

  NS_IMETHOD        SetMaxTextLength(PRUint32 aChars);
  NS_IMETHOD        GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize);
  NS_IMETHOD        SetText(const nsString &aText, PRUint32& aActualSize);
  NS_IMETHOD        InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize);
  NS_IMETHOD        RemoveText();

  NS_IMETHOD        SelectAll();
  NS_IMETHOD        SetSelection(PRUint32 aStartSel, PRUint32 aEndSel);
  NS_IMETHOD        GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel);
  NS_IMETHOD        SetCaretPosition(PRUint32 aPosition);
  NS_IMETHOD        GetCaretPosition(PRUint32& aPosition);

	// Repeater interface
	virtual	void	RepeatAction(const EventRecord& inMacEvent);

protected:

	virtual void		GetRectForMacControl(nsRect &outRect);

	PRBool		mIsPassword;
	PRBool		mIsReadOnly;
	TEHandle	mTE;
};

#endif // nsTextWidget_h__
