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

#ifndef nsTreeView_h___
#define nsTreeView_h___

#include "nsITreeView.h"
#include "nsTreeDataModel.h"
#include "nsTreeItem.h"
#include "nsTreeColumn.h"
#include "nsTreeControlStripItem.h"
#include "nsDataModelWidget.h"

class nsIImageGroup;
class nsIImage;


//------------------------------------------------------------
// A tree view consists of several components: a title bar, a control strip,
// a column bar, the actual tree contents, and an accompanying scrollbar for the
// tree contents.
//
// The actual tree view knows nothing about the data it is displaying.  It queries
// a data model for all properties, and then just uses the values it obtains
// to display the data.  It passes all expand/collapse/deletion/etc. events back
// to the data model.

class nsTreeView : public nsITreeView,
				   public nsDataModelWidget
{
public:
    nsTreeView();
    virtual ~nsTreeView();

	// nsISupports Interface --------------------------------
	NS_DECL_ISUPPORTS
	
	void HandleDataModelEvent(int event, nsHierarchicalDataItem* pItem);

	// Override the widget creation method
	NS_IMETHOD Create(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData);

protected:

	enum { cIndentAmount = 19, cIconMargin = 2, cMinColumnWidth = 30 };

	// These functions are all used in painting.
	void PaintTitleBar(nsIRenderingContext* drawCtx, 
					   nsRect& rect);
	void PaintControlStrip(nsIRenderingContext* drawCtx, 
						   nsRect& rect);
	
	// These functions are used in painting the column bar.
	void PaintColumnBar(nsIRenderingContext* drawCtx, 
					    nsRect& rect);
	void PaintColumnHeader(nsIRenderingContext* drawCtx, 
								   nsTreeColumn* pColumn,
								   PRUint32& currentPosition, 
								   const nsColumnHeaderStyleInfo& styleInfo);
	void PaintPusherArrow(nsIRenderingContext* drawCtx,
								  PRBool isLeftArrow, int left, int width);

	// These functions are used to paint the rows in the tree.
	void PaintTreeRows(nsIRenderingContext* drawCtx, nsRect& rect);
	void PaintTreeRow(nsIRenderingContext* drawCtx, nsTreeItem* pItem, int& yPosition);

	// General function for painting a background image.
	void PaintBackgroundImage(nsIRenderingContext* drawCtx, 
							  nsIImage* bgImage, const nsRect& constraintRect,
							  int xSrcOffset = 0, int ySrcOffset = 0);

	// Helper to parse color strings.
	void ParseColor(char* colorString, nscolor& colorValue);

	// Column Adjusters/Routines
	void ResizeColumns(int width);
	void RecomputeColumnPercentages();
	void ShowColumn();
	void HideColumn();
	void DragColumnEdge(int xPos);
	PRBool DownOnColumnEdge(const nsPoint& point);

	// String drawing helper. Crops left, inserts ellipses, and left justifies string.
	// Assumption is that the font has been set in the
	// rendering context, so the font metrics can be retrieved and used.
	void DrawCroppedString(nsIRenderingContext* drawCtx, nsString text, 
							const nsRect& rect);

	NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

protected:
	// Data members
	nsTreeDataModel* mDataModel; // The data source from which everything to draw is obtained.
	nsIImageGroup* mImageGroup; // Used to make requests for tree images.

	// Cached rects for fast hit testing computation.
	nsRect mTitleBarRect;
	nsRect mControlStripRect;
	nsRect mColumnBarRect;
	nsRect mTreeRect;

	// This rect is an area that needs to be invalidated once the mouse leaves it.  
	// Used on mouse move events.
	nsRect mCachedMoveRect; // Is either the control strip or lines in the tree.  Only
							// these items can possibly be invalidated on a mouse move.
	nsPoint mCachedMovePoint;	// Cache the last mouse move.
	nsPoint mCachedDownPoint;	// Cache the mouse down point.

	PRBool mMouseDown;			// Whether or not the mouse is pressed.
	PRBool mMouseDragging;		// Whether or not we're dragging something.
	
	// These members all have to do with dragging the edges of columns to resize them.
	int mMousedColumnIndex;	// The index of the column edge hit on a mouse down.
	int mLastXPosition; // The last X position of the dragged column edge.
	PRBool mDraggingColumnEdge; // Whether or not we're dragging a column edge.

	PRBool mDraggingColumnHeader;	// Whether or not we're dragging a column header

private:
	// tree views are widgets which exist in one-to-one correspondance with objects on
	// the screen. Copying them does not make sense, so we outlaw it explicitly.
	nsTreeView ( const nsTreeView & ) ;					// DON'T IMPLEMENT
	nsTreeView & operator= ( const nsTreeView & );		// DON'T IMPLEMENT
};

#endif /* nsTreeView_h___ */
