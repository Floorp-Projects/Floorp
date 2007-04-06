/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include "nsISupports.h"
#include "nsColor.h"
#include "nsIMouseListener.h"
#include "nsIMenuListener.h"
#include "nsCoord.h"

#include "prthread.h"
#include "nsEvent.h"
#include "nsCOMPtr.h"

// forward declarations
class   nsIAppShell;
class   nsIToolkit;
class   nsIFontMetrics;
class   nsIRenderingContext;
class   nsIDeviceContext;
class   nsIRegion;
struct  nsRect;
struct  nsFont;
class   nsIMenuBar;
class   nsIEventListener;
class   nsIRollupListener;
class   nsGUIEvent;
struct  nsColorMap;
class   imgIContainer;

#ifdef MOZ_CAIRO_GFX
class   gfxASurface;
#endif

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
#define NS_NATIVE_WINDOW      0
#define NS_NATIVE_GRAPHIC     1
#define NS_NATIVE_COLORMAP    2
#define NS_NATIVE_WIDGET      3
#define NS_NATIVE_DISPLAY     4
#define NS_NATIVE_REGION      5
#define NS_NATIVE_OFFSETX     6
#define NS_NATIVE_OFFSETY     7
#define NS_NATIVE_PLUGIN_PORT 8
#define NS_NATIVE_SCREEN      9
#define NS_NATIVE_SHELLWIDGET 10      // Get the shell GtkWidget
#ifdef XP_MACOSX
#define NS_NATIVE_PLUGIN_PORT_QD    100
#define NS_NATIVE_PLUGIN_PORT_CG    101
#endif

// B3F10C8D-4C07-4B1E-A1CD-B38696426205
#define NS_IWIDGET_IID \
{ 0xB3F10C8D, 0x4C07, 0x4B1E, \
  { 0xA1, 0xCD, 0xB3, 0x86, 0x96, 0x42, 0x62, 0x05 } }


// Hide the native window systems real window type so as to avoid
// including native window system types and api's. This is necessary
// to ensure cross-platform code.
typedef void* nsNativeWidget;

/**
 * Border styles
 */

enum nsWindowType {     // Don't alter previously encoded enum values - 3rd party apps may look at these
  // default top level window
  eWindowType_toplevel,
  // top level window but usually handled differently by the OS
  eWindowType_dialog,
  // used for combo boxes, etc
  eWindowType_popup,
  // child windows (contained inside a window on the desktop (has no border))
  eWindowType_child,
  // windows that are invisible or offscreen
  eWindowType_invisible,
  // plugin window
  eWindowType_plugin,
  // java plugin window
  eWindowType_java,
  // MacOSX sheet (special dialog class)
  eWindowType_sheet
};

enum nsBorderStyle
{
  // no border, titlebar, etc.. opposite of all
  eBorderStyle_none     = 0,

  // all window decorations
  eBorderStyle_all      = 1 << 0,

  // enables the border on the window.  these are only for decoration and are not resize hadles
  eBorderStyle_border   = 1 << 1,

  // enables the resize handles for the window.  if this is set, border is implied to also be set
  eBorderStyle_resizeh  = 1 << 2,

  // enables the titlebar for the window
  eBorderStyle_title    = 1 << 3,

  // enables the window menu button on the title bar.  this being on should force the title bar to display
  eBorderStyle_menu     = 1 << 4,

  // enables the minimize button so the user can minimize the window.
  //   turned off for tranient windows since they can not be minimized separate from their parent
  eBorderStyle_minimize = 1 << 5,

  // enables the maxmize button so the user can maximize the window
  eBorderStyle_maximize = 1 << 6,

  // show the close button
  eBorderStyle_close    = 1 << 7,

  // whatever the OS wants... i.e. don't do anything
  eBorderStyle_default  = -1
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
                eCursor_hyperlink, 
                  ///(north/south/west/east edge sizing)
                eCursor_n_resize,
                eCursor_s_resize,
                eCursor_w_resize,
                eCursor_e_resize,
                  ///(corner sizing)
                eCursor_nw_resize,
                eCursor_se_resize,
                eCursor_ne_resize,
                eCursor_sw_resize,
                eCursor_crosshair,
                eCursor_move,
                eCursor_help,
                eCursor_copy, // CSS3
                eCursor_alias,
                eCursor_context_menu,
                eCursor_cell,
                eCursor_grab,
                eCursor_grabbing,
                eCursor_spinning,
                eCursor_zoom_in,
                eCursor_zoom_out,
                eCursor_not_allowed,
                eCursor_col_resize,
                eCursor_row_resize,
                eCursor_no_drop,
                eCursor_vertical_text,
                eCursor_all_scroll,
                eCursor_nesw_resize,
                eCursor_nwse_resize,
                eCursor_ns_resize,
                eCursor_ew_resize,
                // This one better be the last one in this list.
                eCursorCount
                }; 

enum nsContentType {
  eContentTypeInherit = -1,
  eContentTypeUI = 0,
  eContentTypeContent = 1,
  eContentTypeContentFrame = 2
};

enum nsTopLevelWidgetZPlacement { // for PlaceBehind()
  eZPlacementBottom = 0,  // bottom of the window stack
  eZPlacementBelow,       // just below another widget
  eZPlacementTop          // top of the window stack
};

/**
 * Basic struct for widget initialization data.
 * @see Create member function of nsIWidget
 */

struct nsWidgetInitData {
  nsWidgetInitData()
    : clipChildren(PR_FALSE), 
      clipSiblings(PR_FALSE), 
      mDropShadow(PR_FALSE),
      mListenForResizes(PR_FALSE),
      mWindowType(eWindowType_child),
      mBorderStyle(eBorderStyle_default),
      mContentType(eContentTypeInherit),
      mUnicode(PR_TRUE)
  {
  }

  // when painting exclude area occupied by child windows and sibling windows
  PRPackedBool  clipChildren, clipSiblings, mDropShadow;
  PRPackedBool  mListenForResizes;
  nsWindowType mWindowType;
  nsBorderStyle mBorderStyle;
  nsContentType mContentType;  // Exposed so screen readers know what's UI
  PRPackedBool mUnicode;
};

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIWidget : public nsISupports {

  public:

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWIDGET_IID)

    nsIWidget()
      : mLastChild(nsnull)
      , mPrevSibling(nsnull)
    {}

        
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
     * aInitData cannot be eWindowType_popup here; popups cannot be
     * hooked into the nsIWidget hierarchy.
     *
     * @param     parent or null if it's a top level window
     * @param     aRect     the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     * @param     aContext
     * @param     aAppShell the parent application shell. If nsnull,
     *                      the parent window's application shell will be used.
     * @param     aToolkit
     * @param     aInitData data that is used for widget initialization
     *
     */
    NS_IMETHOD Create(nsIWidget        *aParent,
                        const nsRect     &aRect,
                        EVENT_CALLBACK   aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell      *aAppShell = nsnull,
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
    NS_IMETHOD Create(nsNativeWidget aParent,
                        const nsRect     &aRect,
                        EVENT_CALLBACK   aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell      *aAppShell = nsnull,
                        nsIToolkit       *aToolkit = nsnull,
                        nsWidgetInitData *aInitData = nsnull) = 0;


    /**
     * Accessor functions to get and set the client data associated with the
     * widget.
     */
    //@{
    NS_IMETHOD  GetClientData(void*& aClientData) = 0;
    NS_IMETHOD  SetClientData(void* aClientData) = 0;
    //@}

    /**
     * Close and destroy the internal native window. 
     * This method does not delete the widget.
     */

    NS_IMETHOD Destroy(void) = 0;


    /**
     * Reparent a widget
     *
     * Change the widgets parent
     *
     * @param     aNewParent   new parent 
     */
    NS_IMETHOD SetParent(nsIWidget* aNewParent) = 0;


    /**
     * Return the parent Widget of this Widget or nsnull if this is a 
     * top level window
     *
     * @return the parent widget or nsnull if it does not have a parent
     *
     */
    virtual nsIWidget* GetParent(void) = 0;

    /**
     * Return the first child of this widget.  Will return null if
     * there are no children.
     */
    nsIWidget* GetFirstChild() const {
        return mFirstChild;
    }
    
    /**
     * Return the last child of this widget.  Will return null if
     * there are no children.
     */
    nsIWidget* GetLastChild() const {
        return mLastChild;
    }

    /**
     * Return the next sibling of this widget
     */
    nsIWidget* GetNextSibling() const {
        return mNextSibling;
    }
    
    /**
     * Set the next sibling of this widget
     */
    void SetNextSibling(nsIWidget* aSibling) {
        mNextSibling = aSibling;
    }
    
    /**
     * Return the previous sibling of this widget
     */
    nsIWidget* GetPrevSibling() const {
        return mPrevSibling;
    }

    /**
     * Set the previous sibling of this widget
     */
    void SetPrevSibling(nsIWidget* aSibling) {
        mPrevSibling = aSibling;
    }

    /**
     * Show or hide this widget
     *
     * @param aState PR_TRUE to show the Widget, PR_FALSE to hide it
     *
     */
    NS_IMETHOD Show(PRBool aState) = 0;

    /**
     * Make the window modal
     *
     */
    NS_IMETHOD SetModal(PRBool aModal) = 0;

    /**
     * Returns whether the window is visible
     *
     */
    NS_IMETHOD IsVisible(PRBool & aState) = 0;

    /**
     * Perform platform-dependent sanity check on a potential window position.
     * This is guaranteed to work only for top-level windows.
     *
     * @param aAllowSlop: if true, allow the window to slop offscreen;
     *                    the window should be partially visible. if false,
     *                    force the entire window onscreen (or at least
     *                    the upper-left corner, if it's too large).
     * @param aX in: an x position expressed in screen coordinates.
     *           out: the x position constrained to fit on the screen(s).
     * @param aY in: an y position expressed in screen coordinates.
     *           out: the y position constrained to fit on the screen(s).
     * @return vapid success indication. but see also the parameters.
     *
     **/
    NS_IMETHOD ConstrainPosition(PRBool aAllowSlop,
                                 PRInt32 *aX,
                                 PRInt32 *aY) = 0;

    /**
     * Move this widget.
     *
     * @param aX the new x position expressed in the parent's coordinate system
     * @param aY the new y position expressed in the parent's coordinate system
     *
     **/
    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY) = 0;

    /**
     * Resize this widget. 
     *
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD Resize(PRInt32 aWidth,
                      PRInt32 aHeight,
                      PRBool   aRepaint) = 0;

    /**
     * Move or resize this widget.
     *
     * @param aX       the new x position expressed in the parent's coordinate system
     * @param aY       the new y position expressed in the parent's coordinate system
     * @param aWidth   the new width expressed in the parent's coordinate system
     * @param aHeight  the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted if the size changes
     *
     */
    NS_IMETHOD Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 aWidth,
                      PRInt32 aHeight,
                      PRBool   aRepaint) = 0;

    /**
     * Sets the widget's z-index.
     */
    NS_IMETHOD SetZIndex(PRInt32 aZIndex) = 0;

    /**
     * Gets the widget's z-index. 
     */
    NS_IMETHOD GetZIndex(PRInt32* aZIndex) = 0;

    /**
     * Position this widget just behind the given widget. (Used to
     * control z-order for top-level widgets. Get/SetZIndex by contrast
     * control z-order for child widgets of other widgets.)
     * @param aPlacement top, bottom, or below a widget
     *                   (if top or bottom, param aWidget is ignored)
     * @param aWidget    widget to place this widget behind
     *                   (only if aPlacement is eZPlacementBelow).
     *                   null is equivalent to aPlacement of eZPlacementTop
     * @param aActivate  true to activate the widget after placing it
     */
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget, PRBool aActivate) = 0;

    /**
     * Minimize, maximize or normalize the window size.
     * Takes a value from nsSizeMode (see nsGUIEvent.h)
     */
    NS_IMETHOD SetSizeMode(PRInt32 aMode) = 0;

    /**
     * Return size mode (minimized, maximized, normalized).
     * Returns a value from nsSizeMode (see nsGUIEvent.h)
     */
    NS_IMETHOD GetSizeMode(PRInt32* aMode) = 0;

    /**
     * Enable or disable this Widget
     *
     * @param aState PR_TRUE to enable the Widget, PR_FALSE to disable it.
     *
     */
    NS_IMETHOD Enable(PRBool aState) = 0;

    /**
     * Ask whether the widget is enabled
     * @param aState returns PR_TRUE if the widget is enabled
     */
    NS_IMETHOD IsEnabled(PRBool *aState) = 0;

    /**
     * Give focus to this widget.
     */
    NS_IMETHOD SetFocus(PRBool aRaise = PR_FALSE) = 0;

    /**
     * Get this widget's outside dimensions relative to its parent widget
     *
     * @param aRect on return it holds the  x, y, width and height of this widget
     *
     */
    NS_IMETHOD GetBounds(nsRect &aRect) = 0;


    /**
     * Get this widget's outside dimensions in global coordinates. (One might think this
     * could be accomplished by stringing together other methods in this interface, but
     * then one would bloody one's nose on different coordinate system handling by different
     * platforms.)
     *
     * @param aRect on return it holds the  x, y, width and height of this widget
     *
     */
    NS_IMETHOD GetScreenBounds(nsRect &aRect) = 0;


    /**
     * Get this widget's client area dimensions, if the window has a 3D border appearance
     * this returns the area inside the border, The x and y are always zero
     *
     * @param aRect on return it holds the  x. y, width and height of the client area of this widget
     *
     */
    NS_IMETHOD GetClientBounds(nsRect &aRect) = 0;

    /**
     * Gets the width and height of the borders
     * @param aWidth the width of the border
     * @param aHeight the height of the border
     *
     */
    NS_IMETHOD GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight) = 0;

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

    NS_IMETHOD SetForegroundColor(const nscolor &aColor) = 0;

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

    NS_IMETHOD SetBackgroundColor(const nscolor &aColor) = 0;

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

    NS_IMETHOD SetFont(const nsFont &aFont) = 0;

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

    NS_IMETHOD SetCursor(nsCursor aCursor) = 0;

    /**
     * Sets an image as the cursor for this widget.
     *
     * @param aCursor the cursor to set
     * @param aX the X coordinate of the hotspot (from left).
     * @param aY the Y coordinate of the hotspot (from top).
     * @retval NS_ERROR_NOT_IMPLEMENTED if setting images as cursors is not
     *         supported
     */
    NS_IMETHOD SetCursor(imgIContainer* aCursor,
                         PRUint32 aHotspotX, PRUint32 aHotspotY) = 0;

    /** 
     * Get the window type of this widget
     *
     * @param aWindowType the window type of the widget
     */
    NS_IMETHOD GetWindowType(nsWindowType& aWindowType) = 0;

    /**
     * Set the translucency of the top-level window containing this widget.
     * So, e.g., if you call this on the widget for an IFRAME, the top level
     * browser window containing the IFRAME actually gets set. Be careful.
     *
     * This can fail if the platform doesn't support
     * transparency/translucency. By default widgets are not
     * transparent.  This will also fail if the toplevel window is not
     * a Mozilla window, e.g., if the widget is in an embedded
     * context.
     *
     * After translucency has been enabled, the initial alpha channel
     * value for all pixels is 1, i.e., opaque.
     * If the window is resized then the alpha channel values for
     * all pixels are reset to 1.
     * Pixel RGB color values are already premultiplied with alpha channel values.
     * @param aTranslucent true if the window may have translucent
     *   or transparent pixels
     */
    NS_IMETHOD SetWindowTranslucency(PRBool aTranslucent) = 0;

    /**
     * Get the translucency of the top-level window that contains this
     * widget.
     * @param aTranslucent true if the window may have translucent or
     *   transparent pixels
     */
    NS_IMETHOD GetWindowTranslucency(PRBool& aTranslucent) = 0;

    /**
     * Update the alpha channel for some pixels of the top-level window
     * that contains this widget.
     * The window must have been made translucent using SetWindowTranslucency.
     * Pixel RGB color values are already premultiplied with alpha channel values.
     * @param aRect the rect to update
     * @param aAlphas the alpha values, in w x h array, row-major order,
     * in units of 1/255. nsBlender::GetAlphas is a good way to compute this array.
     */
    NS_IMETHOD UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas) = 0;

    /** 
     * Hide window chrome (borders, buttons) for this widget.
     *
     */
    NS_IMETHOD HideWindowChrome(PRBool aShouldHide) = 0;

    /**
     * Put the toplevel window into or out of fullscreen mode.
     *
     */
    NS_IMETHOD MakeFullScreen(PRBool aFullScreen) = 0;

    /**
     * Validate the widget.
     *
     */
    NS_IMETHOD Validate() = 0;

    /**
     * Invalidate the widget and repaint it.
     *
     * @param aIsSynchronous PR_TRUE then repaint synchronously. If PR_FALSE repaint later.
     * @see #Update()
     */

    NS_IMETHOD Invalidate(PRBool aIsSynchronous) = 0;

    /**
     * Invalidate a specified rect for a widget and repaints it.
     *
     * @param aIsSynchronouse PR_TRUE then repaint synchronously. If PR_FALSE repaint later.
     * @see #Update()
     */

    NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous) = 0;

    /**
     * Invalidate a specified region for a widget and repaints it.
     *
     * @param aIsSynchronouse PR_TRUE then repaint synchronously. If PR_FALSE repaint later.
     * @see #Update()
     */

    NS_IMETHOD InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous) = 0;

    /**
     * Force a synchronous repaint of the window if there are dirty rects.
     *
     * @see Invalidate()
     */

     NS_IMETHOD Update() = 0;

    /**
     * Adds a mouse listener to this widget
     * Any existing mouse listener is replaced
     *
     * @param aListener mouse listener to add to this widget.
     */

    NS_IMETHOD AddMouseListener(nsIMouseListener * aListener) = 0;

    /**
     * Adds an event listener to this widget
     * Any existing event listener is replaced
     *
     * @param aListener event listener to add to this widget.
     */

    NS_IMETHOD AddEventListener(nsIEventListener * aListener) = 0;

    /**
     * Adds a menu listener to this widget
     * Any existing menu listener is replaced
     *
     * @param aListener menu listener to add to this widget.
     */

    NS_IMETHOD AddMenuListener(nsIMenuListener * aListener) = 0;
    
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

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap) = 0;

    /**
     * XXX (This is obsolete and will be removed soon, Use ScrollWidgets instead)
     * Scroll this widget. 
     *
     * @param aDx amount to scroll along the x-axis
     * @param aDy amount to scroll along the y-axis.
     * @param aClipRect clipping rectangle to limit the scroll to.
     *
     */

    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) = 0;

    /**
     * Scroll the contents of the widget. 
     * All child widgets are also scrolled by offsetting their coordinates.
     * A NS_PAINT message is synchronously dispatched for the newly exposed rectangle.
     *
     * @param aDx amount to scroll along the x-axis in pixels
     * @param aDy amount to scroll along the y-axis in pixels
     *
     */

    NS_IMETHOD ScrollWidgets(PRInt32 aDx, PRInt32 aDy) = 0;

    /**
     * Scroll an area of this widget. Child widgets are not scrolled.
     * A NS_PAINT message is synchronously dispatched for the newly exposed rectangle.
     *
     * @param aRect source rectangle to scroll in the widget in pixels
     * @param aDx x offset from the source in pixels
     * @param aDy y offset from the source in pixels
     *
     */

    NS_IMETHOD ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy) = 0;

    /** 
     * Internal methods
     */

    //@{
    virtual void AddChild(nsIWidget* aChild) = 0;
    virtual void RemoveChild(nsIWidget* aChild) = 0;
    virtual void* GetNativeData(PRUint32 aDataType) = 0;
    virtual void FreeNativeData(void * data, PRUint32 aDataType) = 0;//~~~
    virtual nsIRenderingContext* GetRenderingContext() = 0;

    // GetDeviceContext returns a weak pointer to this widget's device context
    virtual nsIDeviceContext* GetDeviceContext() = 0;

    //@}

    /**
     * Set border style
     * Must be called before Create.
     * @param aBorderStyle @see nsBorderStyle
     */

    NS_IMETHOD SetBorderStyle(nsBorderStyle aBorderStyle) = 0;

    /**
     * Set the widget's title.
     * Must be called after Create.
     *
     * @param aTitle string displayed as the title of the widget
     */

    NS_IMETHOD SetTitle(const nsAString& aTitle) = 0;

    /**
     * Set the widget's icon.
     * Must be called after Create.
     *
     * @param anIconSpec string specifying the icon to use; convention is to pass
     *                   a resource: URL from which a platform-dependent resource
     *                   file name will be constructed
     */

    NS_IMETHOD SetIcon(const nsAString& anIconSpec) = 0;

    /**
     * Set the widget's MenuBar.
     * Must be called after Create.
     *
     * @param aMenuBar the menubar
     */

    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar) = 0;

    /**
     * Set the widget's MenuBar's visibility
     *
     * @param aShow PR_TRUE to show, PR_FALSE to hide
     */

    NS_IMETHOD ShowMenuBar(PRBool aShow) = 0;

    /**
     * Convert from this widget coordinates to screen coordinates.
     *
     * @param  aOldRect  widget coordinates stored in the x,y members
     * @param  aNewRect  screen coordinates stored in the x,y members
     */

    NS_IMETHOD WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect) = 0;

    /**
     * Convert from screen coordinates to this widget's coordinates.
     *
     * @param  aOldRect  screen coordinates stored in the x,y members
     * @param  aNewRect  widget's coordinates stored in the x,y members
     */

    NS_IMETHOD ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect) = 0;

    /**
     * When adjustments are to made to a whole set of child widgets, call this
     * before resizing/positioning the child windows to minimize repaints. Must
     * be followed by EndResizingChildren() after child windows have been
     * adjusted.
     *
     */

    NS_IMETHOD BeginResizingChildren(void) = 0;

    /**
     * Call this when finished adjusting child windows. Must be preceded by
     * BeginResizingChildren().
     *
     */

    NS_IMETHOD EndResizingChildren(void) = 0;

    /**
     * Returns the preferred width and height for the widget
     *
     */
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight) = 0;

    /**
     * Set the preferred width and height for the widget
     *
     */
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight) = 0;

    /**
     * Dispatches an event to the widget
     *
     */
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus) = 0;

    /**
     * Enables the dropping of files to a widget (XXX this is temporary)
     *
     */
    NS_IMETHOD EnableDragDrop(PRBool aEnable) = 0;
   
    virtual void  ConvertToDeviceCoordinates(nscoord &aX,nscoord &aY) = 0;

    /**
     * Enables/Disables system mouse capture.
     * @param aCapture PR_TRUE enables mouse capture, PR_FALSE disables mouse capture 
     *
     */
    NS_IMETHOD CaptureMouse(PRBool aCapture) = 0;

    /**
     * Classify the window for the window manager. Mostly for X11.
     */
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType) = 0;

    /**
     * Enables/Disables system capture of any and all events that would cause a
     * dropdown to be rolled up, This method ignores the aConsumeRollupEvent 
     * parameter when aDoCapture is FALSE
     * @param aDoCapture PR_TRUE enables capture, PR_FALSE disables capture 
     * @param aConsumeRollupEvent PR_TRUE consumes the rollup event, PR_FALSE dispatches rollup event
     *
     */
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent) = 0;

    /**
     *   Determine whether a given event should be processed assuming we are
     * the currently active modal window.
     *   Note that the exact semantics of this method are platform-dependent.
     * The Macintosh, for instance, cares deeply that this method do exactly
     * as advertised. Gtk, for instance, handles modality in a completely
     * different fashion and does little if anything with this method.
     * @param aRealEvent event is real or a null placeholder (Macintosh)
     * @param aEvent void pointer to native event structure
     * @param aForWindow return value. PR_TRUE iff event should be processed.
     */
    NS_IMETHOD ModalEventFilter(PRBool aRealEvent, void *aEvent, PRBool *aForWindow) = 0;

    /**
     * Bring this window to the user's attention.  This is intended to be a more
     * gentle notification than popping the window to the top or putting up an
     * alert.  See, for example, Win32 FlashWindow or the NotificationManager on
     * the Mac.  The notification should be suppressed if the window is already
     * in the foreground and should be dismissed when the user brings this window
     * to the foreground.
     * @param aCycleCount Maximum number of times to animate the window per system 
     *                    conventions. If set to -1, cycles indefinitely until 
     *                    window is brought into the foreground.
     */
    NS_IMETHOD GetAttention(PRInt32 aCycleCount) = 0;

    /**
     * Get the last user input event time in milliseconds. If there are any pending
     * native toolkit input events it returns the current time. All input events are 
     * included (ie. it is *not* limited to events targeted at this nsIWidget instance.
     *
     * @param aTime Last user input time in milliseconds. This value can be used to compare
     * durations but can not be used for determining wall clock time. The value returned 
     * is platform dependent, but is compatible with the expression 
     * PR_IntervalToMicroseconds(PR_IntervalNow()).
     */
    NS_IMETHOD GetLastInputEventTime(PRUint32& aTime) = 0;

#ifdef MOZ_CAIRO_GFX
    /**
     * Get the Thebes surface associated with this widget.
     */
    virtual gfxASurface *GetThebesSurface() = 0;
#endif

    /**
     * Set a flag that makes any window resizes use native window animation.
     * Ignored on any OS that doesn't support native animation.
     *
     * @param aAnimate Whether or not you want resizes to be animated.
     * @return NS_ERROR_NOT_IMPLEMENTED if not implemented on widget or platform
     */
    NS_IMETHOD SetAnimatedResize(PRUint16 aAnimation) = 0;

    /**
     * Get a flag that controls native window animation.
     * Ignored on any OS that doesn't support native animation.
     *
     * @param aAnimate Whether or not resizes are animated.
     * @return NS_ERROR_NOT_IMPLEMENTED if not implemented on widget or platform
     */
    NS_IMETHOD GetAnimatedResize(PRUint16* aAnimation) = 0;

protected:
    // keep the list of children.  We also keep track of our siblings.
    // The ownership model is as follows: parent holds a strong ref to
    // the first element of the list, and each element holds a strong
    // ref to the next element in the list.  The prevsibling and
    // lastchild pointers are weak, which is fine as long as they are
    // maintained properly.
    nsCOMPtr<nsIWidget> mFirstChild;
    nsIWidget* mLastChild;
    nsCOMPtr<nsIWidget> mNextSibling;
    nsIWidget* mPrevSibling;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWidget, NS_IWIDGET_IID)

#endif // nsIWidget_h__
