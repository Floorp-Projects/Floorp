/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWidget.h"
#include "nsQWidget.h"
#include "nsIScrollbar.h"

#include <qscrollbar.h>

//=============================================================================
// nsQScrollBar class:
//  Provides a "two-way" interface, allowing XPFE to access Qt Scrollbar
//   functionality and hooking the relevant Qt Events into the XPFE Event
//   Handling.  Inherits basic functionality from nsQBaseWidget.
//=============================================================================
class nsQBaseScrollBar : public nsQBaseWidget
{
  Q_OBJECT
public:
  nsQBaseScrollBar(nsWidget *aWidget);
  ~nsQBaseScrollBar();

  /*** Lifecycle Management ***/
  PRBool CreateNative(int aMinValue,int aMaxValue,
                      int aLineStep,int aPageStep,int aValue,
                      Orientation aOrientation,QWidget *aParent,
                      const char *aName = 0);
  virtual void Destroy();

  /*** Interface to Qt ScrollBar functionality for XPFE ***/
  virtual void SetModal(PRBool aState) {};
  void SetRange(PRUint32 aMin,PRUint32 aMax) {((QScrollBar*)mQWidget)->setRange(aMin,aMax);};
  void SetValue(PRUint32 aValue) {((QScrollBar*)mQWidget)->setValue(aValue);};
  void SetSteps(PRUint32 aLineStep,PRUint32 aPageStep)
  {
    ((QScrollBar*)mQWidget)->setSteps(aLineStep,aPageStep);
  };
  void AddLine() {((QScrollBar*)mQWidget)->addLine();};
  void SubtractLine() {((QScrollBar*)mQWidget)->subtractLine();};
  void AddPage() {((QScrollBar*)mQWidget)->addPage();};
  void SubtractPage() {((QScrollBar*)mQWidget)->subtractPage();};
  int Value() {return(((QScrollBar*)mQWidget)->value());};

  /*** Interface to XPFE Event Handling from Qt ***/
  virtual PRBool MouseButtonEvent(QMouseEvent *aEvent,PRBool aButtonDown,
                                  int aClickCount);
  virtual PRBool MouseMovedEvent(QMouseEvent *aEvent);
  virtual PRBool PaintEvent(QPaintEvent *aEvent);

  void ScrollBarMoved(int aMessage,int aValue = -1);

public slots:
  void ValueChanged(int aValue);

private:
  PRUint32 mQBaseSBID;
};

//=============================================================================
// nsQScrollBar class:
//  Customizes QScrollBar for use by XPFE
//=============================================================================
class nsQScrollBar : public QScrollBar
{
  Q_OBJECT
public:
  nsQScrollBar(int aMinValue,int aMaxValue,int aLineStep,int aPageStep,int aValue, 
               Orientation aOrientation,QWidget *aParent,const char *aName = 0);
  ~nsQScrollBar();

  void closeEvent(QCloseEvent *aEvent);

private:
  PRUint32 mQSBID;
};

//=============================================================================
// nsScrollBar class:
//  the XPFE Scrollbar
//=============================================================================
class nsScrollbar : public nsWidget, public nsIScrollbar
{
public:
  nsScrollbar(PRBool aIsVertical);
  virtual ~nsScrollbar();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIScrollBar implementation
  NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
  NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
  NS_IMETHOD SetPosition(PRUint32 aPos);
  NS_IMETHOD GetPosition(PRUint32& aPos);
  NS_IMETHOD SetThumbSize(PRUint32 aSize);
  NS_IMETHOD GetThumbSize(PRUint32& aSize);
  NS_IMETHOD SetLineIncrement(PRUint32 aSize);
  NS_IMETHOD GetLineIncrement(PRUint32& aSize);
  NS_IMETHOD SetParameters(PRUint32 aMaxRange,PRUint32 aThumbSize,
                           PRUint32 aPosition,PRUint32 aLineIncrement);

  virtual PRBool OnScroll(nsScrollbarEvent &aEvent,PRUint32 cPos);

  virtual PRBool IsScrollBar() const { return PR_TRUE; };

protected:
  NS_IMETHOD CreateNative(QWidget *aParentWindow);                             

private:
  QScrollBar::Orientation mOrientation;
  int mMaxValue;
  int mLineStep;
  int mPageStep;
  int mValue;
  PRUint32 mNsSBID;
};

#endif // nsScrollbar_h__
