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

// foreward declaration
class nsIToolkit;
class nsIFontMetrics;
class nsIToolkit;
class nsIRenderingContext;
class nsIEnumerator;
class nsIDeviceContext;
struct nsRect;
struct nsFont;

class nsIPresContext;

/**
 * Callback function that deals with events.
 * The argument is actually a subtype (subclass) of nsEvent which carries
 * platform specific information about the event. Platform specific code knows
 * how to deal with it.
 * The return value determines whether or not the default action should take place.
 */

typedef nsEventStatus (*PR_CALLBACK EVENT_CALLBACK)(nsGUIEvent *event);

/**
 * Flags for the getNativeData function.
 * @see getNativeData()
 */
#define NS_NATIVE_WINDOW    0
#define NS_NATIVE_GRAPHIC   1
#define NS_NATIVE_COLORMAP  2

// {18032AD5-B265-11d1-AA2A-000000000000}
#define NS_IWIDGET_IID \
{ 0x18032ad5, 0xb265, 0x11d1, \
{ 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


// Hide the native window systems real window type so as to avoid
// including native window system types and api's. This makes it
// easier to write cross-platform code.
typedef void* nsNativeWindow;

enum nsCursor { eCursor_standard, eCursor_wait, eCursor_select, eCursor_hyperlink }; 

class nsIWidget : public nsISupports {

public:

    /**
     * Create and initialize a widget. All the arguments can be NULL in which case
     * a top level window with size 0 is created. The event callback function has to
     * be provided only if the caller wants to deal with the events this widget receives.
     * The event callback is basically a preprocess hook called synchronously. The return
     * value determines whether the event goes to the default window procedure or it is
     * hidden to the os. The assumption is that if the event handler returns false the
     * widget does not see the event.
     *
     * @param      parent or null if it's a top level window
     * @param      the widget dimension
     * @param      the event handler callback function
     * @return     void
     *
     **/
    virtual void Create(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit *aToolkit = nsnull) = 0;

    /**
     * This Create takes a native window as a parent instead of a nsIWidget
     **/
    virtual void Create(nsNativeWindow aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit *aToolkit = nsnull) = 0;

    /**
     * Close and destroy the current window. The object will still be around
     *
     **/
    virtual void Destroy(void) = 0;

    /**
     * Return the parent Widget of this Widget or NULL if this is a top level window
     *
     * @return     nsWidget*, the parent Widget
     *
     **/
    virtual nsIWidget* GetParent(void) = 0;

    /**
     * Return an nsEnumerator over the children of this widget or null if no
     * children.
     *
     * @return an enumerator over the list of children
     *
     **/
    virtual nsIEnumerator*  GetChildren(void) = 0;

    /**
     * used internally to add a remove from the children list
     *
     **/
    virtual void AddChild(nsIWidget* aChild) = 0;
    virtual void RemoveChild(nsIWidget* aChild) = 0;

    /**
     * Show/hide the Widget according to the boolean argument
     *
     * @param aState PR_TRUE to show the Widget, PR_FALSE to hide it
     *
     **/
    virtual void Show(PRBool aState) = 0;

    /**
     * Move the Widget. The coordinates are expressed in the parent coordinate system.
     *
     * @param aX the new x position
     * @param aY the new y position
     *
     **/
    virtual void Move(PRUint32 aX, PRUint32 aY) = 0;

    /**
     * Move/resize/reshape the Widget. The coordinates are expressed in
     * the parent coordinate system.
     *
     * @param aWidth  the new width
     * @param aHeight the new height
     *
     */
    virtual void Resize(PRUint32 aWidth,
                        PRUint32 aHeight) = 0;

    /**
     * Move/resize/reshape the Widget. The coordinates are expressed in
     * the parent coordinate system.
     *
     * @param     aX      the new x position
     * @param     aY      the new y position
     * @param     aWidth  the new width
     * @param     aHeight the new height
     *
     */
    virtual void Resize(PRUint32 aX,
                        PRUint32 aY,
                        PRUint32 aWidth,
                        PRUint32 aHeight) = 0;

    /**
     * Enable/Disable the Widget according to the boolean argument
     *
     * @param aState PR_TRUE to enable the Widget, PR_FALSE to disable it
     *
     */
    virtual void Enable(PRBool aState) = 0;

    /**
     * Give focus to this Widget
     *
     */
    virtual void SetFocus(void) = 0;

    /**
     * Get this Widget dimension
     *
     * @param aRect, on return has the dimension of this Widget
     *
     */
    virtual void GetBounds(nsRect &aRect) = 0;

    /**
     * Get the  foreground color
     * @returns the current foreground color
     *
     */
    virtual nscolor GetForegroundColor(void) = 0;

    /**
     * Set the foreground color
     * @param aColor the new foreground color
     *
     */

    virtual void SetForegroundColor(const nscolor &aColor) = 0;

    /**
     * Get the background color
     * @returns the current background color
     *
     */

    virtual nscolor GetBackgroundColor(void) = 0;

    /**
     * Set the background color
     * @param aColor the new background color
     * @returns the current background color
     *
     */

    virtual void SetBackgroundColor(const nscolor &aColor) = 0;

    /**
     * Get the font 
     * @returns the font metrics 
     */

    virtual nsIFontMetrics* GetFont(void) = 0;

    /**
     * Set the font 
     * @param aFont font to display. @see nsFont for allowable fonts
     */

    virtual void SetFont(const nsFont &aFont) = 0;

    /**
     * Get the cursor for this Widget.
     * @returns the cursor used by the widget
     */

    virtual nsCursor GetCursor(void) = 0;

    /**
     * Set the cursor for this Widget.
     * @aCursor the cursor for this widget
     */

    virtual void SetCursor(nsCursor aCursor) = 0;

    /**
     * Invalidate the Widget and repaint
     *
     * @param     aIsSynchronouse repaint synchronously if aIsSynchronous is PR_TRUE/
     *
     **/
    virtual void Invalidate(PRBool aIsSynchronous) = 0;

  
    /**
     * Adds a MouseListener to the widget
     *
     */
    virtual void AddMouseListener(nsIMouseListener * aListener) = 0;

    /**
     * Return the widget's toolkit
     * @return the toolkit this widget was created in. @see nsToolkit.h
     *
     */
    virtual nsIToolkit* GetToolkit() = 0;    

    /**
     * Set the color map for the widget
     * @param aColorMap color map for displaying the widget
     *
     */

    virtual void SetColorMap(nsColorMap *aColorMap) = 0;

    /**
     * Scroll the widget
     *
     * @param aDx amount to scroll along the x-axis
     * @param aDy amount to scroll along the y-axis.
     * @param aClipRect clipping rectangle to limit the scroll to.
     *
     */

    virtual void Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) = 0;

    /*
     * Internal methods
     */

    virtual void* GetNativeData(PRUint32 aDataType) = 0;
    virtual nsIRenderingContext* GetRenderingContext() = 0;
    virtual nsIDeviceContext* GetDeviceContext() = 0;

};

#endif // nsIWidget_h__
