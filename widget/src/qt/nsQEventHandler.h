/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsQTEventHandler_h
#define __nsQTEventHandler_h

class nsIWidget;
class nsIMenuItem;

//#include "nsISupports.h"
#include "nsWidget.h"

#include <qobject.h>
#include <qevent.h>
#include <map>

class nsQEventHandler : public QObject//, public nsISupports
{
    Q_OBJECT

public:
    //NS_DECL_ISUPPORTS

protected:
    nsQEventHandler();

public:
    static nsQEventHandler * Instance(void * qWidget, 
                                      nsWidget * nWidget);

public slots:
    bool eventFilter(QObject * object, QEvent * event);
    bool MousePressedEvent(QMouseEvent * event, nsWidget * widget);
    bool MouseReleasedEvent(QMouseEvent * event, nsWidget * widget);
    bool DestroyEvent(QCloseEvent * event, nsWidget * widget);
    bool ShowEvent(QShowEvent * event, nsWidget * widget);
    bool HideEvent(QHideEvent * event, nsWidget * widget);
    bool ResizeEvent(QResizeEvent * event, nsWidget * widget);
    bool MoveEvent(QMoveEvent * event, nsWidget * widget);
    bool PaintEvent(QPaintEvent * event, nsWidget * widget);

    bool KeyPressEvent(QWidget * widget);
    bool KeyReleaseEvent(QWidget * widget);
    bool FocusInEvent(QWidget * widget);
    bool FocusOutEvent(QWidget * widget);
    bool ScrollbarValueChanged(int value);
    bool TextChangedEvent(const QString & string);

private:
    static nsQEventHandler *            mInstance;
    static std::map<void *, nsWidget *> mMap;
    static QString                      mObjectName;
};

#endif  // __nsQEventHandler.h
