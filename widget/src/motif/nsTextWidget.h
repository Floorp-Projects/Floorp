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

/**
 * Native Motif single line edit control wrapper. 
 */

class nsTextWidget : public nsTextHelper
{

public:
  nsTextWidget(nsISupports *aOuter);
  virtual ~nsTextWidget();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  void Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);

  void Create(nsNativeWindow aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


  virtual PRBool  OnPaint();
  virtual PRBool  OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool  OnResize(nsRect &aWindowRect);

  // nsTextHelper Interface
  virtual void      SelectAll();
  virtual void      SetMaxTextLength(PRUint32 aChars);
  virtual PRUint32  GetText(nsString& aTextBuffer, PRUint32 aBufferSize);
  virtual PRUint32  SetText(const nsString& aText);
  virtual PRUint32  InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos);
  virtual void      RemoveText();
  virtual void      SetPassword(PRBool aIsPassword);
  virtual PRBool    SetReadOnly(PRBool aReadOnlyFlag);
  virtual void      SetSelection(PRUint32 aStartSel, PRUint32 aEndSel);
  virtual void      GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel);
  virtual void      SetCaretPosition(PRUint32 aPosition);
  virtual PRUint32  GetCaretPosition();
  virtual void      PreCreateWidget(nsWidgetInitData *aInitData);
  virtual PRBool    AutoErase();

protected:
    PRBool  mIsPasswordCallBacksInstalled;

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsTextWidget,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIText interface to nsText w/o using multiple
  // inheritance.
  class AggTextWidget : public nsITextWidget {
  public:
    AggTextWidget();
    virtual ~AggTextWidget();

    AGGRRGATE_METHOD_DEF

    virtual void      SelectAll();
    virtual void      SetMaxTextLength(PRUint32 aChars);
    virtual PRUint32  GetText(nsString& aTextBuffer, PRUint32 aBufferSize);
    virtual PRUint32  SetText(const nsString& aText);
    virtual PRUint32  InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos);
    virtual void      RemoveText();
    virtual void      SetPassword(PRBool aIsPassword);
    virtual PRBool    SetReadOnly(PRBool aReadOnlyFlag);
    virtual void      SetSelection(PRUint32 aStartSel, PRUint32 aEndSel);
    virtual void      GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel);
    virtual void      SetCaretPosition(PRUint32 aPosition);
    virtual PRUint32  GetCaretPosition();
    virtual void      PreCreateWidget(nsWidgetInitData *aInitData);
    virtual PRBool    AutoErase();

  };
  AggTextWidget mAggWidget;


};

#endif // nsTextWidget_h__
