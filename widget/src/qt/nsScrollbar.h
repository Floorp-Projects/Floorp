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

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWidget.h"
#include "nsIScrollbar.h"

#include <qscrollbar.h>

//=============================================================================
//
// nsQScrollBar class
//
//=============================================================================
class nsQScrollBar : public QScrollBar, public nsQBaseWidget
{
    Q_OBJECT
public:
	nsQScrollBar(nsWidget * widget,
                 int minValue, 
                 int maxValue, 
                 int LineStep, 
                 int PageStep, 
                 int value, 
                 Orientation orientation,
                 QWidget * parent, 
                 const char * name=0 );
	~nsQScrollBar();

    void ScrollBarMoved(int message, int value = -1);

public slots:
    void PreviousLine();
    void NextLine();
    void PreviousPage();
    void NextPage();
    void SetValue(int value);
};


/**
 * Native QT scrollbar wrapper.
 */

class nsScrollbar : public nsWidget,
                    public nsIScrollbar
{

public:
    nsScrollbar(PRBool aIsVertical);
    virtual ~nsScrollbar();

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsIScrollBar implementation
    NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
    NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
    NS_IMETHOD SetPosition(PRUint32 aPos);
    NS_IMETHOD GetPosition(PRUint32& aPos);
    NS_IMETHOD SetThumbSize(PRUint32 aSize);
    NS_IMETHOD GetThumbSize(PRUint32& aSize);
    NS_IMETHOD SetLineIncrement(PRUint32 aSize);
    NS_IMETHOD GetLineIncrement(PRUint32& aSize);
    NS_IMETHOD SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                             PRUint32 aPosition, PRUint32 aLineIncrement);
    virtual PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

protected:
    NS_IMETHOD CreateNative(QWidget *parentWindow);

private:
    QScrollBar::Orientation     mOrientation;
    int mMaxValue;
    int mLineStep;
    int mPageStep;
    int mValue;

};

#endif // nsScrollbar_
