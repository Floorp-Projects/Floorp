/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <windows.h>

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsRepository.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);


NS_IMPL_ADDREF(nsTextWidget)
NS_IMPL_RELEASE(nsTextWidget)


//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget() : nsTextHelper()
{
  NS_INIT_REFCNT();
  mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextWidgetIID, NS_ITEXTWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextWidget*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
    }

    return result;
}

// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
// this is need for filtering out the "ding" when the return key is pressed
//
// -----------------------------------------------------------------------
void nsTextWidget::SubclassWindow(BOOL bState)
{
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = (WNDPROC)::SetWindowLong(mWnd, GWL_WNDPROC, 
                                                 (LONG)nsTextWidget::TextWindowProc);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        ::SetWindowLong(mWnd, GWL_USERDATA, (LONG)this);
    } 
    else {
        (void) ::SetWindowLong(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
        mPrevWndProc = NULL;
    }
}

//-------------------------------------------------------------------------
//
// the nsTextWidget procedure for all nsTextWidget in this toolkit
// this is need for filtering out the "ding" when the return key is pressed
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsTextWidget::TextWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Filters the "ding" when hitting the return key
    if (msg == WM_CHAR) {
      long chCharCode = (TCHAR) wParam;    // character code 
      if (chCharCode == 13 || chCharCode == 9) {
        return 0L;
      }
    }

    return nsWindow::WindowProc(hWnd, msg, wParam, lParam);
}


//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsTextWidget::OnPaint()
{
    return PR_FALSE;
}


PRBool nsTextWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsTextWidget::WindowClass()
{
  return(nsTextHelper::WindowClass());
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsTextWidget::WindowStyle()
{
  return(nsTextHelper::WindowStyle());
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------

DWORD nsTextWidget::WindowExStyle()
{
    return WS_EX_CLIENTEDGE;
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsTextWidget::GetBounds(nsRect &aRect)
{
    nsWindow::GetNonClientBounds(aRect);
    return NS_OK;
}

/**
 * Renders the TextWidget for Printing
 *
 **/
NS_METHOD nsTextWidget::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  nsRect rect;
  float  appUnits;
  float  scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);

  context->GetCanonicalPixelScale(scale);
  context->GetDevUnitsToAppUnits(appUnits);

  GetBoundsAppUnits(rect, appUnits);

  aRenderingContext.SetColor(NS_RGB(0,0,0));

  nscolor bgColor  = NS_RGB(255,255,255);
  nscolor fgColor  = NS_RGB(0,0,0);
  nscolor hltColor = NS_RGB(240,240,240);
  nscolor sdwColor = NS_RGB(128,128,128);
  nscolor txtBGColor = NS_RGB(255,255,255);
  nscolor txtFGColor = NS_RGB(0,0,0);
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsRepository::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  bgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetForeground,  fgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DShadow,    sdwColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DHighlight, hltColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_TextBackground,    txtBGColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_TextForeground,    txtFGColor);
  }

  aRenderingContext.SetColor(txtBGColor);
  aRenderingContext.FillRect(rect);

  // Paint Black border
  //nsBaseWidget::Paint(aRenderingContext, aDirtyRect);

  nscoord onePixel  = nscoord(scale);
  nscoord twoPixels = nscoord(scale*2);

  rect.x      += onePixel; 
  rect.y      += onePixel;
  rect.width  -= twoPixels+onePixel; 
  rect.height -= twoPixels+onePixel;

  nscoord right     = rect.x+rect.width;
  nscoord bottom    = rect.y+rect.height;


  // Draw Left & Top
  aRenderingContext.SetColor(NS_RGB(128,128,128));
  DrawScaledLine(aRenderingContext, rect.x, rect.y, right, rect.y, scale, appUnits, PR_TRUE); // top
  DrawScaledLine(aRenderingContext, rect.x, rect.y, rect.x, bottom, scale, appUnits, PR_FALSE); // left

  //DrawScaledLine(aRenderingContext, rect.x+onePixel, rect.y+onePixel, right-onePixel, rect.y+onePixel, scale, appUnits, PR_TRUE); // top + 1
  //DrawScaledLine(aRenderingContext, rect.x+onePixel, rect.y+onePixel, rect.x+onePixel, bottom-onePixel, scale, appUnits, PR_FALSE); // left + 1

  // Draw Right & Bottom
  aRenderingContext.SetColor(NS_RGB(192,192,192));
  DrawScaledLine(aRenderingContext, right, rect.y+onePixel, right, bottom, scale, appUnits, PR_FALSE); // right 
  DrawScaledLine(aRenderingContext, rect.x+onePixel, bottom, right, bottom, scale, appUnits, PR_TRUE); // bottom

  //DrawScaledLine(aRenderingContext, right-onePixel, rect.y+twoPixels, right-onePixel, bottom, scale, appUnits, PR_FALSE); // right + 1
  //DrawScaledLine(aRenderingContext, rect.x+twoPixels, bottom-onePixel, right, bottom-onePixel, scale, appUnits, PR_TRUE); // bottom + 1
  

  aRenderingContext.SetFont(*mFont);

  nscoord textWidth;
  nscoord textHeight;
  aRenderingContext.GetWidth(mText, textWidth);

  nsIFontMetrics* metrics;
  context->GetMetricsFor(*mFont, metrics);
  metrics->GetMaxAscent(textHeight);

  nscoord x = (twoPixels * 2)  + rect.x;
  nscoord y = ((rect.height - textHeight) / 2) + rect.y;
  aRenderingContext.SetColor(txtFGColor);
  if (!mIsPassword) {
    aRenderingContext.DrawString(mText, x, y);
  } else {
    nsString astricks;
    PRInt32 i;
    for (i=0;i<mText.Length();i++) {
      astricks.Append("*");
    }
    aRenderingContext.DrawString(astricks, x, y);

  }

  NS_RELEASE(context);

  return NS_OK;
}


