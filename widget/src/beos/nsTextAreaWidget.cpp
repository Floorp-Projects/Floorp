/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nslog.h"

NS_IMPL_LOG(nsTextAreaWidgetLog, 0)
#define PRINTF NS_LOG_PRINTF(nsTextAreaWidgetLog)
#define FLUSH  NS_LOG_FLUSH(nsTextAreaWidgetLog)

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);



/*NS_IMPL_ADDREF(nsTextAreaWidget)
//NS_IMPL_RELEASE(nsTextAreaWidget)
nsrefcnt nsTextAreaWidget::Release(void)                         
{             
//  NS_PRECONDITION(0 != cnt, "dup release");        
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}*/

NS_IMPL_ADDREF(nsTextAreaWidget)
NS_IMPL_RELEASE(nsTextAreaWidget)

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget()
{
  NS_INIT_REFCNT();
  nsTextHelper::mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);


  if (aIID.Equals(kITextAreaWidgetIID)) {
      nsITextAreaWidget* textArea = this;
      *aInstancePtr = (void*) (textArea);
      NS_ADDREF_THIS();
      return NS_OK;
  } 
  else if (aIID.Equals(kIWidgetIID))
  {
      nsIWidget* widget = this;
      *aInstancePtr = (void*) (widget);
      NS_ADDREF_THIS();
      return NS_OK;
  }

  return nsWindow::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsTextAreaWidget::OnPaint(nsRect &r)
{
    return PR_FALSE;
}

PRBool nsTextAreaWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsTextAreaWidget::GetBounds(nsRect &aRect)
{
#if 0
  nsWindow::GetNonClientBounds(aRect);
#endif
  PRINTF("nsTextAreaWidget::GetBounds not wrong\n");	// the following is just a placeholder
  nsWindow::GetClientBounds(aRect);
  return NS_OK;
}


/**
 * Renders the TextWidget for Printing
 *
 **/
NS_METHOD nsTextAreaWidget::Paint(nsIRenderingContext& aRenderingContext,
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
  aRenderingContext.DrawString(mText, x, y);

  NS_RELEASE(context);

  return NS_OK;
}

BView *nsTextAreaWidget::CreateBeOSView()
{
	mTextView = new nsTextAreaBeOS(this, BRect(0, 0, 0, 0), B_EMPTY_STRING, BRect(0, 0, 0, 0), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
	return new TextFrameBeOS(mTextView, BRect(0, 0, 0, 0), B_EMPTY_STRING, 0, 0);
}

//----------------------------------------------------
// BeOS Sub-Class TextView
//----------------------------------------------------

nsTextAreaBeOS::nsTextAreaBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, BRect aTextRect, uint32 aResizingMode,
    uint32 aFlags )
  : BTextView( aFrame, aName, aTextRect, aResizingMode, aFlags ),
    nsIWidgetStore( aWidgetWindow )
{
}
