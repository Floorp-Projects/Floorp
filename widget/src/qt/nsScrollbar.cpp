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

#include "nsScrollbar.h"
#include "nsGUIEvent.h"

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gQBaseSBCount = 0;
PRUint32 gQBaseSBID = 0;

PRUint32 gQSBCount = 0;
PRUint32 gQSBID = 0;

PRUint32 gNsSBCount = 0;
PRUint32 gNsSBID = 0;
#endif

//=============================================================================
// nsQBaseScrollBar class
//=============================================================================
nsQBaseScrollBar::nsQBaseScrollBar(nsWidget *aWidget)
      : nsQBaseWidget(aWidget)
{
#ifdef DBG_JCG
  gQBaseSBCount++;
  mQBaseSBID = gQBaseSBID++;
  printf("JCG: nsQBaseScrollBar CTOR (%p) ID: %d, Count: %d\n",
         this,mQBaseSBID,gQBaseSBCount);
#endif
}

nsQBaseScrollBar::~nsQBaseScrollBar()
{
#ifdef DBG_JCG
  gQBaseSBCount--;
  printf("JCG: nsQBaseScrollBar DTOR (%p) ID: %d, Count: %d\n",
         this,mQBaseSBID,gQBaseSBCount);
#endif
}

void nsQBaseScrollBar::ValueChanged(int aValue)
{
  ScrollBarMoved(NS_SCROLLBAR_POS,aValue);
}

void nsQBaseScrollBar::ScrollBarMoved(int aMessage,int aValue)
{
  if (mWidget) {
     nsScrollbarEvent nsEvent;

     nsEvent.message         = aMessage;
     nsEvent.widget          = mWidget;
     nsEvent.eventStructType = NS_SCROLLBAR_EVENT;
     nsEvent.position        = aValue;
        
     ((nsScrollbar*)mWidget)->OnScroll(nsEvent,aValue);
  }
}

PRBool nsQBaseScrollBar::CreateNative(int aMinValue,int aMaxValue,
                                      int aLineStep,int aPageStep,int aValue,
                                      Orientation aOrientation,QWidget *aParent,
                                      const char *aName)
{
  if (!(mQWidget = new nsQScrollBar(aMinValue,aMaxValue,aLineStep,aPageStep,aValue,
                                    aOrientation,aParent,aName))) {
    return PR_FALSE;
  }
  connect((QScrollBar*)mQWidget,SIGNAL(valueChanged(int)),this,SLOT(ValueChanged(int)));
  mQWidget->installEventFilter(this);
  return PR_TRUE;
}

void nsQBaseScrollBar::Destroy()
{
  mEnabled = PR_FALSE;

  if (mWidget) {
    mWidget = nsnull;
  }
}

PRBool nsQBaseScrollBar::MouseButtonEvent(QMouseEvent *aEvent,PRBool aButtonDown,
                                          int aClickCount)
{
  return PR_FALSE;
}

PRBool nsQBaseScrollBar::MouseMovedEvent(QMouseEvent *aEvent)
{
  return PR_FALSE;
}

PRBool nsQBaseScrollBar::PaintEvent(QPaintEvent *aEvent)
{
  return PR_FALSE;
}

//=============================================================================
// nsQScrollBar class
//=============================================================================
nsQScrollBar::nsQScrollBar(int aMinValue,int aMaxValue,
                           int aLineStep,int aPageStep,int aValue,
                           Orientation aOrientation,QWidget *aParent,
                           const char *aName)
              : QScrollBar(aMinValue,aMaxValue,aLineStep,aPageStep,aValue,
                           aOrientation,aParent,aName)
{
#ifdef DBG_JCG
  gQSBCount++;
  mQSBID = gQSBID++;
  printf("JCG: nsQScrollBar CTOR (%p) ID: %d, Count: %d\n",this,mQSBID,gQSBCount);
#endif
  setMouseTracking(PR_TRUE);
  setTracking(PR_TRUE);
}

nsQScrollBar::~nsQScrollBar()
{
#ifdef DBG_JCG
  gQSBCount--;
  printf("JCG: nsQScrollBar DTOR (%p) ID: %d, Count: %d\n",this,mQSBID,gQSBCount);
#endif
}

void nsQScrollBar::closeEvent(QCloseEvent *aEvent)
{
  aEvent->ignore();
}

//=============================================================================
// nsScrollBar class
//=============================================================================
NS_IMPL_ADDREF_INHERITED(nsScrollbar,nsWidget);
NS_IMPL_RELEASE_INHERITED(nsScrollbar,nsWidget);
NS_IMPL_QUERY_INTERFACE2(nsScrollbar,nsIScrollbar,nsIWidget)

//-------------------------------------------------------------------------
// nsScrollbar constructor
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWidget(), nsIScrollbar()
{
#ifdef DBG_JCG
  gNsSBCount++;
  mNsSBID = gNsSBID++;
  printf("JCG: nsScrollBar CTOR (%p) ID: %d, Count: %d\n",this,mNsSBID,gNsSBCount);
#endif
  mOrientation = aIsVertical ? QScrollBar::Vertical : QScrollBar::Horizontal;
  mLineStep = 1;
  mPageStep = 10;
  mMaxValue = 100;
  mValue    = 0;
  mListenForResizes = PR_TRUE;
}

//-------------------------------------------------------------------------
// nsScrollbar destructor
//-------------------------------------------------------------------------
nsScrollbar::~nsScrollbar()
{
#ifdef DBG_JCG
  gNsSBCount--;
  printf("JCG: nsScrollBar DTOR (%p) ID: %d, Count: %d\n",this,mNsSBID,gNsSBCount);
#endif
}

//-------------------------------------------------------------------------
// Define the range settings
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  mMaxValue = aEndRange;
  ((nsQBaseScrollBar*)mWidget)->SetRange(0,mMaxValue - mPageStep);
  return NS_OK;
}

//-------------------------------------------------------------------------
// Return the range settings
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetMaxRange(PRUint32 &aMaxRange)
{
  aMaxRange = mMaxValue;
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set the thumb position
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  mValue = aPos;
  ((nsQBaseScrollBar*)mWidget)->SetValue(mValue);
  return NS_OK;
}

//-------------------------------------------------------------------------
// Get the current thumb position.
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetPosition(PRUint32 &aPos)
{
  aPos = mValue;
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set the thumb size
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  if (aSize > 0) {
    mPageStep = aSize;
        
    ((nsQBaseScrollBar*)mWidget)->SetSteps(mLineStep,mPageStep);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Get the thumb size
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetThumbSize(PRUint32 &aThumbSize)
{
  aThumbSize = mPageStep;
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set the line increment for this scrollbar
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
  if (aLineIncrement > 0) {
    mLineStep = aLineIncrement;

    ((nsQBaseScrollBar*)mWidget)->SetSteps(mLineStep,mPageStep);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Get the line increment for this scrollbar
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32 &aLineInc)
{
  aLineInc = mLineStep;
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set all scrolling parameters
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange,PRUint32 aThumbSize,
                                     PRUint32 aPosition,PRUint32 aLineIncrement)
{
  mPageStep = (aThumbSize > 0) ? aThumbSize : 1;
  mValue    = (aPosition > 0) ? aPosition : 0;
  mLineStep = (aLineIncrement > 0) ? aLineIncrement : 1;
  mMaxValue = (aMaxRange > 0) ? aMaxRange : 10;
    
  ((nsQBaseScrollBar*)mWidget)->SetValue(mValue);
  ((nsQBaseScrollBar*)mWidget)->SetSteps(mLineStep,mPageStep);
  ((nsQBaseScrollBar*)mWidget)->SetRange(0,mMaxValue - mPageStep);
  return NS_OK;
}

//-------------------------------------------------------------------------
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnScroll(nsScrollbarEvent &aEvent,PRUint32 cPos)
{
  PRBool result = PR_TRUE;

  switch (aEvent.message) {
    // scroll one line right or down
    case NS_SCROLLBAR_LINE_NEXT:
      ((nsQBaseScrollBar*)mWidget)->AddLine();
      mValue = ((nsQBaseScrollBar*)mWidget)->Value();

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) {
        aEvent.position = (PRUint32)mValue;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        mValue = aEvent.position;
      }
      break;

    // scroll one line left or up
    case NS_SCROLLBAR_LINE_PREV:
      ((nsQBaseScrollBar*)mWidget)->SubtractLine();
      mValue = ((nsQBaseScrollBar*)mWidget)->Value();

      // if an event callback is registered, give it the chance
      // to change the decrement
      if (mEventCallback) {
        aEvent.position = (PRUint32)mValue;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        mValue = aEvent.position;
      }
      break;

    // Scrolls one page right or down
    case NS_SCROLLBAR_PAGE_NEXT:
      ((nsQBaseScrollBar*)mWidget)->AddPage();
      mValue = ((nsQBaseScrollBar*)mWidget)->Value();

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) {
        aEvent.position = (PRUint32)mValue;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        mValue = aEvent.position;
      }
      break;

    // Scrolls one page left or up.
    case NS_SCROLLBAR_PAGE_PREV:
      ((nsQBaseScrollBar*)mWidget)->SubtractPage();
      mValue = ((nsQBaseScrollBar*)mWidget)->Value();

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) {
        aEvent.position = (PRUint32)mValue;
        result = ConvertStatus((*mEventCallback)(&aEvent));
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
        result = ConvertStatus((*mEventCallback)(&aEvent));
        mValue = aEvent.position;
      }
      break;
    }
    return result;
}

//-------------------------------------------------------------------------
// Create the native scrollbar widget
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::CreateNative(QWidget *aParentWindow)
{
  mWidget = new nsQBaseScrollBar(this);

  if (!mWidget)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!((nsQBaseScrollBar*)mWidget)->CreateNative(0,mMaxValue,mLineStep,mPageStep,
                                                  mValue,mOrientation,aParentWindow,
                                                  QScrollBar::tr("nsScrollBar"))) {
    delete mWidget;
    mWidget = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return nsWidget::CreateNative(aParentWindow);
}
