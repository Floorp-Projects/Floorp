/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <windows.h>

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

//-------------------------------------------------------------------------
nsresult
NS_NewTextWidget(nsITextWidget** aControl)
{
  NS_PRECONDITION(aControl, "null OUT ptr");
  if (nsnull == aControl) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTextWidget* it = new nsTextWidget;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(it);
  *aControl = (nsITextWidget*)it;
  return NS_OK;
}

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

    if (result == NS_NOINTERFACE && aIID.Equals(NS_GET_IID(nsITextWidget))) {
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
  if (NULL != mWnd) {
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = (WNDPROC)::SetWindowLong(mWnd, GWL_WNDPROC, 
                                                 (LONG)nsTextWidget::TextWindowProc);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        SetNSWindowPtr(mWnd, this);
    } 
    else {
        (void) ::SetWindowLong(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
        SetNSWindowPtr(mWnd, NULL);
        mPrevWndProc = NULL;
    }
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
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
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
    aRenderingContext.DrawString(mText, x, y + textHeight);
  } else {
    nsString astricks;
    PRUint32 i;
    for (i=0;i<mText.Length();i++) {
      astricks.AppendWithConversion("*");
    }
    aRenderingContext.DrawString(astricks, x, y + textHeight);

  }

  NS_RELEASE(context);

  return NS_OK;
}


