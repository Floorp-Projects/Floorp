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

#include "nspr.h"
#include "nsString.h"
#include "nsRepository.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsIImageGroup.h"
#include "nsIImage.h"
#include "nsTreeView.h"
#include "nsHTTreeDataModel.h"

static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);

NS_IMPL_ADDREF(nsTreeView)
NS_IMPL_RELEASE(nsTreeView)

static nsEventStatus PR_CALLBACK
HandleTreeEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsIContentConnector * tree;
  if (NS_OK == aEvent->widget->QueryInterface(kIContentConnectorIID,(void**)&tree)) {
    result = tree->HandleEvent(aEvent);
    NS_RELEASE(tree);
  }
  return result;
}

NS_METHOD nsTreeView::Create(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData)
{
  nsresult answer = ChildWindow::Create(aParent, aRect,
     nsnull != aHandleEventFunction ? aHandleEventFunction : HandleTreeEvent,
     aContext, aAppShell, aToolkit, aInitData);

  if (mDataModel)
	mDataModel->SetDataModelListener(this);

  return answer;
}

nsTreeView::nsTreeView() : nsIContentConnector(), nsDataModelWidget()
{
  NS_INIT_REFCNT();
  mDataModel = nsnull;
  mMouseDown = PR_FALSE;
  mMouseDragging = PR_FALSE;
  mDraggingColumnHeader = PR_FALSE;
  mDraggingColumnEdge = PR_FALSE;
  mLastXPosition = 0;
  mMousedColumnIndex = 0;

  // The data model is created and bound to the widget.
  mDataModel = new nsHTTreeDataModel(); 
}

//--------------------------------------------------------------------
nsTreeView::~nsTreeView()
{
	// The tree has the responsibility of deleting the data model.
	delete mDataModel;
}

// ISupports Implementation --------------------------------------------------------------------
nsresult nsTreeView::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(kIContentConnectorIID)) {                                          
    *aInstancePtr = (void*) (nsIContentConnector *)this;                                        
    AddRef();                                                           
    return NS_OK;                                                        
  }     
  return (nsWindow::QueryInterface(aIID, aInstancePtr));
}

NS_METHOD nsTreeView::SetContentRoot(nsIContent* pContent)
{
	if (mDataModel)
	{
		mDataModel->SetContentRoot(pContent);
	}

	return NS_OK;
}

void nsTreeView::HandleDataModelEvent(int anEvent, nsHierarchicalDataItem* pItem)
{
	Invalidate(PR_FALSE);
}

//-----------------------------------------------------
nsEventStatus nsTreeView::HandleEvent(nsGUIEvent *aEvent) 
{
  if (aEvent->message == NS_SIZE)
  {
	  nsRect r;
	  aEvent->widget->GetBounds(r);
	  
	  // Adjust our column headers' positions.
	  ResizeColumns(r.width);

	  // Do a repaint.
	  if (mTitleBarRect.width != r.width)
		aEvent->widget->Invalidate(PR_FALSE);
  }
  else if (aEvent->message == NS_PAINT) 
  {
    nsRect r;
    aEvent->widget->GetBounds(r);
    r.x = 0;
    r.y = 0;
	
	nsRect rect(r);

	nsDrawingSurface ds;
    nsIRenderingContext * ctx = ((nsPaintEvent*)aEvent)->renderingContext;

    ctx->CreateDrawingSurface(&r, 0, ds);
    ctx->SelectOffScreenDrawingSurface(ds);

	if (mDataModel)
	{
		// Paint the title bar.
		PaintTitleBar(ctx, r);

		// Paint the control strip.
		PaintControlStrip(ctx, r);

		// Paint the column bar.
		PaintColumnBar(ctx, r);

		// Paint the tree.
		PaintTreeRows(ctx, r);
	}
	ctx->CopyOffScreenBits(ds, 0, 0, rect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
    ctx->DestroyDrawingSurface(ds);
  }
  else if (aEvent->message == NS_MOUSE_EXIT)
  {
	  aEvent->widget->Invalidate(PR_FALSE); // Makes sure we ditch rollover feedback on exit.
  }
  else if (aEvent->message == NS_MOUSE_MOVE)
  {
	  HandleMouseMove(aEvent); 
  }
  else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
  {
	  mCachedDownPoint.x = aEvent->point.x;
	  mCachedDownPoint.y = aEvent->point.y;
	  
	  mMouseDown = PR_TRUE; // We could potentially initiate a drag. Don't kick it off until
							// we know for sure.
  }
  else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
  {
	  if (mMouseDown)
	  {
		  mCachedMovePoint.x = aEvent->point.x;
		  mCachedMovePoint.y = aEvent->point.y;

		  mMouseDown = PR_FALSE; 
		  mMouseDragging = PR_FALSE;

		  if (mDraggingColumnEdge)
		  {
			  // Finish up.
			  DragColumnEdge(mCachedMovePoint.x);

			  RecomputeColumnPercentages();

			  mDraggingColumnEdge = PR_FALSE;
		  }
		  
		  if (mColumnBarRect.Contains(mCachedMovePoint.x, mCachedMovePoint.y))
		  {
			  int currentPosition = 0;
			  int remainingSpace = mColumnBarRect.width;
			  int totalSpace = mColumnBarRect.width;
			  PRUint32 count = mDataModel->GetVisibleColumnCount();

			  // The user boinked a column or a pusher.  Figure out which.
			  for (PRUint32 n = 0; n < count; n++)
			  {
				  // Fetch each column.
				  nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
				  int pixelWidth = pColumn->GetPixelWidth();
				  remainingSpace -= pixelWidth;
				  currentPosition += pixelWidth;

				  // TODO: See if we hit this column header
				  if (mCachedMovePoint.x < currentPosition)
				  {
					  // We hit this column header.
					  return nsEventStatus_eIgnore;
				  }
			  }
			  
			  // Must have hit a pusher
			  if (mCachedMovePoint.x < (currentPosition + (remainingSpace / 2)))
				  ShowColumn(); // Hit the left pusher
			  else HideColumn(); // Hit the right pusher
		  }
	  }
  }
  return nsEventStatus_eIgnore;
}

void nsTreeView::HandleMouseMove(nsGUIEvent* aEvent)
{
  // Remember our point for easy reference.
  mCachedMovePoint.x = aEvent->point.x;
  mCachedMovePoint.y = aEvent->point.y;
	  
  if (mMouseDown)
  {
	  // The user has the left mouse button down.
	  if (!mMouseDragging)
	  {
		  // We have the left mouse button down and we're moving. Check to see if we should
		  // initiate a drag and drop operation.
		  if (mCachedMovePoint.x < mCachedDownPoint.x - 3 ||
			  mCachedMovePoint.x > mCachedDownPoint.x + 3 ||
			  mCachedMovePoint.y < mCachedDownPoint.y - 3 ||
			  mCachedMovePoint.y > mCachedDownPoint.y + 3)
		  {
			  // We're dragging baby.
			  mMouseDragging = PR_TRUE;

			  // Question is, just what are we dragging?
			  if (mColumnBarRect.Contains(mCachedDownPoint.x, mCachedDownPoint.y))
			  {
				  // The user is messing with the columns.  Either a column header
				  // is being resized or a column is being dragged.
				  if (DownOnColumnEdge(mCachedDownPoint))
				  {
					  mDraggingColumnEdge = PR_TRUE;
					  mLastXPosition = mCachedDownPoint.x;
					  DragColumnEdge(mCachedMovePoint.x);
				  }
			  }
			  else if (mTreeRect.Contains(mCachedDownPoint.x, mCachedDownPoint.y))
			  {
				  // The user is dragging a tree item
			  }
		  }
	  }
	  else
	  {
		  // The user is dragging.
		  if (mDraggingColumnEdge)
		  {
			  // Keep on dragging.
			  DragColumnEdge(mCachedMovePoint.x);
		  }	
		  // Will add code for dragging column headers and tree items here.
	  }
  }
  else
  {	  
	  // The user doesn't have a mouse down. Just rolling over items.
	  aEvent->widget->Invalidate(PR_FALSE);
	  
	  if (mColumnBarRect.Contains(mCachedMovePoint.x, mCachedMovePoint.y) &&
		  DownOnColumnEdge(mCachedMovePoint))
	  {
		  // Change the cursor to a WE resize if on a column edge.
		  aEvent->widget->SetCursor(eCursor_sizeWE);
	  }
	  else aEvent->widget->SetCursor(eCursor_standard);
  }
}

void nsTreeView::PaintTitleBar(nsIRenderingContext* drawCtx, 
							   nsRect& rect)
{
	mTitleBarRect.SetRect(0,0,0,0);
	if (mDataModel) // There is no title bar shown unless we have a valid data model to query.
	{
		nsRect titleBarRect(rect);
		int titleBarHeight = 0; 
		int fontHeight = 0;
				
		if (mDataModel->ShowTitleBar()) 
		{
			titleBarHeight = 30; // Assume an initial height of 30. Change if font doesn't fit.
			nsIDeviceContext* dc = GetDeviceContext();
			float t2d;
			dc->GetTwipsToDevUnits(t2d);
			
			// Need to figure out our style info for the title bar.
			nsFont titleBarFont("MS Sans Serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
								400, NS_FONT_DECORATION_NONE,
								nscoord(t2d * NSIntPointsToTwips(10)));
			
			nsBasicStyleInfo styleInfo(titleBarFont);

			mDataModel->GetTitleBarStyle(dc, styleInfo);

			NS_RELEASE(dc);
			drawCtx->SetFont(styleInfo.font);
			nsIFontMetrics* fontMetrics;
			drawCtx->GetFontMetrics(fontMetrics);
			fontMetrics->GetHeight(fontHeight);
			titleBarHeight = fontHeight + 4; // Pad just a little.
			NS_RELEASE(fontMetrics);
			
			// Modify the rect we'll be returning for future painting.
			titleBarRect.height = titleBarHeight;
			rect.y = titleBarRect.height;
			rect.height -= titleBarRect.height;
			mTitleBarRect = titleBarRect;

			// First we lay down the background, and then we draw the text.
			drawCtx->SetColor(styleInfo.backgroundColor);
			drawCtx->FillRect(titleBarRect);

			// If there is a background image, draw it.
			if (styleInfo.pBackgroundImage)
				PaintBackgroundImage(drawCtx, styleInfo.pBackgroundImage, mTitleBarRect);

			if (mDataModel->ShowTitleBarText()) // Only show the text if we should.
			{
				drawCtx->SetColor(styleInfo.foregroundColor);
				nsString titleBarText;
				mDataModel->GetTitleBarText(titleBarText);
				DrawCroppedString(drawCtx, titleBarText, mTitleBarRect);
			}
		}
	}
}

void nsTreeView::PaintControlStrip(nsIRenderingContext* drawCtx, 
								   nsRect& rect)
{
	// Paint the add box, the edit box, and the close box.
	mControlStripRect.SetRect(0,0,0,0);
	if (mDataModel) // There is no title bar shown unless we have a valid data source to query.
	{
		nsRect controlStripRect(rect);
		int controlStripHeight = 0; // Assume an initial height of 0.
		int fontHeight = 0;
		nsIDeviceContext* dc = GetDeviceContext();
			
		if (mDataModel->ShowControlStrip())
		{
			// Need to figure out which font we're using for the control strip
			float t2d;
			dc->GetTwipsToDevUnits(t2d);
			nsFont controlStripFont("MS Sans Serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
								400, NS_FONT_DECORATION_NONE,
								nscoord(t2d * NSIntPointsToTwips(10)));
			
			nsBasicStyleInfo styleInfo(controlStripFont);
			mDataModel->GetControlStripStyle(dc, styleInfo);

			NS_RELEASE(dc);
			
			drawCtx->SetFont(styleInfo.font);
			nsIFontMetrics* fontMetrics;
			drawCtx->GetFontMetrics(fontMetrics);
			fontMetrics->GetHeight(fontHeight);
			controlStripHeight = fontHeight + 4; // Pad just a little.
			NS_RELEASE(fontMetrics);
			
			// Modify the rect we'll be returning for future painting.
			controlStripRect.height = controlStripHeight;
			rect.y += controlStripRect.height;
			rect.height -= controlStripRect.height;
			mControlStripRect = controlStripRect;

			// We now know the size of the control strip.  Retrieve the color information.
			
			// First we lay down the background, and then we draw the text.
			drawCtx->SetColor(styleInfo.backgroundColor);
			drawCtx->FillRect(controlStripRect);

			// If there is a background image, draw it.
			if (styleInfo.pBackgroundImage)
				PaintBackgroundImage(drawCtx, styleInfo.pBackgroundImage, mControlStripRect);

			drawCtx->SetColor(styleInfo.foregroundColor);

			// Iterate over the control strip items.
			PRUint32 itemCount = mDataModel->GetControlStripItemCount();
			int xOffset = 2;
			for (PRUint32 i = 0; i < itemCount; i++)
			{
				nsTreeControlStripItem* pItem = mDataModel->GetNthControlStripItem(i);
				nsString itemText;
				pItem->GetText(itemText);

				drawCtx->DrawString(itemText, xOffset, controlStripRect.y + 2, 1); // Indent slightly

				// Offset by the width of the text + 10.
				int strWidth = 0;
				drawCtx->GetWidth(itemText, strWidth);

				nsRect itemRect(xOffset-2, controlStripRect.y, 
							   strWidth + 4, controlStripRect.height);
				
				xOffset += strWidth + 10;

				if (itemRect.Contains(mCachedMovePoint.x, mCachedMovePoint.y))
					drawCtx->DrawRect(itemRect);
			}

			// Draw the close text at the rightmost side of the tree.
			nsString nsCloseText;
			mDataModel->GetControlStripCloseText(nsCloseText);
			int strWidth = 0;
			drawCtx->GetWidth(nsCloseText, strWidth);
			drawCtx->DrawString(nsCloseText, controlStripRect.width - strWidth - 2, 
								controlStripRect.y + 2, 1);
			nsRect closeRect = nsRect(controlStripRect.width - strWidth - 4, 
								controlStripRect.y, 
								strWidth + 4, 
								controlStripRect.height);
			
			if (closeRect.Contains(mCachedMovePoint.x, mCachedMovePoint.y))
				drawCtx->DrawRect(closeRect);
		}
	}
}

void nsTreeView::PaintColumnBar(nsIRenderingContext* drawCtx, 
								nsRect& rect)
{
	// The painting of the column headers along with the pushers.
	if (mDataModel)
	{
		// The very first step is to find out exactly how tall the column
		// bar needs to be given the font being used.
		
		PRBool needToResizeColumns = FALSE;
		if (mDataModel->ShowColumnHeaders() && mColumnBarRect == nsRect(0,0,0,0))
		{
			// Column sizes haven't been properly computed yet. Let's make
			// sure we're sane.
			needToResizeColumns = PR_TRUE;
		}

		mColumnBarRect.SetRect(0,0,0,0);
		
		nsRect columnBarRect(rect);
		int columnBarHeight = 0; // Assume an initial height of 0.
		int fontHeight = 0;

		if (mDataModel->ShowColumnHeaders()) // Default assumption is that we show it.
		{
			// Need to figure out our style info.
			nsIDeviceContext* dc = GetDeviceContext();
			float t2d;
			dc->GetTwipsToDevUnits(t2d);
			nsFont columnHeaderFont("MS Sans Serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
								400, NS_FONT_DECORATION_NONE,
								nscoord(t2d * NSIntPointsToTwips(10)));
			
			nsColumnHeaderStyleInfo styleInfo(columnHeaderFont);
			mDataModel->GetColumnHeaderStyle(dc, styleInfo);

			NS_RELEASE(dc);
			drawCtx->SetFont(styleInfo.font);
			nsIFontMetrics* fontMetrics;
			drawCtx->GetFontMetrics(fontMetrics);
			fontMetrics->GetHeight(fontHeight);
			columnBarHeight = fontHeight + 4; // Pad just a little.
			NS_RELEASE(fontMetrics);
			
			// Modify the rect we'll be returning for future painting.
			columnBarRect.height = columnBarHeight;
			rect.y += columnBarRect.height;
			rect.height -= columnBarRect.height;
			mColumnBarRect = columnBarRect;

			// We now know the size of the column bar  
			// If our size info was screwy before, let's sync it up now.
			// Don't move this. Code ordering is critical here.
			if (needToResizeColumns)
				ResizeColumns(columnBarRect.width);
			
			// First we lay down the background color and image if there is one
			drawCtx->SetColor(styleInfo.backgroundColor);
			drawCtx->FillRect(mColumnBarRect);
			// If there is a background image, draw it.
			if (styleInfo.pBackgroundImage)
				PaintBackgroundImage(drawCtx, styleInfo.pBackgroundImage,
									 mColumnBarRect);

			// Draw the columns.
			PRUint32 count = mDataModel->GetVisibleColumnCount();
			PRUint32 currentPosition = 0;
			for (PRUint32 n = 0; n < count; n++)
			{
				// Fetch each column.
				nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
				if (pColumn)
				{
					PaintColumnHeader(drawCtx, pColumn, currentPosition, 
									  styleInfo);
				}
			}

			// Draw the column pushers 
			int pusherWidth = (int)(mColumnBarRect.height * 1.25);
			if (pusherWidth%2 != 0)
				pusherWidth++;
			int singlePusher = pusherWidth / 2;

			drawCtx->SetColor(styleInfo.foregroundColor);
			drawCtx->DrawRect(currentPosition-1, mColumnBarRect.y, 
							  singlePusher+1, mColumnBarRect.height);

			drawCtx->DrawRect(currentPosition+singlePusher-1, 
							  mColumnBarRect.y, singlePusher+1,
							  mColumnBarRect.height);

			// Are our pushers enabled or disabled?
			PRUint32 visColumns = mDataModel->GetVisibleColumnCount();
			PRUint32 totalColumns = mDataModel->GetColumnCount();

			// Left pusher
			PRBool enabled = PR_TRUE;
			if (visColumns == totalColumns)
				enabled = PR_FALSE;
			if (enabled)
				drawCtx->SetColor(styleInfo.foregroundColor);
			else drawCtx->SetColor(styleInfo.disabledColor);
			PaintPusherArrow(drawCtx, PR_TRUE, currentPosition, singlePusher);

			// Right pusher
			enabled = PR_TRUE;
			if (visColumns<=1)
				enabled = PR_FALSE;
			if (enabled)
				drawCtx->SetColor(styleInfo.foregroundColor);
			else drawCtx->SetColor(styleInfo.disabledColor);
			PaintPusherArrow(drawCtx, PR_FALSE, currentPosition+singlePusher, singlePusher);
		}
	}
}

void nsTreeView::PaintColumnHeader(nsIRenderingContext* drawCtx, 
								   nsTreeColumn* pColumn,
								   PRUint32& currentPosition, 
								   const nsColumnHeaderStyleInfo& styleInfo)
{
	// If we're sorted, then fill with a sort column header color.
	PRBool isSortColumn = pColumn->IsSortColumn();
	if (isSortColumn)
		drawCtx->SetColor(styleInfo.sortBGColor);
	else drawCtx->SetColor(styleInfo.backgroundColor);

	// Compute this column's rectangle.
	int pixelWidth = pColumn->GetPixelWidth();
	nsRect colRect(currentPosition, mColumnBarRect.y, pixelWidth, mColumnBarRect.height);

	// Set to foreground color to draw the framing rect and text.
	if (isSortColumn)
		drawCtx->SetColor(styleInfo.sortFGColor);
	else drawCtx->SetColor(styleInfo.foregroundColor);

	// Draw the frame rect.
	colRect.x--; // The next two lines assure that there's no ugly left border on the first column
				 // and that the borderlines of each column header overlap.
	colRect.width++;
	drawCtx->DrawRect(colRect);

	// Now draw the text.
	nsString columnName("Name");
	pColumn->GetColumnDisplayText(columnName);

	colRect.x++;
	colRect.width--;
	DrawCroppedString(drawCtx, columnName, colRect);
	
	if (isSortColumn)
	{
		// TODO: Now draw the sort indicator (up or down arrow) as needed.
	}
	
	// Update the current position for the next column.
	currentPosition += pixelWidth;
}

void nsTreeView::PaintPusherArrow(nsIRenderingContext* drawCtx,
								  PRBool isLeftArrow, int left, int width)
{
	int horStart;
	int horEnd;
	int change;
	if (isLeftArrow)
	{
		horStart = left+3;
		horEnd = left+width-5;
		change = 1;
	}
	else
	{
		horStart = left+width-5;
		horEnd = left+3;
		change = -1;
	}

	int lineHeight = 0;
	for (int i = horStart; i != horEnd; i += change, lineHeight += 2)
	{
		int vertStart = mColumnBarRect.y + (mColumnBarRect.height - lineHeight)/2;
		int vertEnd = vertStart + lineHeight;

		if (vertStart < mColumnBarRect.y + 2)
			break;
		
		drawCtx->DrawLine(i, vertStart, i, vertEnd);
	}
}
					
void nsTreeView::PaintTreeRows(nsIRenderingContext* drawCtx, 
								nsRect& rect)
{
	// The fun part. Painting of the individual lines of the tree.

	// Start at the top line, getting the hint for what the top line is
	// from the data model..  Let each line paint itself and update its
	// position so that the next line knows where to begin.  Stop when you run out of lines
	// or when you're dealing with lines that are no longer visible.
	if (!mDataModel)
		return;
			
	int yPosition = rect.y;
	PRUint32 n = mDataModel->GetFirstVisibleItemIndex();
	nsTreeItem* pItem = (nsTreeItem*)mDataModel->GetNthItem(n);

	while (pItem && yPosition < rect.y + rect.height)
	{
		PaintTreeRow(drawCtx, pItem, yPosition); 
		n++;
		pItem = (nsTreeItem*)mDataModel->GetNthItem(n);
	}

	if (yPosition < rect.y + rect.height)
	{
		// Fill the remaining area. TODO: Account for sort highlighting
		nsRect remainderRect(0, yPosition, rect.width, rect.height);
		nscolor viewBGColor = NS_RGB(240,240,240);
		drawCtx->SetColor(viewBGColor);
		drawCtx->FillRect(remainderRect);
	}
}

void nsTreeView::PaintTreeRow(nsIRenderingContext* drawCtx, nsTreeItem* pItem, int& yPosition)
{
	// Determine the height of this tree line.  It is going to be the max of
	// three objects: the trigger image, the icon, and the font height.
	// Will take whichever of these three is the largest, and we will add on 5 pixels.
	// 4 pixels of padding, and 1 pixel for the horizontal divider line.
	// Need to figure out which font we're using for the title bar.
	nsIDeviceContext* dc = GetDeviceContext();
	float t2d;
	dc->GetTwipsToDevUnits(t2d);
	nsFont treeItemFont("MS Sans Serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
								400, NS_FONT_DECORATION_NONE,
								nscoord(t2d * NSIntPointsToTwips(10)));
	nsTreeItemStyleInfo styleInfo(treeItemFont);
	
	pItem->GetItemStyle(dc, styleInfo);

	int lineHeight = 19;
	int fontHeight = 0;

	NS_RELEASE(dc);
	drawCtx->SetFont(styleInfo.font);
	nsIFontMetrics* fontMetrics;
	drawCtx->GetFontMetrics(fontMetrics);
	fontMetrics->GetHeight(fontHeight);

	int fontWithPadding = fontHeight + 4;

	int triggerHeight = 0;
	int iconHeight = 0;
	nsIImage* pTriggerImage = styleInfo.pTriggerImage;
	nsIImage* pIconImage = styleInfo.pIconImage;
	PRBool showTrigger = styleInfo.showTrigger;
	PRBool leftJustifyTrigger = styleInfo.leftJustifyTrigger;
	PRBool showIcon = styleInfo.showIcon;

	if (pTriggerImage)
		triggerHeight = pTriggerImage->GetHeight();
	if (pIconImage)
		iconHeight = pIconImage->GetHeight();
	
	lineHeight = fontWithPadding + 1; 
	if (triggerHeight + 5 > lineHeight)
		lineHeight = triggerHeight + 5; // Ensure 2 pixels of padding on either side + 1 for div.
	if (iconHeight + 5 > lineHeight)
		lineHeight = iconHeight + 5;  // Ensure pixel padding plus 1 for div.

	NS_RELEASE(fontMetrics);
	
	// Modify the position we'll be returning for future painting.
	nsRect lineRect(0, yPosition, mColumnBarRect.width, lineHeight);
	yPosition += lineHeight;

	// Fill the rect with our BGColor. TODO: Fetch properties etc.
	drawCtx->SetColor(styleInfo.backgroundColor);
	drawCtx->FillRect(lineRect);
	if (styleInfo.pBackgroundImage)
	{
		PaintBackgroundImage(drawCtx, styleInfo.pBackgroundImage, lineRect);
	}
	
	// Draw the horizontal divider
	if (styleInfo.showHorizontalDivider)
	{
		drawCtx->SetColor(styleInfo.horizontalDividerColor);
		drawCtx->DrawLine(0, yPosition-1, mColumnBarRect.width, yPosition-1);
	}

	// Iterate over the visible columns and paint the data specified.
	PRUint32 count = mDataModel->GetVisibleColumnCount();
	int currentPosition = 0;
	for (PRUint32 n = 0; n < count; n++)
	{
		// Fetch each column.
		nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
		if (pColumn)
		{
			// Retrieve the column's current pixel width.
			int pixelWidth = pColumn->GetPixelWidth();

			// Draw our text indented slightly, centered in our line rect, and cropped.
			nsString nodeText("Column Data");
			pItem->GetTextForColumn(pColumn, nodeText);

			int textStart = currentPosition + 2;
			if (n == 0)
			{
				// This is the image column.
				// Draw the trigger image at the appropriate spot.
				int indentation = pItem->GetIndentationLevel();
				int pixelIndent = cIndentAmount * indentation;
				int iconMargin = cIconMargin; // Distance from trigger edge to icon edge
				int triggerStart = 4;

				int triggerWidth = (pTriggerImage && showTrigger ? pTriggerImage->GetWidth() : 0);
				int iconStart = triggerStart + triggerWidth + 
								cIconMargin + pixelIndent;
				if (!leftJustifyTrigger)
				{	
					triggerStart = 4 + pixelIndent;
					iconStart = triggerStart + triggerWidth + cIconMargin;
				}

				if (pTriggerImage && showTrigger)
					drawCtx->DrawImage(pTriggerImage, triggerStart,
									   lineRect.y + (lineHeight-1-triggerHeight)/2);
				if (pIconImage)
					drawCtx->DrawImage(pIconImage, iconStart,
									   lineRect.y + (lineHeight-1-iconHeight)/2);

				textStart = iconStart + (showIcon && pIconImage ? pIconImage->GetWidth() : 0) + 2;
			}

			// Determine the rect to use. 
			int start = lineRect.y + (lineHeight-fontWithPadding-1)/2; 
			nsRect textRect(textStart, start, pixelWidth-2-(textStart-currentPosition), fontWithPadding);
			nsRect resultRect = DrawCroppedString(drawCtx, nodeText, textRect, PR_FALSE);
			
			// Modify the result rect so it is the appropriate size for the drawing of rollover and
			// selection.
			resultRect.y = lineRect.y; 
			resultRect.height = lineRect.height;
			
			// Now we really draw the string, using the appropriate colors/styles.
			// Options are ROLLOVER, SELECTION, or NORMAL.  
			if (resultRect.Contains(mCachedMovePoint.x, mCachedMovePoint.y))
			{
				// Rollover style should be applied.
				drawCtx->SetColor(styleInfo.rolloverFGColor);
				
				// TODO: Handle Rollover BACKGROUND color. Could be applied to text, to cell, or to line.
			}
			else drawCtx->SetColor(styleInfo.foregroundColor);

			// Now draw the column data for real.
			DrawCroppedString(drawCtx, nodeText, textRect);

			currentPosition += pixelWidth;

			// Draw the vertical divider
			if (styleInfo.showVerticalDivider)
			{
				drawCtx->SetColor(styleInfo.verticalDividerColor);
				drawCtx->DrawLine(textStart+textRect.width + 1, lineRect.y, 
								  textStart+textRect.width + 1, lineRect.y + lineRect.height);
			}
		}
	}
}

void nsTreeView::PaintBackgroundImage(nsIRenderingContext* ctx,
									  nsIImage* bgImage, const nsRect& constraintRect,
								      int xSrcOffset, int ySrcOffset)
{
	// This code gets a bit intense. Will comment heavily.

	int imageWidth = bgImage->GetWidth();	// The dimensions of the background image being tiled.
	int imageHeight = bgImage->GetHeight();

	int totalWidth = constraintRect.width;   // The dimensions of the space we're
	int totalHeight = constraintRect.height; // drawing into.

	if (imageWidth <= 0 || imageHeight <= 0) // Don't draw anything if we don't have a sane image.
		return;

	int xSize = imageWidth - xSrcOffset;	// The dimensions of the actual tile we'll end
	int ySize = imageHeight - ySrcOffset;	// up drawing.  A subset of the full BG image.
	
	xSize = (xSize > totalWidth) ? totalWidth : xSize;		
	ySize = (ySize > totalHeight) ? totalHeight : ySize;

	int rightMostPoint = constraintRect.x + constraintRect.width;	// Edges of the space we're
	int bottomMostPoint = constraintRect.y + constraintRect.height; // drawing into.

	int xDstOffset = constraintRect.x;	// Top-left coordinates in the space where
	int yDstOffset = constraintRect.y;  // we'll be drawing.  Where we'll place the tile.

	int initXOffset = xSrcOffset;
	
	// Tile vertically until we move out of the constraining rect.
	while (yDstOffset < bottomMostPoint)
	{
		// Tile horizontally until we move out of the constraining rect.
		while (xDstOffset < rightMostPoint)
		{
			// Draw the subimage.  Pull the subimage from the larger image
			// and then draw it.
			ctx->DrawImage(bgImage, nsRect(xSrcOffset, ySrcOffset, xSize, ySize),
						   nsRect(xDstOffset, yDstOffset, xSize, ySize));

			// The next subimage will be as much of the full BG image as can fit in the
			// constraining rect.  If we're at the edge, we don't draw quite as much.
			xSrcOffset = 0;
			xDstOffset += xSize;
			xSize = (xDstOffset + imageWidth) > rightMostPoint ? imageWidth - (xDstOffset + imageWidth) + rightMostPoint : imageWidth;
		}

		xSrcOffset = initXOffset;	// Start of all rows will be at the same initial x offset.
		xDstOffset = constraintRect.x; // Reset our x-position for drawing the next row.
		xSize = (xDstOffset + imageWidth) > rightMostPoint ? rightMostPoint - xDstOffset : imageWidth;

		// Determine the height of the next row.  Will be as much of the BG image
		// as can fit in the constraining rect. If we're at the bottom edge, we don't
		// draw quite as much.
		ySrcOffset = 0;
		yDstOffset += ySize;
		ySize = (yDstOffset + imageHeight) > bottomMostPoint ? bottomMostPoint - yDstOffset : imageHeight;
	}
}

void nsTreeView::ShowColumn()
{
	int pusherWidth = (int)(mColumnBarRect.height * 1.25);
	if (pusherWidth % 2 != 0)
		pusherWidth++;

	int totalSpace = mColumnBarRect.width - pusherWidth;
	int remainingSpace = totalSpace;
	PRUint32 visColumns = mDataModel->GetVisibleColumnCount();
	PRUint32 totalColumns = mDataModel->GetColumnCount();

	if (visColumns < totalColumns)
	{
		visColumns++;
		mDataModel->SetVisibleColumnCount(visColumns);

		nsTreeColumn* pShownColumn = mDataModel->GetNthColumn(visColumns-1);
		double totalPercentage = 1.0 + pShownColumn->GetDesiredPercentage();

		// Do a recomputation of column widths and percentages
        for (PRUint32 i = 0; i < visColumns; i++)
        {
			nsTreeColumn* pColumn = mDataModel->GetNthColumn(i);
			pColumn->SetDesiredPercentage(pColumn->GetDesiredPercentage() /
											  totalPercentage);
			
			pColumn->SetPixelWidth((int)(pColumn->GetDesiredPercentage() * totalSpace));
			remainingSpace -= pColumn->GetPixelWidth();
			if (i == visColumns-1)
				pColumn->SetPixelWidth(pColumn->GetPixelWidth() + remainingSpace);
		}

		Invalidate(PR_FALSE);
	}
}


void nsTreeView::HideColumn()
{
	int pusherWidth = (int)(mColumnBarRect.height * 1.25);
	if (pusherWidth % 2 != 0)
		pusherWidth++;

	int totalSpace = mColumnBarRect.width - pusherWidth;
	int remainingSpace = totalSpace;
	PRUint32 visColumns = mDataModel->GetVisibleColumnCount();
	PRUint32 totalColumns = mDataModel->GetColumnCount();

	if (visColumns > 1)
	{
		visColumns--;
		mDataModel->SetVisibleColumnCount(visColumns);

		// Do a recomputation of column widths and percentages
		nsTreeColumn* pHiddenColumn = mDataModel->GetNthColumn(visColumns);

		// The hidden column's desired percentage should be changed so that
		// if it is subsequently reshown, it will be the same size.
		double totalPercentage = 1.0 - pHiddenColumn->GetDesiredPercentage();
		pHiddenColumn->SetDesiredPercentage(pHiddenColumn->GetDesiredPercentage() / totalPercentage);

		for (PRUint32 i = 0; i < visColumns; i++)
		{
			nsTreeColumn* pColumn = mDataModel->GetNthColumn(i);
			pColumn->SetDesiredPercentage(pColumn->GetDesiredPercentage() / totalPercentage);
			pColumn->SetPixelWidth((int)(pColumn->GetDesiredPercentage() * totalSpace));
			remainingSpace -= pColumn->GetPixelWidth();
			if (i == visColumns-1)
				pColumn->SetPixelWidth(pColumn->GetPixelWidth() + remainingSpace);
			
		}

		Invalidate(PR_FALSE);
	}
}

void nsTreeView::RecomputeColumnPercentages()
{
	int width = mColumnBarRect.width;
	PRUint32 count = mDataModel->GetVisibleColumnCount();
	int currentPosition = 0;
	int pusherWidth = (int)(mColumnBarRect.height * 1.25);
	if (pusherWidth % 2 != 0)
		pusherWidth++;

	int totalSpace = width - pusherWidth;
	
	for (PRUint32 n = 0; n < count; n++)
	{
		// Fetch each column.
		nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
		if (pColumn)
		{
			int pixelWidth = pColumn->GetPixelWidth();
			pColumn->SetDesiredPercentage(((double)pixelWidth)/((double)totalSpace));
		}
	}
}

void nsTreeView::ResizeColumns(int width)
{
    // Need to do the appropriate resizing of the columns.
	PRUint32 count = mDataModel->GetVisibleColumnCount();
	int currentPosition = 0;
	int pusherWidth = (int)(mColumnBarRect.height * 1.25);
	if (pusherWidth % 2 != 0)
		pusherWidth++;

	int remainingSpace = width - pusherWidth;
	int totalSpace = remainingSpace;
	double newColPercentage = 1.0 / count;

	for (PRUint32 n = 0; n < count; n++)
	{
		// Fetch each column.
		nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
		if (pColumn)
		{
			double desiredPercentage = pColumn->GetDesiredPercentage();
			int newPixelWidth = (int)(desiredPercentage*totalSpace);
			pColumn->SetPixelWidth(newPixelWidth);
			remainingSpace -= newPixelWidth;
			if (n == count-1)
				pColumn->SetPixelWidth(newPixelWidth + remainingSpace);
		}
	}
}

PRBool nsTreeView::DownOnColumnEdge(const nsPoint& point)
{
	int x = point.x; // Only x coord is relevant.
	if (!mDataModel) return PR_FALSE;
	
	PRUint32 count = mDataModel->GetVisibleColumnCount();
	int currentPosition = 0;
	
	for (PRUint32 n = 0; n < count-1; n++)
	{
		// Fetch each column.
		nsTreeColumn* pColumn = mDataModel->GetNthColumn(n);
		if (pColumn)
		{
			int pixelWidth = pColumn->GetPixelWidth();
			if (currentPosition + pixelWidth - 2 <= x &&
				x <= currentPosition + pixelWidth + 2)
			{
				// Cache the column hit in case we end up dragging it around.
				mMousedColumnIndex = n;
				return PR_TRUE;
			}

			if (x < currentPosition + pixelWidth)
				return PR_FALSE;

			currentPosition += pixelWidth;
		}
	}
	return PR_FALSE;
}

void nsTreeView::DragColumnEdge(int xPos)
{
	nsTreeColumn* pLeftColumn = mDataModel->GetNthColumn(mMousedColumnIndex);
	nsTreeColumn* pRightColumn = mDataModel->GetNthColumn(mMousedColumnIndex+1);
	if (pLeftColumn && pRightColumn)
	{
		// Adjust the two columns' pixel widths.
		int leftPixelWidth = pLeftColumn->GetPixelWidth();
		int rightPixelWidth = pRightColumn->GetPixelWidth();
		int dragDiff = ((int)xPos) - ((int)mLastXPosition);
		int newLeft = leftPixelWidth+dragDiff;
		int newRight = rightPixelWidth-dragDiff;
		if (newLeft > cMinColumnWidth && newRight > cMinColumnWidth)
		{
			pLeftColumn->SetPixelWidth(newLeft);
			pRightColumn->SetPixelWidth(newRight);

			mLastXPosition = xPos;
			Invalidate(PR_FALSE);
		}
	}
}

// Helpers
void nsTreeView::ParseColor(char* colorString, nscolor& colorValue)
{
	if (!NS_ColorNameToRGB(colorString, &colorValue))
		NS_HexToRGB(colorString, &colorValue);
}

nsRect nsTreeView::DrawCroppedString(nsIRenderingContext* drawCtx, nsString text, 
							const nsRect& rect, PRBool drawText)
{
	int strWidth = 0;
	drawCtx->GetWidth(text, strWidth);
	nsString cropString(text);
	if (strWidth + 4 > rect.width)
	{
		int length = text.Length();
		if (length > 3)
		{
			cropString = "";
			text.Left(cropString, length - 3);
			cropString += "...";
		
			drawCtx->GetWidth(cropString, strWidth);
			while (strWidth + 4 > rect.width && length > 3)
			{
				length--;
				cropString = "";
				text.Left(cropString, length - 3);
				cropString += "...";
				drawCtx->GetWidth(cropString, strWidth);
			}
		}
	}

	if (drawText)
	{
		// Draw the cropString.
		drawCtx->DrawString(cropString, rect.x + 2, rect.y + 2, 1); 
	}

	int width = strWidth + 4;
	if (width > rect.width)
		width = rect.width;
	return nsRect(rect.x, rect.y, width, rect.height);
}

