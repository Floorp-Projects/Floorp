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

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include "nsISupports.h"
#include "nsColor.h"
#include "nsIMouseListener.h"
#include "nsIImage.h"

#include "prthread.h"
#include "nsGUIEvent.h"

// forward declarations
class   nsIToolkit;
class   nsIFontMetrics;
class   nsIToolkit;
class   nsIRenderingContext;
class   nsIEnumerator;
class   nsIDeviceContext;
struct  nsRect;
struct  nsFont;

/**
 * Callback function that processes events.
 * The argument is actually a subtype (subclass) of nsEvent which carries
 * platform specific information about the event. Platform specific code knows
 * how to deal with it.
 * The return value determines whether or not the default action should take place.
 */

typedef nsEventStatus (*PR_CALLBACK EVENT_CALLBACK)(nsGUIEvent *event);

/**
 * Flags for the getNativeData function.
 * See getNativeData()
 */
#define NS_NATIVE_WINDOW    0
#define NS_NATIVE_GRAPHIC   1
#define NS_NATIVE_COLORMAP  2

// {18032AD5-B265-11d1-AA2A-000000000000}
#define NS_IWIDGET_IID \
{ 0x18032ad5, 0xb265, 0x11d1, \
{ 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


// Hide the native window systems real window type so as to avoid
// including native window system types and api's. This is necessary
// to ensure cross-platform code.
typedef void* nsNativeWindow;

/**
 * Border styles
 */

enum nsBorderStyle {   
                  ///no border
                eBorderStyle_none,
                  ///dialog box border + title area
                eBorderStyle_dialog, 
              }; 

/**
 * Cursor types.
 */

enum nsCursor {   ///(normal cursor,       usually rendered as an arrow)
                eCursor_standard, 
                  ///(system is busy,      usually rendered as a hourglass or watch)
                eCursor_wait, 
                  ///(Selecting something, usually rendered as an IBeam)
                eCursor_select, 
                  ///(can hyper-link,      usually rendered as a human hand)
                eCursor_hyperlink }; 


/**
 * Basic struct for widget initialization data.
 * @see Create member function of nsIWidget
 */

struct nsWidgetInitData { };

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIWidget : public nsISupports {

  public:

    /**
     * Create and initialize a widget. 
     *
     * The widget represents a window that can be drawn into. It also is the 
     * base class for user-interface widgets such as buttons and text boxes.
     *
     * All the arguments can be NULL in which case a top level window
     * with size 0 is created. The event callback function has to be
     * provided only if the caller wants to deal with the events this
     * widget receives.  The event callback is basically a preprocess
     * hook called synchronously. The return value determines whether
     * the event goes to the default window procedure or it is hidden
     * to the os. The assumption is that if the event handler returns
     * false the widget does not see the event. The widget should not 
     * automatically clear the window to the background color. The 
     * calling code must handle paint messages and clear the background 
     * itself. 
     *
     * @param     parent or null if it's a top level window
     * @param     aRect     the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     * @param     aInitData data that is used for widget initialization
     *
     */
    virtual void Create(nsIWidget        *aParent,
                        const nsRect     &aRect,
                        EVENT_CALLBACK   aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit       *aToolkit = nsnull,
                        nsWidgetInitData *aInitData = nsnull) = 0;

    /**
     * Create and initialize a widget with a native window parent
     *
     * The widget represents a window that can be drawn into. It also is the 
     * base class for user-interface widgets such as buttons and text boxes.
     *
     * All the arguments can be NULL in which case a top level window
     * with size 0 is created. The event callback function has to be
     * provided only if the caller wants to deal with the events this
     * widget receives.  The event callback is basically a preprocess
     * hook called synchronously. The return value determines whether
     * the event goes to the default window procedure or it is hidden
     * to the os. The assumption is that if the event handler returns
     * false the widget does not see the event.
     *
     * @param     aParent   native window.
     * @param     aRect     the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     */
    virtual void Create(nsNativeWindow aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit *aToolkit = nsnull,
                        nsWidgetInitData *aInitData = nsnull) = 0;

    /**
     * Close and destroy the internal native window. 
     * This method does not delete the widget.
     */

    virtual void Destroy(void) = 0;

    /**
     * Return the parent Widget of this Widget or nsnull if this is a 
     * top level window
     *
     * @return the parent widget or nsnull if it does not have a parent
     *
     */
    virtual nsIWidget* GetParent(void) = 0;

    /**
     * Return an nsEnumerator over the children of this widget.
     *
     * @return an enumerator over the list of children or nsnull if it does not
     * have any children
     *
     */
    virtual nsIEnumerator*  GetChildren(void) = 0;

    /**
     * Show or hide this widget
     *
     * @param aState PR_TRUE to show the Widget, PR_FALSE to hide it
     *
     */
    virtual void Show(PRBool aState) = 0;

    /**
     * Move this widget.
     *
     * @param aX the new x position expressed in the parent's coordinate system
     * @param aY the new y position expressed in the parent's coordinate system
     *
     **/
    virtual void Move(PRUint32 aX, PRUint32 aY) = 0;

    /**
     * Resize this widget. 
     *
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     *
     */
    virtual void Resize(PRUint32 aWidth,
                        PRUint32 aHeight) = 0;

    /**
     * Move or resize this widget.
     *
     * @param aX      the new x position expressed in the parent's coordinate system
     * @param aY      the new y position expressed in the parent's coordinate system
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     *
     */
    virtual void Resize(PRUint32 aX,
                        PRUint32 aY,
                        PRUint32 aWidth,
                        PRUint32 aHeight) = 0;

    /**
     * Enable or disable this Widget
     *
     * @param aState PR_TRUE to enable the Widget, PR_FALSE to disable it.
     *
     */
    virtual void Enable(PRBool aState) = 0;

    /**
     * Give focus to this widget.
     */
    virtual void SetFocus(void) = 0;

    /**
     * Get this widget's dimension
     *
     * @param aRect on return it holds the  x. y, width and height of this widget
     *
     */
    virtual void GetBounds(nsRect &aRect) = 0;

    /**
     * Get the foreground color for this widget
     *
     * @return this widget's foreground color
     *
     */
    virtual nscolor GetForegroundColor(void) = 0;

    /**
     * Set the foreground color for this widget
     *
     * @param aColor the new foreground color
     *
     */

    virtual void SetForegroundColor(const nscolor &aColor) = 0;

    /**
     * Get the background color for this widget
     *
     * @return this widget's background color
     *
     */

    virtual nscolor GetBackgroundColor(void) = 0;

    /**
     * Set the background color for this widget
     *
     * @param aColor the new background color
     *
     */

    virtual void SetBackgroundColor(const nscolor &aColor) = 0;

    /**
     * Get the font for this widget
     *
     * @return the font metrics 
     */

    virtual nsIFontMetrics* GetFont(void) = 0;

    /**
     * Set the font for this widget 
     *
     * @param aFont font to display. See nsFont for allowable fonts
     */

    virtual void SetFont(const nsFont &aFont) = 0;

    /**
     * Get the cursor for this widget.
     *
     * @return this widget's cursor.
     */

    virtual nsCursor GetCursor(void) = 0;

    /**
     * Set the cursor for this widget
     *
     * @param aCursor the new cursor for this widget
     */

    virtual void SetCursor(nsCursor aCursor) = 0;

    /**
     * Invalidate the widget and repaint it.
     *
     * @param aIsSynchronouse PR_TRUE then repaint synchronously. If PR_FALSE repaint later.
     *
     */

    virtual void Invalidate(PRBool aIsSynchronous) = 0;

  
    /**
     * Adds a mouse listener to this widget
     * Any existing mouse listener is replaced
     *
     * @param aListener mouse listener to add to this widget.
     */

    virtual void AddMouseListener(nsIMouseListener * aListener) = 0;

    /**
     * Return the widget's toolkit
     *
     * @return the toolkit this widget was created in. See nsToolkit.
     */

    virtual nsIToolkit* GetToolkit() = 0;    

    /**
     * Set the color map for this widget
     *
     * @param aColorMap color map for displaying this widget
     *
     */

    virtual void SetColorMap(nsColorMap *aColorMap) = 0;

    /**
     * Scroll this widget.
     *
     * @param aDx amount to scroll along the x-axis
     * @param aDy amount to scroll along the y-axis.
     * @param aClipRect clipping rectangle to limit the scroll to.
     *
     */

    virtual void Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) = 0;

    /** 
     * Internal methods
     */

    //@{
    virtual void AddChild(nsIWidget* aChild) = 0;
    virtual void RemoveChild(nsIWidget* aChild) = 0;
    virtual void* GetNativeData(PRUint32 aDataType) = 0;
    virtual nsIRenderingContext* GetRenderingContext() = 0;
    virtual nsIDeviceContext* GetDeviceContext() = 0;
    //@}

    /**
     * Set border style
     * Must be called before Create.
     * @param aBorderStyle @see nsBorderStyle
     */

    virtual void SetBorderStyle(nsBorderStyle aBorderStyle) = 0;

    /**
     * Set the widget's title.
     * Must be called after Create.
     *
     * @param aTitle string displayed as the title of the widget
     */

    virtual void SetTitle(const nsString& aTitle) = 0;

    /**
     * Set the collection of tooltip rectangles.
     * A NS_SHOW_TOOLTIP event is generated when the mouse hovers over one
     * of the rectangles. a NS_HIDE_TOOLTIP event is generated when the mouse
     * is moved or a new tooltip is displayed.
     *
     * @param      aNumberOfTips    number of tooltip areas.
     * @param      aTooltipArea     array of x,y,width,height rectangles specifying hot areas
     *
     */

    virtual void SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]) = 0;

    /**
     * Update the collection of tooltip rectangles. The number of tooltips must
     * match the original number of tooltips specified in SetTooltips. Must be called
     * after calling SetTooltips.
     *
     * @param      aNewTips     array of x,y,width,height rectangles specifying the new hot areas
     *
     */

    virtual void UpdateTooltips(nsRect* aNewTips[]) = 0;

    /**
     * Remove the collection of tooltip rectangles.
     */

    virtual void RemoveTooltips() = 0;

     /**
     * Convert from this widget coordinates to screen coordinates.
     *
     * @param  aOldRect  widget coordinates stored in the x,y members
     * @param  aNewRect  screen coordinates stored in the x,y members
     */

    virtual void WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect) = 0;

    /**
     * Convert from screen coordinates to this widget's coordinates.
     *
     * @param  aOldRect  screen coordinates stored in the x,y members
     * @param  aNewRect  widget's coordinates stored in the x,y members
     */

    virtual void ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect) = 0;

    /**
     * When adjustments are to made to a whole set of child widgets, call this
     * before resizing/positioning the child windows to minimize repaints. Must
     * be followed by EndResizingChildren() after child windows have been
     * adjusted.
     *
     */

    virtual void BeginResizingChildren(void) = 0;

    /**
     * Call this when finished adjusting child windows. Must be preceded by
     * BeginResizingChildren().
     *
     */

    virtual void EndResizingChildren(void) = 0;
};

#endif // nsIWidget_h__
