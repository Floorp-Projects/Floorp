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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

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

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsTextWidget::OnPaint(nsRect &r)
{
    return PR_FALSE;
}


PRBool nsTextWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsTextWidget::GetBounds(nsRect &aRect)
{
#if 0
  nsWindow::GetNonClientBounds(aRect);
#endif
printf("nsTextWidget::GetBounds not wrong\n");	// the following is just a placeholder
  nsWindow::GetClientBounds(aRect);
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
    aRenderingContext.DrawString(mText, x, y);
  } else {
    nsString astricks;
    PRInt32 i;
    for (i=0;i<mText.Length();i++) {
      astricks.AppendWithConversion("*");
    }
    aRenderingContext.DrawString(astricks, x, y);

  }

  NS_RELEASE(context);

  return NS_OK;
}

BView *nsTextWidget::CreateBeOSView()
{
	mTextView = new nsTextViewBeOS(this, BRect(0, 0, 0, 0), B_EMPTY_STRING, BRect(0, 0, 0, 0), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
	return new TextFrameBeOS(mTextView, BRect(0, 0, 0, 0), B_EMPTY_STRING, 0, 0);
}

//----------------------------------------------------
// BeOS Sub-Class TextView
//----------------------------------------------------

nsTextViewBeOS::nsTextViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, BRect aTextRect, uint32 aResizingMode,
    uint32 aFlags )
  : BTextView( aFrame, aName, aTextRect, aResizingMode, aFlags ),
    nsIWidgetStore( aWidgetWindow )
{
	DisallowChar(10);
	DisallowChar(13);
	SetMaxBytes(256);
	SetWordWrap(false);
}

void nsTextViewBeOS::KeyDown(const char *bytes, int32 numBytes)
{
	// call back if not enter/return
	if(numBytes != 1 || (bytes[0] != 10 && bytes[0] != 13))
		BTextView::KeyDown(bytes, numBytes);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[4];
		args[0] = NS_KEY_DOWN;
		args[1] = bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();

		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 4, args);
		t->CallMethodAsync(info);
	}
}

void nsTextViewBeOS::KeyUp(const char *bytes, int32 numBytes)
{
	// call back if not enter/return
	if(numBytes != 1 || (bytes[0] != 10 && bytes[0] != 13))
		BTextView::KeyUp(bytes, numBytes);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[4];
		args[0] = NS_KEY_UP;
		args[1] = (int32)bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();

		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 4, args);
		t->CallMethodAsync(info);
	}
}
