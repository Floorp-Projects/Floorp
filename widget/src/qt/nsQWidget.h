/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *		John C. Griggs <johng@corel.com>
 *      	Denis Issoupov <denis@macadamian.com> 
 *
 */
#ifndef nsQWidget_h__
#define nsQWidget_h__

#include <qwidget.h>
#include <qstring.h>

#include "nsIWidget.h"

class nsFontQT;
class nsWidget;

/* Utility Functions */
PRBool NS_IsMouseInWindow(void *aWin,PRInt32 aMouseX,PRInt32 aMouseY);
unsigned int NS_GetQWFlags(nsBorderStyle aBorder,nsWindowType aType);

//=============================================================================
// nsQWidget class:
//  Customizes QWidget for use in XPFE
//=============================================================================
class nsQWidget : public QWidget
{
  Q_OBJECT
public:
  nsQWidget(QWidget *aParent,const char *aName = 0,WFlags aFlags = WResizeNoErase);
  virtual ~nsQWidget();

  virtual void SetModal(PRBool aState);

private:
  PRUint32 mQWidgetID;
};

//=============================================================================
// nsQBaseWidget class
//  Provides a "two-way" interface, allowing XPFE to access Qt Widget
//   functionality and hooking the relevant Qt Events into the XPFE Event
//   Handling
//=============================================================================
class nsQBaseWidget : public QObject
{
  Q_OBJECT
public:
  nsQBaseWidget(nsWidget *aWidget);
  virtual ~nsQBaseWidget();

  /*** Lifecycle Management ***/
  PRBool CreateNative(QWidget *aParent = 0,const char *aName = 0,
                      unsigned int aFlags = WResizeNoErase);
  virtual void Destroy();

  /*** Interface to Qt Widget functionality for XPFE ***/
  void Enable(PRBool aState);
  PRBool HandlePopup(void *aEvent);
  virtual void SetCursor(nsCursor aCursor);
  virtual void SetFont(nsFontQT *aFont);
  virtual void SetModal(PRBool aState)
  {
    ((nsQWidget*)mQWidget)->SetModal(aState);
  };
  virtual const char *Name() {return mQWidget->name();};
  virtual int  Width() {return mQWidget->width();};
  virtual int  Height() {return mQWidget->height();};
  virtual int  BoundsX() {return 0;};
  virtual int  BoundsY() {return 0;};
  virtual void OffsetXYToGlobal(PRInt32 *aX,PRInt32 *aY)
  {
    QPoint offset(0,0);
    offset = mQWidget->mapToGlobal(offset);
    *aX = offset.x();
    *aY = offset.y();
  }; 
  virtual void OffsetXYFromGlobal(PRInt32 *aX,PRInt32 *aY)
  {
    QPoint offset(0,0);
    offset = mQWidget->mapFromGlobal(offset);
    *aX = offset.x();
    *aY = offset.y();
  }; 
  virtual void Move(PRInt32 aX,PRInt32 aY) {mQWidget->move(aX,aY);};
  virtual void Resize(PRInt32 aW,PRInt32 aH) {mQWidget->resize(aW,aH);};
  virtual void Scroll(PRInt32 aDx,PRInt32 aDy) {mQWidget->scroll(aDx,aDy);};
  virtual void Show() {mQWidget->show();};
  virtual void Hide() {mQWidget->hide();};
  virtual bool IsVisible() {return mQWidget->isVisible();};
  virtual void SetFocus() {mQWidget->setFocus();};
  virtual void SetTitle(const char *aTitleStr)
  {
    mQWidget->setCaption(QString::fromLocal8Bit(aTitleStr));
  };
  virtual void SetBackgroundColor(const nscolor &aColor)
  {
    QColor color(NS_GET_R(aColor),NS_GET_G(aColor),NS_GET_B(aColor));
    mQWidget->setBackgroundColor(color);
  };
  virtual void Repaint(PRBool aErase) {mQWidget->repaint(aErase);};
  virtual void Repaint(PRInt32 aX,PRInt32 aY,PRInt32 aW, PRInt32 aH,PRBool aErase)
  {
     mQWidget->repaint(aX,aY,aW,aH,aErase);
  };
  virtual void Update() {mQWidget->update();};
  virtual void Update(PRInt32 aX,PRInt32 aY,PRInt32 aW, PRInt32 aH)
  {
     mQWidget->update(aX,aY,aW,aH);
  };
  virtual void *GetNativeWindow() {return((void*)(QPaintDevice*)mQWidget);};
  virtual void *GetNativeWidget() {return((void*)mQWidget);};

  virtual void* X11Display() {return (void*)mQWidget->x11Display();};
  virtual void* WinID() {return (void*)mQWidget->winId();};

  /*** Interface to XPFE Event Handling from Qt ***/
  bool eventFilter(QObject *aObj,QEvent *aEvent);

  virtual PRBool MouseButtonEvent(QMouseEvent *aEvent,PRBool aButtonDown,
                                  int aClickCount);
  virtual PRBool MouseMovedEvent(QMouseEvent *aEvent);
  virtual PRBool MouseEnterEvent(QEvent *aEvent);
  virtual PRBool MouseExitEvent(QEvent *aEvent);
  virtual PRBool DestroyEvent();
  virtual PRBool ResizeEvent(QResizeEvent *aEvent);
  virtual PRBool MoveEvent(QMoveEvent *aEvent);
  virtual PRBool PaintEvent(QPaintEvent *aEvent);
  virtual PRBool KeyPressEvent(QKeyEvent *aEvent);
  virtual PRBool KeyReleaseEvent(QKeyEvent *aEvent);
  virtual PRBool FocusInEvent();
  virtual PRBool FocusOutEvent();
  virtual PRBool DragEnterEvent(QDragEnterEvent *aEvent);
  virtual PRBool DragMoveEvent(QDragMoveEvent *aEvent);
  virtual PRBool DragLeaveEvent(QDragLeaveEvent *aEvent);
  virtual PRBool DropEvent(QDropEvent *aEvent);

protected:
  nsWidget *mWidget;
  QWidget *mQWidget;
  PRBool mEnabled;
  PRBool mDestroyed;

private:
  PRUint32 mQBaseID;
};

#endif //nsQWidget_h__
