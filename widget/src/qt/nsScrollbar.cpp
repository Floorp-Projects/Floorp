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
#include "nsScrollbar.h"
#include "nsGUIEvent.h"

#define mScroll  ((QScrollBar*)mWidget)

nsQScrollBar::nsQScrollBar(int aMinValue, int aMaxValue, int aLineStep,
                           int aPageStep, int aValue, Orientation aOrientation,
                           QWidget *aParent, const char *aName)
    : QScrollBar(aMinValue, aMaxValue, aLineStep, aPageStep, aValue,
                 aOrientation, aParent, aName)
{
    qDebug("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    connect(this,SIGNAL(valueChanged(int)),
            SLOT(slotValueChanged(int)));
}

void nsQScrollBar::slotValueChanged(int aValue)
{
    ScrollBarMoved(NS_SCROLLBAR_POS,aValue);
}

void nsQScrollBar::ScrollBarMoved(int aMessage,int aValue)
{
    nsScrollbarEvent nsEvent;

    nsEvent.message         = aMessage;
    nsEvent.widget          = 0;
    nsEvent.eventStructType = NS_SCROLLBAR_EVENT;
    nsEvent.position        = aValue;

    //mReceiver->scrollEvent(nsEvent,aValue);
    //FIXME:
}

//=============================================================================
// nsScrollBar class
//=============================================================================
NS_IMPL_ADDREF_INHERITED(nsNativeScrollbar,nsCommonWidget)
NS_IMPL_RELEASE_INHERITED(nsNativeScrollbar,nsCommonWidget)
NS_IMPL_QUERY_INTERFACE2(nsNativeScrollbar,nsINativeScrollbar,nsIWidget)

nsNativeScrollbar::nsNativeScrollbar()
    : nsCommonWidget(),
      nsINativeScrollbar()
{

    qDebug("YESSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS");
    mOrientation = true ? QScrollBar::Vertical : QScrollBar::Horizontal;
    mLineStep = 1;
    mPageStep = 10;
    mMaxValue = 100;
    mValue    = 0;
    mListenForResizes = PR_TRUE;
}

nsNativeScrollbar::~nsNativeScrollbar()
{
}

NS_METHOD
nsNativeScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    mMaxValue = aEndRange;

    mScroll->setRange(0,mMaxValue - mPageStep);

    return NS_OK;
}

NS_METHOD
nsNativeScrollbar::GetMaxRange(PRUint32 *aMaxRange)
{
    *aMaxRange = mMaxValue;
    return NS_OK;
}

NS_METHOD nsNativeScrollbar::SetPosition(PRUint32 aPos)
{
    mValue = aPos;

    mScroll->setValue(mValue);

    return NS_OK;
}

NS_METHOD
nsNativeScrollbar::GetPosition(PRUint32 *aPos)
{
    *aPos = mValue;
    return NS_OK;
}

//-------------------------------------------------------------------------
// Set the line increment for this scrollbar
//-------------------------------------------------------------------------
NS_METHOD
nsNativeScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
    if (aLineIncrement > 0) {
        mLineStep = aLineIncrement;

        mScroll->setSteps(mLineStep,mPageStep);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
// Get the line increment for this scrollbar
//-------------------------------------------------------------------------
NS_METHOD
nsNativeScrollbar::GetLineIncrement(PRUint32 *aLineInc)
{
    *aLineInc = mLineStep;
    return NS_OK;
}

#if 0
NS_METHOD
nsNativeScrollbar::SetParameters(PRUint32 aMaxRange,PRUint32 aThumbSize,
                           PRUint32 aPosition,PRUint32 aLineIncrement)
{
    mPageStep = (aThumbSize > 0) ? aThumbSize : 1;
    mValue    = (aPosition > 0) ? aPosition : 0;
    mLineStep = (aLineIncrement > 0) ? aLineIncrement : 1;
    mMaxValue = (aMaxRange > 0) ? aMaxRange : 10;

    mScroll->setValue(mValue);
    mScroll->setSteps(mLineStep,mPageStep);
    mScroll->setRange(0,mMaxValue - mPageStep);
    return NS_OK;
}
#endif

//-------------------------------------------------------------------------
// Deal with scrollbar messages (actually implemented only in nsNativeScrollbar)
//-------------------------------------------------------------------------
PRBool nsNativeScrollbar::OnScroll(nsScrollbarEvent &aEvent,PRUint32 cPos)
{
    nsEventStatus result = nsEventStatus_eIgnore;

    switch (aEvent.message) {
        // scroll one line right or down
    case NS_SCROLLBAR_LINE_NEXT:
        mScroll->addLine();
        mValue = mScroll->value();

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) {
            aEvent.position = (PRUint32)mValue;
            result = (*mEventCallback)(&aEvent);
            mValue = aEvent.position;
        }
        break;

        // scroll one line left or up
    case NS_SCROLLBAR_LINE_PREV:
        mScroll->subtractLine();
        mValue = mScroll->value();

        // if an event callback is registered, give it the chance
        // to change the decrement
        if (mEventCallback) {
            aEvent.position = (PRUint32)mValue;
            result = (*mEventCallback)(&aEvent);
            mValue = aEvent.position;
        }
        break;

        // Scrolls one page right or down
    case NS_SCROLLBAR_PAGE_NEXT:
        mScroll->addPage();
        mValue = mScroll->value();

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) {
            aEvent.position = (PRUint32)mValue;
            result = (*mEventCallback)(&aEvent);
            mValue = aEvent.position;
        }
        break;

        // Scrolls one page left or up.
    case NS_SCROLLBAR_PAGE_PREV:
        mScroll->subtractPage();
        mValue = mScroll->value();

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) {
            aEvent.position = (PRUint32)mValue;
            result = (*mEventCallback)(&aEvent);
            mValue = aEvent.position;
        }
        break;

        // Scrolls to the absolute position. The current position is specified by
        // the cPos parameter.
    case NS_SCROLLBAR_POS:
        mValue = cPos;

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) {
            aEvent.position = (PRUint32)mValue;
            result = (*mEventCallback)(&aEvent);
            mValue = aEvent.position;
        }
        break;
    }

    return ignoreEvent(result);
}

QWidget*
nsNativeScrollbar::createQWidget(QWidget *parent, nsWidgetInitData */*aInitData*/)
{
    mWidget =  new nsQScrollBar(0, mMaxValue, mLineStep, mPageStep,
                                mValue, mOrientation, parent);
    mWidget->setMouseTracking(PR_TRUE);

    return mWidget;
}

/* void setContent (in nsIContent content, in nsISupports scrollbar, in nsIScrollbarMediator mediator); */
NS_IMETHODIMP
nsNativeScrollbar::SetContent(nsIContent *content, nsISupports *scrollbar, nsIScrollbarMediator *mediator)
{
    qDebug("XXXXXXXX SetContent");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long narrowSize; */
NS_IMETHODIMP
nsNativeScrollbar::GetNarrowSize(PRInt32 *aNarrowSize)
{
    qDebug("XXXXXXXXXXXX GetNarrowSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}
/* attribute unsigned long viewSize; */
NS_IMETHODIMP
nsNativeScrollbar::GetViewSize(PRUint32 *aViewSize)
{
    qDebug("XXXXXXXXX GetViewSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsNativeScrollbar::SetViewSize(PRUint32 aViewSize)
{
    qDebug("XXXXXXXXXXX SetViewSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}
