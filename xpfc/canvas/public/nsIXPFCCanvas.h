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

#ifndef nsIXPFCCanvas_h___
#define nsIXPFCCanvas_h___

#include "nsRect.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"
#include "nsIIterator.h"
#include "nsSize.h"
#include "nsILayout.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"

class nsIModel;
class nsIXPFCCommand;

// IID for the nsIXPFCCanvas interface
#define NS_IXPFC_CANVAS_IID   \
{ 0x6bc9da40, 0xe9e7, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

/**
 * XPFC Canvas interface. This is the core interface to all 
 * renderable components in the XPFC world
 */
class nsIXPFCCanvas : public nsISupports
{

public:

  /**
   * Initialize the XPFCCanvas
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD  Init() = 0;

  /**
   * Initialize the XPFCCanvas
   * @param aNativeParent a native parent for drawing into.
   * @param aBounds the bounds for thw canvas, relative to it's parent
   * @param aHandleEventFunction The event procedure for handling GUIEvents
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD  Init(nsNativeWidget aNativeParent, 
                   const nsRect& aBounds, 
                   EVENT_CALLBACK aHandleEventFunction) = 0;

  /**
   * Initialize the XPFCCanvas
   * @param aParent a nsIWidget pointer to the parent.
   * @param aBounds the bounds for thw canvas, relative to it's parent
   * @param aHandleEventFunction The event procedure for handling GUIEvents
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD  Init(nsIWidget * aParent, 
                   const nsRect& aBounds, 
                   EVENT_CALLBACK aHandleEventFunction) = 0;

  /**
   * Create an Iterator for this canvas's children
   * @param aIterator out paramater which implements the iterator interface.
   * @result The result of the iterator creation, NS_Ok if no errors
   */
  NS_IMETHOD  CreateIterator(nsIIterator ** aIterator) = 0;

  /**
   * Tells this canvas to layout itself and it's children
   * @result The result of the layout process, NS_Ok if no errors
   */
  NS_IMETHOD  Layout() = 0;

  /**
   * Get a pointer to the object implementing the nsILayout interface for this canvas
   * @result nsILayout pointer, nsnull if no layout object assigned, else a pointer to the layout object
   */
  NS_IMETHOD_(nsILayout *)  GetLayout() = 0;
  NS_IMETHOD                SetLayout(nsILayout * aLayout) = 0;

  /**
   * Get a pointer to the object implementing the nsIModel interface for this canvas
   * @result nsIModel pointer, nsnull if no model object assigned, else a pointer to the model object
   */
  NS_IMETHOD_(nsIModel *)   GetModel() = 0;
  NS_IMETHOD                SetModel(nsIModel * aModel) = 0;

  /**
   * Get the Name of this canvas, relative to itself
   * @result nsString reference, the name of the canvas
   */
  NS_IMETHOD_(nsString&) GetNameID() = 0;

  /**
   * Set the Name of this canvas, relative to itself
   * @result result of setting the name. NS_OK if successful
   */
  NS_IMETHOD  SetNameID(nsString& aString) = 0;

  /**
   * Get the Label of this canvas
   * @result nsString reference, the label of the canvas
   */
  NS_IMETHOD_(nsString&) GetLabel() = 0;

  /**
   * Set the Label of this canvas
   * @result result of setting the label. NS_OK if successful
   */
  NS_IMETHOD  SetLabel(nsString& aString) = 0;

  /**
   * Handle a GUI Event
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of gui event processing
   */
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnPaint Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnPaint event processing
   */
  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect) = 0;

  /**
   * Handle an OnResize Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnResize event processing
   */
  NS_IMETHOD_(nsEventStatus) OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Handle an OnMove Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMove event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMove(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnGotFocus Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnGotFocus event processing
   */
  NS_IMETHOD_(nsEventStatus) OnGotFocus(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnLostFocus Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnLostFocus event processing
   */
  NS_IMETHOD_(nsEventStatus) OnLostFocus(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnLeftButtonDown Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnLeftButtonDown event processing
   */
  NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnLeftButtonUp Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnLeftButtonUp event processing
   */
  NS_IMETHOD_(nsEventStatus) OnLeftButtonUp(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnLeftButtonDoubleClick Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnLeftButtonDoubleClick event processing
   */
  NS_IMETHOD_(nsEventStatus) OnLeftButtonDoubleClick(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMiddleButtonDown Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMiddleButtonDown event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMiddleButtonDown(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMiddleButtonUp Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMiddleButtonUp event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMiddleButtonUp(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMiddleButtonDoubleClick Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMiddleButtonDoubleClick event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMiddleButtonDoubleClick(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnRightButtonDown Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnRightButtonDown event processing
   */
  NS_IMETHOD_(nsEventStatus) OnRightButtonDown(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnRightButtonUp Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnRightButtonUp event processing
   */
  NS_IMETHOD_(nsEventStatus) OnRightButtonUp(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnRightButtonDoubleClick Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnRightButtonDoubleClick event processing
   */
  NS_IMETHOD_(nsEventStatus) OnRightButtonDoubleClick(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMouseEnter Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMouseEnter event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMouseEnter(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMouseExit Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMouseExit event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMouseExit(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnMouseMove Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnMouseMove event processing
   */
  NS_IMETHOD_(nsEventStatus) OnMouseMove(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnKeyUp Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnKeyUp event processing
   */
  NS_IMETHOD_(nsEventStatus) OnKeyUp(nsGUIEvent *aEvent) = 0;

  /**
   * Handle an OnKeyDown Message
   * @param aEvent The GUI Event to be handled
   * @result nsEventStatus, status of OnKeyDown event processing
   */
  NS_IMETHOD_(nsEventStatus) OnKeyDown(nsGUIEvent *aEvent) = 0;

  /**
   * Create an nsIWidget for this canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD CreateWidget() = 0;

  /**
   * Get the nsIWidget aggregated by this canvas
   * @result nsIWidget pointer, nsnull if no widget aggregated, else the nsIWidget interface
   */
  NS_IMETHOD_(nsIWidget *) GetWidget() = 0;

  /**
   * Get the parent canvas
   * @result nsIXPFCCanvas pointer, nsnull if no parent, else the nsIXPFCCanvas interface
   */
  NS_IMETHOD_(nsIXPFCCanvas *) GetParent() = 0;

  /**
   * Set the parent canvas
   * @param aCanvas The parent canvas
   * @result none
   */
  NS_IMETHOD_(void) SetParent(nsIXPFCCanvas * aCanvas) = 0;

  /**
   * Set the bounds for this container
   * @param aBounds The new canvas bounds
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetBounds(const nsRect& aBounds) = 0;

  /**
   * Get the bounds for this container
   * @param aRect Reference to an nsRect for the bounds
   * @result none
   */
  NS_IMETHOD_(void) GetBounds(nsRect& aRect) = 0;

  /**
   * Add a child canvas
   * @param aChildCanvas The child canvas to add
   * @param aPosition a position for inserting, default -1 means append
   * @result none
   */
  NS_IMETHOD_(void) AddChildCanvas(nsIXPFCCanvas * aChildCanvas, 
                                   PRInt32 aPosition = -1) = 0;

  /**
   * Remove a child canvas
   * @param aChildCanvas The child canvas to be removed
   * @result none
   */
  NS_IMETHOD_(void) RemoveChildCanvas(nsIXPFCCanvas * aChildCanvas) = 0;

  /**
   * Reparent this canvas
   * @param aParentCanvas The new parent
   * @result none
   */
  NS_IMETHOD_(void) Reparent(nsIXPFCCanvas * aParentCanvas) = 0;

  /**
   * Get the background color for this canvas
   * @result nscolor the background color
   */
  NS_IMETHOD_(nscolor) GetBackgroundColor(void) = 0;

  /**
   * Set the background color for this canvas
   * @param aColor The background color
   * @result none
   */
  NS_IMETHOD_(void)    SetBackgroundColor(const nscolor &aColor) = 0;

  /**
   * Get the foreground color for this canvas
   * @result nscolor the foreground color
   */
  NS_IMETHOD_(nscolor) GetForegroundColor(void) = 0;

  /**
   * Set the foreground color for this canvas
   * @param aColor The foreground color
   * @result none
   */
  NS_IMETHOD_(void)    SetForegroundColor(const nscolor &aColor) = 0;

  /**
   * Get the border color for this canvas
   * @result nscolor the border color
   */
  NS_IMETHOD_(nscolor) GetBorderColor(void) = 0;

  /**
   * Set the border color for this canvas
   * @param aColor The border color
   * @result none
   */
  NS_IMETHOD_(void)  SetBorderColor(const nscolor &aColor) = 0;

  /**
   * Highlight the incoming color
   * @param aColor the color to be highlighted
   * @result nscolor, the highlighted color
   */
  NS_IMETHOD_(nscolor) Highlight(const nscolor &aColor) = 0;

  /**
   * Dim the incoming color
   * @param aColor the color to be dimmed
   * @result nscolor, the dimmed color
   */
  NS_IMETHOD_(nscolor) Dim(const nscolor &aColor) = 0;

  /**
   * Find the topmost canvas given an nsPoint
   * @param aPoint The point over the canvas
   * @result nsIXPFCCanvas pointer, the canvas under the point
   */
  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromPoint(const nsPoint &aPoint) = 0;

  /**
   * Find the canvas with the given tabbing data
   * @param aTabGroup The tab group
   * @param aTabID The tab id
   * @result nsIXPFCCanvas pointer, the canvas with the tab data
   */
  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromTab(PRUint32 aTabGroup, PRUint32 aTabID) = 0;

  /**
   * The the preferred size for canvas's of this class
   * @param aSize, the default size for this class of canvas
   * @return nsresult, NS_OK if successful
   */
  NS_IMETHOD GetClassPreferredSize(nsSize& aSize) = 0;

  /**
   * Get the preferred size for this canvas
   * @param aSize, the preferred size, [-1,-1] if no preferred size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  GetPreferredSize(nsSize &aSize) = 0;

  /**
   * Get the maximum size for this canvas
   * @param aSize, the maximum size, [-1,-1] if no maximum size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  GetMaximumSize(nsSize &aSize) = 0;

  /**
   * Get the minimum size for this canvas
   * @param aSize, the minimum size, [-1,-1] if no minimum size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  GetMinimumSize(nsSize &aSize) = 0;

  /**
   * Set the preferred size for this canvas
   * @param aSize, the preferred size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetPreferredSize(nsSize &aSize) = 0;

  /**
   * Set the maximum size for this canvas
   * @param aSize, the maximum size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetMaximumSize(nsSize &aSize) = 0;

  /**
   * Set the minimum size for this canvas
   * @param aSize, the minimum size
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetMinimumSize(nsSize &aSize) = 0;

  /**
   * Get the major axis weight
   * @result PRFloat64, The weight for the major axis
   */
  NS_IMETHOD_(PRFloat64) GetMajorAxisWeight() = 0;

  /**
   * Get the minor axis weight
   * @result PRFloat64, The weight for the minor axis
   */
  NS_IMETHOD_(PRFloat64) GetMinorAxisWeight() = 0;

  /**
   * Set the major axis weight
   * @param aWeight, The weight for the major axis
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetMajorAxisWeight(PRFloat64 aWeight) = 0;

  /**
   * Set the minor axis weight
   * @param aWeight, The weight for the minor axis
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetMinorAxisWeight(PRFloat64 aWeight) = 0;

  /**
   * Check to see if this container has a preferred size associated with it
   * @result PRBool, PR_TRUE if the canvas has a preferred size, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) HasPreferredSize() = 0;

  /**
   * Check to see if this container has a minimu size associated with it
   * @result PRBool, PR_TRUE if the canvas has a minimum size, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) HasMinimumSize() = 0;

  /**
   * Check to see if this container has a maximum size associated with it
   * @result PRBool, PR_TRUE if the canvas has a maximum size, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) HasMaximumSize() = 0;

  /**
   * Get the number of children this canvas has
   * @result PRUint32, The number of children
   */
  NS_IMETHOD_(PRUint32) GetChildCount() = 0;

  /**
   * Delete all child canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD DeleteChildren() = 0;

  /**
   * Delete aCount child canvas's, removing from the end
   * @param aCount, the number of children to be deleted
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD DeleteChildren(PRUint32 aCount) = 0;

  /**
   * Find the canvas with the given name
   * @param aName, the name to search for
   * @result nsIXPFCCanvas pointer, The canvas with aName, nsnull if none found
   */
  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromName(nsString& aName) = 0;

  /**
   * Set the opacity for this canvas
   * @param aOpacity, the opacity
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetOpacity(PRFloat64 aOpacity) = 0;

  /**
   * Get the opacity for this canvas
   * @result PRFloat64, the opacity
   */
  NS_IMETHOD_(PRFloat64) GetOpacity() = 0;

  /**
   * Set the visibility of this canvas
   * @param aVisibility, PR_TRUE if visible
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD  SetVisibility(PRBool aVisibility) = 0;

  /**
   * Get the visibility of this canvas
   * @result PRBool, PR_TRUE if visible, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) GetVisibility() = 0;

  /**
   * Get the font for this canvas
   * @result nsFont, the canvas font
   */
  NS_IMETHOD_(nsFont&) GetFont() = 0;

  /**
   * Set the font for this canvas
   * @param aFont the nsFont for this canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD SetFont(nsFont& aFont) = 0;

  /**
   * Get the font metrics for this canvas
   * @param nsIFontMetrics, the canvas font metrics
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD GetFontMetrics(nsIFontMetrics ** aFontMetrics) = 0;


  /**
   * Save canvas graphical state
   * @param aRenderingContext, rendering context to save state to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD PushState(nsIRenderingContext& aRenderingContext) = 0;

  /**
   * Get and and set RenderingContext to this graphical state
   * @param aRenderingContext, rendering context to get previously saved state from
   * @return if PR_TRUE, indicates that the clipping region after
   *         popping state is empty, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) PopState(nsIRenderingContext& aRenderingContext) = 0;

  /**
   * Set the tab id for this canvas
   * @param aTabID, the tab id
   * @return nsresult NS_OK, if successful
   */
  NS_IMETHOD  SetTabID(PRUint32 aTabID) = 0;

  /**
   * Get the tab id for this canvas
   * @return PRUint32 the tab id
   */
  NS_IMETHOD_(PRUint32) GetTabID() = 0;

  /**
   * Set the tab group for this canvas
   * @param aTabGroup, the tab id
   * @return nsresult NS_OK, if successful
   */
  NS_IMETHOD  SetTabGroup(PRUint32 aTabGroup) = 0;

  /**
   * Get the tab group for this canvas
   * @return PRUint32 the tab group
   */
  NS_IMETHOD_(PRUint32) GetTabGroup() = 0;

  /**
   * Set keyboard focus to this canvas
   * @return nsresult, NS_OK if successful
   */
  NS_IMETHOD SetFocus() = 0;

  NS_IMETHOD FindLargestTabGroup(PRUint32& aTabGroup) = 0;
  NS_IMETHOD FindLargestTabID(PRUint32 aTabGroup, PRUint32& aTabID) = 0;

  NS_IMETHOD SetCommand(nsString& aCommand) = 0;
  NS_IMETHOD_(nsString&) GetCommand() = 0;
  NS_IMETHOD_(nsEventStatus) ProcessCommand(nsIXPFCCommand * aCommand) = 0;

  /**
   * Dump the canvas hierarchy to a file
   * @param f, the FILE to dump to
   * @param indent, the indentation level for current canvas list
   * @result nsresult, NS_OK if successful
   */
#if defined(DEBUG) && defined(XP_PC)
  NS_IMETHOD  DumpCanvas(FILE * f, PRUint32 indent) = 0 ;
#endif

  /**
   * Load a Widget with the given class ID
   * @param aClassIID, The class of the nsIWidget implementation
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD LoadWidget(const nsCID &aClassIID) = 0;

};

#endif /* nsIXPFCCanvas_h___ */
