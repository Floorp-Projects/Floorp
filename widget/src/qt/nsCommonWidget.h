/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#ifndef NSCOMMONWIDGET_H
#define NSCOMMONWIDGET_H

#include "nsBaseWidget.h"

#include "nsEvent.h"

#include <qevent.h> //XXX switch for forward-decl

class nsIToolkit;
class nsWidgetInitData;
class nsIDeviceContext;
class nsIAppShell;
class nsIFontMetrics;
class nsColorMap;
class nsFont;
class nsRect;
class nsAString;
class nsIMenuBar;
class nsGUIEvent;
class nsIRollupListener;
class QWidget;
class nsQtEventDispatcher;

class nsCommonWidget : public nsBaseWidget
{
public:
    nsCommonWidget();
    ~nsCommonWidget();

    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD Show(PRBool);
    NS_IMETHOD IsVisible(PRBool&);

    NS_IMETHOD ConstrainPosition(PRBool, PRInt32*, PRInt32*);
    NS_IMETHOD Move(PRInt32, PRInt32);
    NS_IMETHOD Resize(PRInt32, PRInt32, PRBool);
    NS_IMETHOD Resize(PRInt32, PRInt32, PRInt32, PRInt32, PRBool);
    NS_IMETHOD Enable(PRBool);
    NS_IMETHOD IsEnabled(PRBool*);
    NS_IMETHOD SetFocus(PRBool araise = PR_FALSE);

    virtual nsIFontMetrics* GetFont();

    NS_IMETHOD SetFont(const nsFont&);
    NS_IMETHOD Invalidate(PRBool);
    NS_IMETHOD Invalidate(const nsRect&, int);
    NS_IMETHOD Update();
    NS_IMETHOD SetColorMap(nsColorMap*);
    NS_IMETHOD Scroll(PRInt32, PRInt32, nsRect*);
    NS_IMETHOD ScrollWidgets(PRInt32 aDx, PRInt32 aDy);

    NS_IMETHOD SetModal(PRBool aModal);

    virtual void* GetNativeData(PRUint32);

    NS_IMETHOD SetTitle(const nsAString&);
    NS_IMETHOD SetMenuBar(nsIMenuBar*);
    NS_IMETHOD ShowMenuBar(PRBool);
    NS_IMETHOD GetScreenBounds(nsRect &aRect);
    NS_IMETHOD WidgetToScreen(const nsRect&, nsRect&);
    NS_IMETHOD ScreenToWidget(const nsRect&, nsRect&);
    NS_IMETHOD BeginResizingChildren();
    NS_IMETHOD EndResizingChildren();
    NS_IMETHOD GetPreferredSize(PRInt32&, PRInt32&);
    NS_IMETHOD SetPreferredSize(PRInt32, PRInt32);
    NS_IMETHOD DispatchEvent(nsGUIEvent*, nsEventStatus&);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener*, PRBool, PRBool);

    // nsIWidget
    NS_IMETHOD         Create(nsIWidget        *aParent,
                              const nsRect     &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell      *aAppShell,
                              nsIToolkit       *aToolkit,
                              nsWidgetInitData *aInitData);
    NS_IMETHOD         Create(nsNativeWidget aParent,
                              const nsRect     &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell      *aAppShell,
                              nsIToolkit       *aToolkit,
                              nsWidgetInitData *aInitData);

    nsCursor GetCursor();
    NS_METHOD SetCursor(nsCursor aCursor);

protected:
    /**
     * Event handlers (proxied from the actual qwidget).
     * They follow normal Qt widget semantics.
     */
    friend class nsQtEventDispatcher;
    friend class InterceptContainer;
    friend class MozQWidget;

    virtual bool mousePressEvent(QMouseEvent *);
    virtual bool mouseReleaseEvent(QMouseEvent *);
    virtual bool mouseDoubleClickEvent(QMouseEvent *);
    virtual bool mouseMoveEvent(QMouseEvent *);
    virtual bool wheelEvent(QWheelEvent *);
    virtual bool keyPressEvent(QKeyEvent *);
    virtual bool keyReleaseEvent(QKeyEvent *);
    virtual bool focusInEvent(QFocusEvent *);
    virtual bool focusOutEvent(QFocusEvent *);
    virtual bool enterEvent(QEvent *);
    virtual bool leaveEvent(QEvent *);
    virtual bool paintEvent(QPaintEvent *);
    virtual bool moveEvent(QMoveEvent *);
    virtual bool resizeEvent(QResizeEvent *);
    virtual bool closeEvent(QCloseEvent *);
    virtual bool contextMenuEvent(QContextMenuEvent *);
    virtual bool imStartEvent(QIMEvent *);
    virtual bool imComposeEvent(QIMEvent *);
    virtual bool imEndEvent(QIMEvent *);
    virtual bool dragEnterEvent(QDragEnterEvent *);
    virtual bool dragMoveEvent(QDragMoveEvent *);
    virtual bool dragLeaveEvent(QDragLeaveEvent *);
    virtual bool dropEvent(QDropEvent *);
    virtual bool showEvent(QShowEvent *);
    virtual bool hideEvent(QHideEvent *);

protected:
    virtual QWidget  *createQWidget(QWidget *parent, nsWidgetInitData *aInitData) = 0;
    virtual void NativeResize(PRInt32, PRInt32, PRInt32, PRInt32, PRBool);
    virtual void NativeResize(PRInt32, PRInt32, PRBool);
    virtual void NativeShow(PRBool);

    bool ignoreEvent(nsEventStatus aStatus) const;

    /**
     * Has to be called in subclasses after they created
     * the actual QWidget if they overwrite the Create
     * calls from the nsCommonWidget class.
     */
    void Initialize(QWidget *widget);

    void DispatchGotFocusEvent(void);
    void DispatchLostFocusEvent(void);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResizeEvent(nsRect &aRect, nsEventStatus &aStatus);

    void InitKeyEvent(nsKeyEvent *nsEvent, QKeyEvent *qEvent);
    void InitMouseEvent(nsMouseEvent *nsEvent, QMouseEvent *qEvent, int aClickCount);
    void InitMouseWheelEvent(nsMouseScrollEvent *aEvent, QWheelEvent *qEvent);

    void CommonCreate(nsIWidget *aParent, PRBool aListenForResizes);

    PRBool AreBoundsSane() const;

protected:
    QWidget *mContainer;
    QWidget *mWidget;
    nsQtEventDispatcher *mDispatcher;
    PRPackedBool   mListenForResizes;
    PRPackedBool   mNeedsResize;
    PRPackedBool   mNeedsShow;
    PRPackedBool   mIsShown;
    nsCOMPtr<nsIWidget> mParent;

private:
    nsresult NativeCreate(nsIWidget        *aParent,
                          QWidget          *aNativeParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK    aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData);

};

#endif
