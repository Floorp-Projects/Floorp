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

#ifndef nsTextAreaWidget_h__
#define nsTextAreaWidget_h__

#include "nsWidget.h"
#include "nsTextHelper.h"
#include "nsITextAreaWidget.h"
#include <qmultilineedit.h>

//=============================================================================
//
// nsQMultiLineEdit class
//
//=============================================================================
class nsQMultiLineEdit : public QMultiLineEdit, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQMultiLineEdit(nsWidget * widget,
                     QWidget * parent = 0, 
                     const char * name = 0);
    ~nsQMultiLineEdit();
};

/**
 * Native QT multi-line edit control wrapper.
 */

class nsTextAreaWidget : public nsTextHelper
{

public:
    nsTextAreaWidget();
    virtual ~nsTextAreaWidget();

    NS_DECL_ISUPPORTS

    virtual PRBool OnPaint(nsPaintEvent & aEvent);
    virtual PRBool OnResize(nsRect &aRect);

protected:
    NS_METHOD CreateNative(QWidget *parentWindow);
};


#endif // nsTextAreaWidget_h__
