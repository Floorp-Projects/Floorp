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

#ifndef nsTextWidget_h__
#define nsTextWidget_h__

#include "nsWidget.h"
#include "nsTextHelper.h"
#include "nsITextWidget.h"
#include <qlineedit.h>

//=============================================================================
//
// nsQLineEdit class
//
//=============================================================================
class nsQLineEdit : public QLineEdit, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQLineEdit(nsWidget * widget,
                QWidget * parent = 0, 
                const char * name = 0);
    ~nsQLineEdit();
};

/**
 * Native QT single line edit control wrapper.
 */

class nsTextWidget : public nsTextHelper
{

public:
    nsTextWidget();
    virtual ~nsTextWidget();

    NS_DECL_ISUPPORTS_INHERITED
    
    virtual PRBool OnPaint(nsPaintEvent & aEvent);
    virtual PRBool OnResize(nsRect &aRect);
};

#endif // nsTextWidget_h__
