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
 */

#ifndef __nsQTEventHandler_h
#define __nsQTEventHandler_h

class nsIWidget;
class nsIMenuItem;

#include "nsWidget.h"

#include <qobject.h>
#include <qevent.h>

class nsQEventHandler : public QObject
{
    Q_OBJECT

public:
    nsQEventHandler(nsWidget *aWidget);
    ~nsQEventHandler();

    void Enable(bool aEnable);
    void Destroy(void);
    bool eventFilter(QObject * object, QEvent * event);

    bool MouseButtonEvent(QMouseEvent *event,nsWidget *widget,
                          bool buttonDown,int clickCnt);
    bool MouseMovedEvent(QMouseEvent * event, nsWidget * widget);
    bool MouseEnterEvent(QEvent * event, nsWidget * widget);
    bool MouseExitEvent(QEvent * event, nsWidget * widget);
    bool DestroyEvent(QCloseEvent * event, nsWidget * widget);
    bool HideEvent(QHideEvent * event, nsWidget * widget);
    bool ResizeEvent(QResizeEvent * event, nsWidget * widget);
    bool MoveEvent(QMoveEvent * event, nsWidget * widget);
    bool PaintEvent(QPaintEvent * event, nsWidget * widget);
    bool KeyPressEvent(QKeyEvent * event, nsWidget * widget);
    bool KeyReleaseEvent(QKeyEvent * event, nsWidget * widget);
    bool FocusInEvent(QFocusEvent * event, nsWidget * widget);
    bool FocusOutEvent(QFocusEvent * event, nsWidget * widget);

protected:
    PRInt32 GetNSKey(PRInt32 key, PRInt32 state);

private:
    nsWidget * mWidget;
    bool       mEnabled;
    bool       mDestroyed;
};

#endif  // __nsQEventHandler.h
