/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsRegion.h"
#include "nsStringGlue.h"

#include "prthread.h"
#include "nsEvent.h"
#include "nsCOMPtr.h"
#include "nsITheme.h"
#include "nsNativeWidget.h"
#include "nsWidgetInitData.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "LayersTypes.h"

// forward declarations
class   nsFontMetrics;
class   nsRenderingContext;
class   nsDeviceContext;
struct  nsFont;
class   nsIRollupListener;
class   nsGUIEvent;
class   imgIContainer;
class   gfxASurface;
class   nsIContent;
class   ViewWrapper;

namespace mozilla {
namespace dom {
class TabChild;
}
namespace layers {
class LayerManager;
class PLayersChild;
}
}

/**
 * Callback function that processes events.
 *
 * The argument is actually a subtype (subclass) of nsEvent which carries
 * platform specific information about the event. Platform specific code
 * knows how to deal with it.
 *
 * The return value determines whether or not the default action should take
 * place.
 */
typedef nsEventStatus (* EVENT_CALLBACK)(nsGUIEvent *event);

/**
 * Flags for the getNativeData function.
 * See getNativeData()
 */
#define NS_NATIVE_WINDOW      0
#define NS_NATIVE_GRAPHIC     1
#define NS_NATIVE_TMP_WINDOW  2
#define NS_NATIVE_WIDGET      3
#define NS_NATIVE_DISPLAY     4
#define NS_NATIVE_REGION      5
#define NS_NATIVE_OFFSETX     6
#define NS_NATIVE_OFFSETY     7
#define NS_NATIVE_PLUGIN_PORT 8
#define NS_NATIVE_SCREEN      9
#define NS_NATIVE_SHELLWIDGET 10      // Get the shell GtkWidget
// Has to match to NPNVnetscapeWindow, and shareable across processes
// HWND on Windows and XID on X11
#define NS_NATIVE_SHAREABLE_WINDOW 11
#ifdef XP_MACOSX
#define NS_NATIVE_PLUGIN_PORT_QD    100
#define NS_NATIVE_PLUGIN_PORT_CG    101
#endif
#ifdef XP_WIN
#define NS_NATIVE_TSF_THREAD_MGR       100
#define NS_NATIVE_TSF_CATEGORY_MGR     101
#define NS_NATIVE_TSF_DISPLAY_ATTR_MGR 102
#endif

#define NS_IWIDGET_IID \
  { 0x91aafae4, 0xd814, 0x4803, \
    { 0x9a, 0xf5, 0xb0, 0x2f, 0x1b, 0x2c, 0xaf, 0x57 } }

/*
 * Window shadow styles
 * Also used for the -moz-window-shadow CSS property
 */

#define NS_STYLE_WINDOW_SHADOW_NONE             0
#define NS_STYLE_WINDOW_SHADOW_DEFAULT          1
#define NS_STYLE_WINDOW_SHADOW_MENU             2
#define NS_STYLE_WINDOW_SHADOW_TOOLTIP          3
#define NS_STYLE_WINDOW_SHADOW_SHEET            4

/**
 * nsIWidget::OnIMEFocusChange should be called during blur,
 * but other OnIME*Change methods should not be called
 */
#define NS_SUCCESS_IME_NO_UPDATES \
    NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_WIDGET, 1)

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
                eCursor_none,
                // This one better be the last one in this list.
                eCursorCount
                }; 

enum nsTopLevelWidgetZPlacement { // for PlaceBehind()
  eZPlacementBottom = 0,  // bottom of the window stack
  eZPlacementBelow,       // just below another widget
  eZPlacementTop          // top of the window stack
};

/**
 * Preference for receiving IME updates
 *
 * If mWantUpdates is true, PuppetWidget will forward
 * nsIWidget::OnIMETextChange and nsIWidget::OnIMESelectionChange to the chrome
 * process. This incurs overhead from observers and IPDL. If the IME
 * implementation on a particular platform doesn't care about OnIMETextChange
 * and OnIMESelectionChange from content processes, they should set
 * mWantUpdates to false to avoid these overheads.
 *
 * If mWantHints is true, PuppetWidget will forward the content of text fields
 * to the chrome process to be cached. This way we return the cached content
 * during query events. (see comments in bug 583976). This only makes sense
 * for IME implementations that do use query events, otherwise there's a
 * significant overhead. Platforms that don't use query events should set
 * mWantHints to false.
 */
struct nsIMEUpdatePreference {

  nsIMEUpdatePreference()
    : mWantUpdates(false), mWantHints(false)
  {
  }
  nsIMEUpdatePreference(bool aWantUpdates, bool aWantHints)
    : mWantUpdates(aWantUpdates), mWantHints(aWantHints)
  {
  }
  bool mWantUpdates;
  bool mWantHints;
};


/* 
 * Contains IMEStatus plus information about the current 
 * input context that the IME can use as hints if desired.
 */

namespace mozilla {
namespace widget {

struct IMEState {
  /**
   * IME enabled states, the mEnabled value of
   * SetInputContext()/GetInputContext() should be one value of following
   * values.
   *
   * WARNING: If you change these values, you also need to edit:
   *   nsIDOMWindowUtils.idl
   *   nsContentUtils::GetWidgetStatusFromIMEStatus
   */
  enum Enabled {
    /**
     * 'Disabled' means the user cannot use IME. So, the IME open state should
     * be 'closed' during 'disabled'.
     */
    DISABLED,
    /**
     * 'Enabled' means the user can use IME.
     */
    ENABLED,
    /**
     * 'Password' state is a special case for the password editors.
     * E.g., on mac, the password editors should disable the non-Roman
     * keyboard layouts at getting focus. Thus, the password editor may have
     * special rules on some platforms.
     */
    PASSWORD,
    /**
     * This state is used when a plugin is focused.
     * When a plug-in is focused content, we should send native events
     * directly. Because we don't process some native events, but they may
     * be needed by the plug-in.
     */
    PLUGIN
  };
  Enabled mEnabled;

  /**
   * IME open states the mOpen value of SetInputContext() should be one value of
   * OPEN, CLOSE or DONT_CHANGE_OPEN_STATE.  GetInputContext() should return
   * OPEN, CLOSE or OPEN_STATE_NOT_SUPPORTED.
   */
  enum Open {
    /**
     * 'Unsupported' means the platform cannot return actual IME open state.
     * This value is used only by GetInputContext().
     */
    OPEN_STATE_NOT_SUPPORTED,
    /**
     * 'Don't change' means the widget shouldn't change IME open state when
     * SetInputContext() is called.
     */
    DONT_CHANGE_OPEN_STATE = OPEN_STATE_NOT_SUPPORTED,
    /**
     * 'Open' means that IME should compose in its primary language (or latest
     * input mode except direct ASCII character input mode).  Even if IME is
     * opened by this value, users should be able to close IME by theirselves.
     * Web contents can specify this value by |ime-mode: active;|.
     */
    OPEN,
    /**
     * 'Closed' means that IME shouldn't handle key events (or should handle
     * as ASCII character inputs on mobile device).  Even if IME is closed by
     * this value, users should be able to open IME by theirselves.
     * Web contents can specify this value by |ime-mode: inactive;|.
     */
    CLOSED
  };
  Open mOpen;

  IMEState() : mEnabled(ENABLED), mOpen(DONT_CHANGE_OPEN_STATE) { }

  IMEState(Enabled aEnabled, Open aOpen = DONT_CHANGE_OPEN_STATE) :
    mEnabled(aEnabled), mOpen(aOpen)
  {
  }
};

struct InputContext {
  IMEState mIMEState;

  /* The type of the input if the input is a html input field */
  nsString mHTMLInputType;

  /* A hint for the action that is performed when the input is submitted */
  nsString mActionHint;
};

struct InputContextAction {
  /**
   * mCause indicates what action causes calling nsIWidget::SetInputContext().
   * It must be one of following values.
   */
  enum Cause {
    // The cause is unknown but originated from content. Focus might have been
    // changed by content script.
    CAUSE_UNKNOWN,
    // The cause is unknown but originated from chrome. Focus might have been
    // changed by chrome script.
    CAUSE_UNKNOWN_CHROME,
    // The cause is user's keyboard operation.
    CAUSE_KEY,
    // The cause is user's mouse operation.
    CAUSE_MOUSE
  };
  Cause mCause;

  /**
   * mFocusChange indicates what happened for focus.
   */
  enum FocusChange {
    FOCUS_NOT_CHANGED,
    // A content got focus.
    GOT_FOCUS,
    // Focused content lost focus.
    LOST_FOCUS,
    // Menu got pseudo focus that means focused content isn't changed but
    // keyboard events will be handled by menu.
    MENU_GOT_PSEUDO_FOCUS,
    // Menu lost pseudo focus that means focused content will handle keyboard
    // events.
    MENU_LOST_PSEUDO_FOCUS
  };
  FocusChange mFocusChange;

  bool ContentGotFocusByTrustedCause() const {
    return (mFocusChange == GOT_FOCUS &&
            mCause != CAUSE_UNKNOWN);
  }

  bool UserMightRequestOpenVKB() const {
    return (mFocusChange == FOCUS_NOT_CHANGED &&
            mCause == CAUSE_MOUSE);
  }

  InputContextAction() :
    mCause(CAUSE_UNKNOWN), mFocusChange(FOCUS_NOT_CHANGED)
  {
  }

  InputContextAction(Cause aCause,
                     FocusChange aFocusChange = FOCUS_NOT_CHANGED) :
    mCause(aCause), mFocusChange(aFocusChange)
  {
  }
};

/**
 * Size constraints for setting the minimum and maximum size of a widget.
 * Values are in device pixels.
 */
struct SizeConstraints {
  SizeConstraints()
    : mMaxSize(NS_MAXSIZE, NS_MAXSIZE)
  {
  }

  SizeConstraints(nsIntSize aMinSize,
                  nsIntSize aMaxSize)
  : mMinSize(aMinSize),
    mMaxSize(aMaxSize)
  {
  }

  nsIntSize mMinSize;
  nsIntSize mMaxSize;
};

} // namespace widget
} // namespace mozilla

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIWidget : public nsISupports {
  protected:
    typedef mozilla::dom::TabChild TabChild;

  public:
    typedef mozilla::layers::LayerManager LayerManager;
    typedef mozilla::layers::LayersBackend LayersBackend;
    typedef mozilla::layers::PLayersChild PLayersChild;
    typedef mozilla::widget::IMEState IMEState;
    typedef mozilla::widget::InputContext InputContext;
    typedef mozilla::widget::InputContextAction InputContextAction;
    typedef mozilla::widget::SizeConstraints SizeConstraints;

    // Used in UpdateThemeGeometries.
    struct ThemeGeometry {
      // The -moz-appearance value for the themed widget
      PRUint8 mWidgetType;
      // The device-pixel rect within the window for the themed widget
      nsIntRect mRect;

      ThemeGeometry(PRUint8 aWidgetType, const nsIntRect& aRect)
       : mWidgetType(aWidgetType)
       , mRect(aRect)
      { }
    };

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWIDGET_IID)

    nsIWidget()
      : mLastChild(nullptr)
      , mPrevSibling(nullptr)
    {}

        
    /**
     * Create and initialize a widget. 
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
     * In practice at least one of aParent and aNativeParent will be null. If
     * both are null the widget isn't parented (e.g. context menus or
     * independent top level windows).
     *
     * @param     aParent       parent nsIWidget
     * @param     aNativeParent native parent widget
     * @param     aRect         the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     * @param     aContext
     * @param     aInitData     data that is used for widget initialization
     *
     */
    NS_IMETHOD Create(nsIWidget        *aParent,
                      nsNativeWidget   aNativeParent,
                      const nsIntRect  &aRect,
                      EVENT_CALLBACK   aHandleEventFunction,
                      nsDeviceContext *aContext,
                      nsWidgetInitData *aInitData = nullptr) = 0;

    /**
     * Allocate, initialize, and return a widget that is a child of
     * |this|.  The returned widget (if nonnull) has gone through the
     * equivalent of CreateInstance(widgetCID) + Create(...).
     *
     * |CreateChild()| lets widget backends decide whether to parent
     * the new child widget to this, nonnatively parent it, or both.
     * This interface exists to support the PuppetWidget backend,
     * which is entirely non-native.  All other params are the same as
     * for |Create()|.
     *
     * |aForceUseIWidgetParent| forces |CreateChild()| to only use the
     * |nsIWidget*| this, not its native widget (if it exists), when
     * calling |Create()|.  This is a timid hack around poorly
     * understood code, and shouldn't be used in new code.
     */
    virtual already_AddRefed<nsIWidget>
    CreateChild(const nsIntRect  &aRect,
                EVENT_CALLBACK   aHandleEventFunction,
                nsDeviceContext  *aContext,
                nsWidgetInitData *aInitData = nullptr,
                bool             aForceUseIWidgetParent = false) = 0;

    /**
     * Set the event callback for a widget. If a device context is not
     * provided then the existing device context will remain, it will
     * not be nulled out.
     */
    NS_IMETHOD SetEventCallback(EVENT_CALLBACK aEventFunction,
                                nsDeviceContext *aContext) = 0;

    /**
     * Attach to a top level widget. 
     *
     * In cases where a top level chrome widget is being used as a content
     * container, attach a secondary event callback and update the device
     * context. The primary event callback will continue to be called, so the
     * owning base window will continue to function.
     *
     * aViewEventFunction Event callback that will receive mirrored
     *                    events.
     * aContext The new device context for the view
     */
    NS_IMETHOD AttachViewToTopLevel(EVENT_CALLBACK aViewEventFunction,
                                    nsDeviceContext *aContext) = 0;

    /**
     * Accessor functions to get and set secondary client data. Used by
     * nsIView in connection with AttachViewToTopLevel above.
     */
    NS_IMETHOD SetAttachedViewPtr(ViewWrapper* aViewWrapper) = 0;
    virtual ViewWrapper* GetAttachedViewPtr() = 0;

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
     * Change the widget's parent. Null parents are allowed.
     *
     * @param     aNewParent   new parent 
     */
    NS_IMETHOD SetParent(nsIWidget* aNewParent) = 0;

    NS_IMETHOD RegisterTouchWindow() = 0;
    NS_IMETHOD UnregisterTouchWindow() = 0;

    /**
     * Return the parent Widget of this Widget or nullptr if this is a 
     * top level window
     *
     * @return the parent widget or nullptr if it does not have a parent
     *
     */
    virtual nsIWidget* GetParent(void) = 0;

    /**
     * Return the top level Widget of this Widget
     *
     * @return the top level widget
     */
    virtual nsIWidget* GetTopLevelWidget() = 0;

    /**
     * Return the top (non-sheet) parent of this Widget if it's a sheet,
     * or nullptr if this isn't a sheet (or some other error occurred).
     * Sheets are only supported on some platforms (currently only OS X).
     *
     * @return the top (non-sheet) parent widget or nullptr
     *
     */
    virtual nsIWidget* GetSheetWindowParent(void) = 0;

    /**
     * Return the physical DPI of the screen containing the window ...
     * the number of device pixels per inch.
     */
    virtual float GetDPI() = 0;

    /**
     * Return the default scale factor for the window. This is the
     * default number of device pixels per CSS pixel to use. This should
     * depend on OS/platform settings such as the Mac's "UI scale factor"
     * or Windows' "font DPI".
     */
    virtual double GetDefaultScale() = 0;

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
     * @param aState true to show the Widget, false to hide it
     *
     */
    NS_IMETHOD Show(bool aState) = 0;

    /**
     * Make the window modal
     *
     */
    NS_IMETHOD SetModal(bool aModal) = 0;

    /**
     * Returns whether the window is visible
     *
     */
    virtual bool IsVisible() const = 0;

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
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 PRInt32 *aX,
                                 PRInt32 *aY) = 0;

    /**
     * Move this widget.
     *
     * Coordinates refer to the top-left of the widget.  For toplevel windows
     * with decorations, this is the top-left of the titlebar and frame .
     *
     * @param aX the new x position expressed in the parent's coordinate system
     * @param aY the new y position expressed in the parent's coordinate system
     *
     **/
    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY) = 0;

    /**
     * Reposition this widget so that the client area has the given offset.
     *
     * @param aX       the new x offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aY       the new y offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     *
     **/
    NS_IMETHOD MoveClient(PRInt32 aX, PRInt32 aY) = 0;

    /**
     * Resize this widget. Any size constraints set for the window by a
     * previous call to SetSizeConstraints will be applied.
     *
     * @param aWidth  the new width expressed in the parent's coordinate system
     * @param aHeight the new height expressed in the parent's coordinate system
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD Resize(PRInt32 aWidth,
                      PRInt32 aHeight,
                      bool     aRepaint) = 0;

    /**
     * Move or resize this widget. Any size constraints set for the window by
     * a previous call to SetSizeConstraints will be applied.
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
                      bool     aRepaint) = 0;

    /**
     * Resize the widget so that the inner client area has the given size.
     *
     * @param aWidth   the new width of the client area.
     * @param aHeight  the new height of the client area.
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD ResizeClient(PRInt32 aWidth,
                            PRInt32 aHeight,
                            bool  aRepaint) = 0;

    /**
     * Resize and reposition the widget so tht inner client area has the given
     * offset and size.
     *
     * @param aX       the new x offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aY       the new y offset of the client area expressed as an
     *                 offset from the origin of the client area of the parent
     *                 widget (for root widgets and popup widgets it is in
     *                 screen coordinates)
     * @param aWidth   the new width of the client area.
     * @param aHeight  the new height of the client area.
     * @param aRepaint whether the widget should be repainted
     *
     */
    NS_IMETHOD ResizeClient(PRInt32 aX,
                            PRInt32 aY,
                            PRInt32 aWidth,
                            PRInt32 aHeight,
                            bool    aRepaint) = 0;

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
                           nsIWidget *aWidget, bool aActivate) = 0;

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
     * @param aState true to enable the Widget, false to disable it.
     *
     */
    NS_IMETHOD Enable(bool aState) = 0;

    /**
     * Ask whether the widget is enabled
     */
    virtual bool IsEnabled() const = 0;

    /**
     * Request activation of this window or give focus to this widget.
     *
     * @param aRaise If true, this function requests activation of this
     *               widget's toplevel window.
     *               If false, the appropriate toplevel window (which in
     *               the case of popups may not be this widget's toplevel
     *               window) is already active, and this function indicates
     *               that keyboard events should be reported through the
     *               aHandleEventFunction provided to this->Create().
     */
    NS_IMETHOD SetFocus(bool aRaise = false) = 0;

    /**
     * Get this widget's outside dimensions relative to its parent widget. For
     * popup widgets the returned rect is in screen coordinates and not
     * relative to its parent widget.
     *
     * @param aRect   On return it holds the  x, y, width and height of
     *                this widget.
     */
    NS_IMETHOD GetBounds(nsIntRect &aRect) = 0;

    /**
     * Get this widget's outside dimensions in global coordinates. This
     * includes any title bar on the window.
     *
     * @param aRect   On return it holds the  x, y, width and height of
     *                this widget.
     */
    NS_IMETHOD GetScreenBounds(nsIntRect &aRect) = 0;

    /**
     * Get this widget's client area bounds, if the window has a 3D border
     * appearance this returns the area inside the border. The position is the
     * position of the client area relative to the client area of the parent
     * widget (for root widgets and popup widgets it is in screen coordinates).
     *
     * @param aRect   On return it holds the  x. y, width and height of
     *                the client area of this widget.
     */
    NS_IMETHOD GetClientBounds(nsIntRect &aRect) = 0;

    /**
     * Get the non-client area dimensions of the window.
     * 
     */
    NS_IMETHOD GetNonClientMargins(nsIntMargin &margins) = 0;

    /**
     * Sets the non-client area dimensions of the window. Pass -1 to restore
     * the system default frame size for that border. Pass zero to remove
     * a border, or pass a specific value adjust a border. Units are in
     * pixels. (DPI dependent)
     *
     * Platform notes:
     *  Windows: shrinking top non-client height will remove application
     *  icon and window title text. Glass desktops will refuse to set
     *  dimensions between zero and size < system default.
     *
     */
    NS_IMETHOD SetNonClientMargins(nsIntMargin &margins) = 0;

    /**
     * Get the client offset from the window origin.
     *
     * @return the x and y of the offset.
     *
     */
    virtual nsIntPoint GetClientOffset() = 0;

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
     * Set the transparency mode of the top-level window containing this widget.
     * So, e.g., if you call this on the widget for an IFRAME, the top level
     * browser window containing the IFRAME actually gets set. Be careful.
     *
     * This can fail if the platform doesn't support
     * transparency/glass. By default widgets are not
     * transparent.  This will also fail if the toplevel window is not
     * a Mozilla window, e.g., if the widget is in an embedded
     * context.
     *
     * After transparency/glass has been enabled, the initial alpha channel
     * value for all pixels is 1, i.e., opaque.
     * If the window is resized then the alpha channel values for
     * all pixels are reset to 1.
     * Pixel RGB color values are already premultiplied with alpha channel values.
     */
    virtual void SetTransparencyMode(nsTransparencyMode aMode) = 0;

    /**
     * Get the transparency mode of the top-level window that contains this
     * widget.
     */
    virtual nsTransparencyMode GetTransparencyMode() = 0;

    /**
     * This represents a command to set the bounds and clip region of
     * a child widget.
     */
    struct Configuration {
        nsIWidget* mChild;
        nsIntRect mBounds;
        nsTArray<nsIntRect> mClipRegion;
    };

    /**
     * Sets the clip region of each mChild (which must actually be a child
     * of this widget) to the union of the pixel rects given in
     * mClipRegion, all relative to the top-left of the child
     * widget. Clip regions are not implemented on all platforms and only
     * need to actually work for children that are plugins.
     * 
     * Also sets the bounds of each child to mBounds.
     * 
     * This will invalidate areas of the children that have changed, but
     * does not need to invalidate any part of this widget.
     * 
     * Children should be moved in the order given; the array is
     * sorted so to minimize unnecessary invalidation if children are
     * moved in that order.
     */
    virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) = 0;

    /**
     * Appends to aRects the rectangles constituting this widget's clip
     * region. If this widget is not clipped, appends a single rectangle
     * (0, 0, bounds.width, bounds.height).
     */
    virtual void GetWindowClipRegion(nsTArray<nsIntRect>* aRects) = 0;

    /**
     * Set the shadow style of the window.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    NS_IMETHOD SetWindowShadowStyle(PRInt32 aStyle) = 0;

    /*
     * On Mac OS X, this method shows or hides the pill button in the titlebar
     * that's used to collapse the toolbar.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    virtual void SetShowsToolbarButton(bool aShow) = 0;

    /*
     * On Mac OS X Lion, this method shows or hides the full screen button in
     * the titlebar that handles native full screen mode.
     *
     * Ignored on child widgets, non-Mac platforms, & pre-Lion Mac.
     */
    virtual void SetShowsFullScreenButton(bool aShow) = 0;

    enum WindowAnimationType {
      eGenericWindowAnimation,
      eDocumentWindowAnimation
    };

    /**
     * Sets the kind of top-level window animation this widget should have.  On
     * Mac OS X, this causes a particular kind of animation to be shown when the
     * window is first made visible.
     *
     * Ignored on child widgets and on non-Mac platforms.
     */
    virtual void SetWindowAnimationType(WindowAnimationType aType) = 0;

    /** 
     * Hide window chrome (borders, buttons) for this widget.
     *
     */
    NS_IMETHOD HideWindowChrome(bool aShouldHide) = 0;

    /**
     * Put the toplevel window into or out of fullscreen mode.
     *
     */
    NS_IMETHOD MakeFullScreen(bool aFullScreen) = 0;

    /**
     * Invalidate a specified rect for a widget so that it will be repainted
     * later.
     */
    NS_IMETHOD Invalidate(const nsIntRect & aRect) = 0;

    enum LayerManagerPersistence
    {
      LAYER_MANAGER_CURRENT = 0,
      LAYER_MANAGER_PERSISTENT
    };

    /**
     * Return the widget's LayerManager. The layer tree for that
     * LayerManager is what gets rendered to the widget.
     *
     * @param aAllowRetaining an outparam that states whether the returned
     * layer manager should be used for retained layers
     */
    inline LayerManager* GetLayerManager(bool* aAllowRetaining = nullptr)
    {
        return GetLayerManager(nullptr, mozilla::layers::LAYERS_NONE,
                               LAYER_MANAGER_CURRENT, aAllowRetaining);
    }

    inline LayerManager* GetLayerManager(LayerManagerPersistence aPersistence,
                                         bool* aAllowRetaining = nullptr)
    {
        return GetLayerManager(nullptr, mozilla::layers::LAYERS_NONE,
                               aPersistence, aAllowRetaining);
    }

    /**
     * Like GetLayerManager(), but prefers creating a layer manager of
     * type |aBackendHint| instead of what would normally be created.
     * LAYERS_NONE means "no hint".
     */
    virtual LayerManager* GetLayerManager(PLayersChild* aShadowManager,
                                          LayersBackend aBackendHint,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr) = 0;

    /**
     * Called before the LayerManager draws the layer tree.
     *
     * @param aManager The drawing LayerManager.
     * @param aWidgetRect The current widget rect that is being drawn.
     */
    virtual void DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect) = 0;

    /**
     * Called after the LayerManager draws the layer tree
     *
     * @param aManager The drawing LayerManager.
     * @param aRect Current widget rect that is being drawn.
     */
    virtual void DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect) = 0;

    /**
     * Called when Gecko knows which themed widgets exist in this window.
     * The passed array contains an entry for every themed widget of the right
     * type (currently only NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR and
     * NS_THEME_TOOLBAR) within the window, except for themed widgets which are
     * transformed or have effects applied to them (e.g. CSS opacity or
     * filters).
     * This could sometimes be called during display list construction
     * outside of painting.
     * If called during painting, it will be called before we actually
     * paint anything.
     */
    virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) = 0;

    /**
     * Informs the widget about the region of the window that is opaque.
     *
     * @param aOpaqueRegion the region of the window that is opaque.
     */
    virtual void UpdateOpaqueRegion(const nsIntRegion &aOpaqueRegion) {}

    /** 
     * Internal methods
     */

    //@{
    virtual void AddChild(nsIWidget* aChild) = 0;
    virtual void RemoveChild(nsIWidget* aChild) = 0;
    virtual void* GetNativeData(PRUint32 aDataType) = 0;
    virtual void FreeNativeData(void * data, PRUint32 aDataType) = 0;//~~~

    // GetDeviceContext returns a weak pointer to this widget's device context
    virtual nsDeviceContext* GetDeviceContext() = 0;

    //@}

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
     * Return this widget's origin in screen coordinates.
     *
     * @return screen coordinates stored in the x,y members
     */

    virtual nsIntPoint WidgetToScreenOffset() = 0;

    /**
     * Given the specified client size, return the corresponding window size,
     * which includes the area for the borders and titlebar. This method
     * should work even when the window is not yet visible.
     */
    virtual nsIntSize ClientToWindowSize(const nsIntSize& aClientSize) = 0;

    /**
     * Dispatches an event to the widget
     *
     */
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus) = 0;

    /**
     * Enables the dropping of files to a widget (XXX this is temporary)
     *
     */
    NS_IMETHOD EnableDragDrop(bool aEnable) = 0;
   
    /**
     * Enables/Disables system mouse capture.
     * @param aCapture true enables mouse capture, false disables mouse capture 
     *
     */
    NS_IMETHOD CaptureMouse(bool aCapture) = 0;

    /**
     * Classify the window for the window manager. Mostly for X11.
     */
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType) = 0;

    /**
     * Enables/Disables system capture of any and all events that would cause a
     * dropdown to be rolled up, This method ignores the aConsumeRollupEvent 
     * parameter when aDoCapture is FALSE
     * @param aDoCapture true enables capture, false disables capture 
     * @param aConsumeRollupEvent true consumes the rollup event, false dispatches rollup event
     *
     */
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener, bool aDoCapture,
                                   bool aConsumeRollupEvent) = 0;

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
     * Ask whether there user input events pending.  All input events are
     * included, including those not targeted at this nsIwidget instance.
     */
    virtual bool HasPendingInputEvent() = 0;

    /**
     * Called when when we need to begin secure keyboard input, such as when a password field
     * gets focus.
     *
     * NOTE: Calls to this method may not be nested and you can only enable secure keyboard input
     * for one widget at a time.
     */
    NS_IMETHOD BeginSecureKeyboardInput() = 0;

    /**
     * Called when when we need to end secure keyboard input, such as when a password field
     * loses focus.
     *
     * NOTE: Calls to this method may not be nested and you can only enable secure keyboard input
     * for one widget at a time.
     */
    NS_IMETHOD EndSecureKeyboardInput() = 0;

    /**
     * Set the background color of the window titlebar for this widget. On Mac,
     * for example, this will remove the grey gradient and bottom border and
     * instead show a single, solid color.
     *
     * Ignored on any platform that does not support it. Ignored by widgets that
     * do not represent windows.
     *
     * @param aColor  The color to set the title bar background to. Alpha values 
     *                other than fully transparent (0) are respected if possible  
     *                on the platform. An alpha of 0 will cause the window to 
     *                draw with the default style for the platform.
     *
     * @param aActive Whether the color should be applied to active or inactive
     *                windows.
     */
    NS_IMETHOD SetWindowTitlebarColor(nscolor aColor, bool aActive) = 0;

    /**
     * If set to true, the window will draw its contents into the titlebar
     * instead of below it.
     *
     * Ignored on any platform that does not support it. Ignored by widgets that
     * do not represent windows.
     * May result in a resize event, so should only be called from places where
     * reflow and painting is allowed.
     *
     * @param aState Whether drawing into the titlebar should be activated.
     */
    virtual void SetDrawsInTitlebar(bool aState) = 0;

    /*
     * Determine whether the widget shows a resize widget. If it does,
     * aResizerRect returns the resizer's rect.
     *
     * Returns false on any platform that does not support it.
     *
     * @param aResizerRect The resizer's rect in device pixels.
     * @return Whether a resize widget is shown.
     */
    virtual bool ShowsResizeIndicator(nsIntRect* aResizerRect) = 0;

    /**
     * Get the Thebes surface associated with this widget.
     */
    virtual gfxASurface *GetThebesSurface() = 0;

    /**
     * Return the popup that was last rolled up, or null if there isn't one.
     */
    virtual nsIContent* GetLastRollup() = 0;

    /**
     * Begin a window resizing drag, based on the event passed in.
     */
    NS_IMETHOD BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical) = 0;

    /**
     * Begin a window moving drag, based on the event passed in.
     */
    NS_IMETHOD BeginMoveDrag(nsMouseEvent* aEvent) = 0;

    enum Modifiers {
        CAPS_LOCK = 0x01, // when CapsLock is active
        NUM_LOCK = 0x02, // when NumLock is active
        SHIFT_L = 0x0100,
        SHIFT_R = 0x0200,
        CTRL_L = 0x0400,
        CTRL_R = 0x0800,
        ALT_L = 0x1000, // includes Option
        ALT_R = 0x2000,
        COMMAND_L = 0x4000,
        COMMAND_R = 0x8000,
        HELP = 0x10000,
        FUNCTION = 0x100000,
        NUMERIC_KEY_PAD = 0x01000000 // when the key is coming from the keypad
    };
    /**
     * Utility method intended for testing. Dispatches native key events
     * to this widget to simulate the press and release of a key.
     * @param aNativeKeyboardLayout a *platform-specific* constant.
     * On Mac, this is the resource ID for a 'uchr' or 'kchr' resource.
     * On Windows, it is converted to a hex string and passed to
     * LoadKeyboardLayout, see
     * http://msdn.microsoft.com/en-us/library/ms646305(VS.85).aspx
     * @param aNativeKeyCode a *platform-specific* keycode.
     * On Windows, this is the virtual key code.
     * @param aModifiers some combination of the above 'Modifiers' flags;
     * not all flags will apply to all platforms. Mac ignores the _R
     * modifiers. Windows ignores COMMAND, NUMERIC_KEY_PAD, HELP and
     * FUNCTION.
     * @param aCharacters characters that the OS would decide to generate
     * from the event. On Windows, this is the charCode passed by
     * WM_CHAR.
     * @param aUnmodifiedCharacters characters that the OS would decide
     * to generate from the event if modifier keys (other than shift)
     * were assumed inactive. Needed on Mac, ignored on Windows.
     * @return NS_ERROR_NOT_AVAILABLE to indicate that the keyboard
     * layout is not supported and the event was not fired
     */
    virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                              PRInt32 aNativeKeyCode,
                                              PRUint32 aModifierFlags,
                                              const nsAString& aCharacters,
                                              const nsAString& aUnmodifiedCharacters) = 0;

    /**
     * Utility method intended for testing. Dispatches native mouse events
     * may even move the mouse cursor. On Mac the events are guaranteed to
     * be sent to the window containing this widget, but on Windows they'll go
     * to whatever's topmost on the screen at that position, so for
     * cross-platform testing ensure that your window is at the top of the
     * z-order.
     * @param aPoint screen location of the mouse, in device
     * pixels, with origin at the top left
     * @param aNativeMessage *platform-specific* event type (e.g. on Mac,
     * NSMouseMoved; on Windows, MOUSEEVENTF_MOVE, MOUSEEVENTF_LEFTDOWN etc)
     * @param aModifierFlags *platform-specific* modifier flags (ignored
     * on Windows)
     */
    virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                PRUint32 aNativeMessage,
                                                PRUint32 aModifierFlags) = 0;

    /**
     * A shortcut to SynthesizeNativeMouseEvent, abstracting away the native message.
     */
    virtual nsresult SynthesizeNativeMouseMove(nsIntPoint aPoint) = 0;

    /**
     * Utility method intended for testing. Dispatching native mouse scroll
     * events may move the mouse cursor.
     *
     * @param aPoint            Mouse cursor position in screen coordinates.
     *                          In device pixels, the origin at the top left of
     *                          the primary display.
     * @param aNativeMessage    Platform native message.
     * @param aDeltaX           The delta value for X direction.  If the native
     *                          message doesn't indicate X direction scrolling,
     *                          this may be ignored.
     * @param aDeltaY           The delta value for Y direction.  If the native
     *                          message doesn't indicate Y direction scrolling,
     *                          this may be ignored.
     * @param aDeltaZ           The delta value for Z direction.  If the native
     *                          message doesn't indicate Z direction scrolling,
     *                          this may be ignored.
     * @param aModifierFlags    Must be values of Modifiers, or zero.
     * @param aAdditionalFlags  See nsIDOMWidnowUtils' consts and their
     *                          document.
     */
    virtual nsresult SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                                      PRUint32 aNativeMessage,
                                                      double aDeltaX,
                                                      double aDeltaY,
                                                      double aDeltaZ,
                                                      PRUint32 aModifierFlags,
                                                      PRUint32 aAdditionalFlags) = 0;

    /**
     * Activates a native menu item at the position specified by the index
     * string. The index string is a string of positive integers separated
     * by the "|" (pipe) character. The last integer in the string represents
     * the item index in a submenu located using the integers preceding it.
     *
     * Example: 1|0|4
     * In this string, the first integer represents the top-level submenu
     * in the native menu bar. Since the integer is 1, it is the second submeu
     * in the native menu bar. Within that, the first item (index 0) is a
     * submenu, and we want to activate the 5th item within that submenu.
     */
    virtual nsresult ActivateNativeMenuItemAt(const nsAString& indexString) = 0;

    /**
     * This is used for native menu system testing.
     *
     * Updates a native menu at the position specified by the index string.
     * The index string is a string of positive integers separated by the "|" 
     * (pipe) character.
     *
     * Example: 1|0|4
     * In this string, the first integer represents the top-level submenu
     * in the native menu bar. Since the integer is 1, it is the second submeu
     * in the native menu bar. Within that, the first item (index 0) is a
     * submenu, and we want to update submenu at index 4 within that submenu.
     *
     * If this is called with an empty string it forces a full reload of the
     * menu system.
     */
    virtual nsresult ForceUpdateNativeMenuAt(const nsAString& indexString) = 0;

    /*
     * Force Input Method Editor to commit the uncommitted input
     */
    NS_IMETHOD ResetInputState()=0;

    /*
     * Following methods relates to IME 'Opened'/'Closed' state.
     * 'Opened' means the user can input any character. I.e., users can input Japanese  
     * and other characters. The user can change the state to 'Closed'.
     * 'Closed' means the user can input ASCII characters only. This is the same as a
     * non-IME environment. The user can change the state to 'Opened'.
     * For more information is here.
     * http://bugzilla.mozilla.org/show_bug.cgi?id=16940#c48
     */

    /*
     * Destruct and don't commit the IME composition string.
     */
    NS_IMETHOD CancelIMEComposition() = 0;

    /*
     * Notifies the input context changes.
     */
    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction) = 0;

    /*
     * Get current input context.
     */
    NS_IMETHOD_(InputContext) GetInputContext() = 0;

    /**
     * Set accelerated rendering to 'True' or 'False'
     */
    NS_IMETHOD SetAcceleratedRendering(bool aEnabled) = 0;

    /*
     * Get toggled key states.
     * aKeyCode should be NS_VK_CAPS_LOCK or  NS_VK_NUM_LOCK or
     * NS_VK_SCROLL_LOCK.
     * aLEDState is the result for current LED state of the key.
     * If the LED is 'ON', it returns TRUE, otherwise, FALSE.
     * If the platform doesn't support the LED state (or we cannot get the
     * state), this method returns NS_ERROR_NOT_IMPLEMENTED.
     */
    NS_IMETHOD GetToggledKeyState(PRUint32 aKeyCode, bool* aLEDState) = 0;

    /*
     * An editable node (i.e. input/textarea/design mode document)
     *  is receiving or giving up focus
     * aFocus is true if node is receiving focus
     * aFocus is false if node is giving up focus (blur)
     *
     * If this returns NS_ERROR_*, OnIMETextChange and OnIMESelectionChange
     * and OnIMEFocusChange(false) will be never called.
     *
     * If this returns NS_SUCCESS_IME_NO_UPDATES, OnIMEFocusChange(false)
     * will be called but OnIMETextChange and OnIMESelectionChange will NOT.
     */
    NS_IMETHOD OnIMEFocusChange(bool aFocus) = 0;

    /*
     * Text content of the focused node has changed
     * aStart is the starting offset of the change
     * aOldEnd is the ending offset of the change
     * aNewEnd is the caret offset after the change
     */
    NS_IMETHOD OnIMETextChange(PRUint32 aStart,
                               PRUint32 aOldEnd,
                               PRUint32 aNewEnd) = 0;

    /*
     * Selection has changed in the focused node
     */
    NS_IMETHOD OnIMESelectionChange(void) = 0;

    /*
     * Retrieves preference for IME updates
     */
    virtual nsIMEUpdatePreference GetIMEUpdatePreference() = 0;

    /*
     * Call this method when a dialog is opened which has a default button.
     * The button's rectangle should be supplied in aButtonRect.
     */ 
    NS_IMETHOD OnDefaultButtonLoaded(const nsIntRect &aButtonRect) = 0;

    /**
     * Compute the overridden system mouse scroll speed on the root content of
     * web pages.  The widget may set the same value as aOriginalDelta.  E.g.,
     * when the system scrolling settings were customized, widget can respect
     * the will of the user.
     *
     * This is called only when the mouse wheel event scrolls the root content
     * of the web pages by line.  In other words, this isn't called when the
     * mouse wheel event is used for zoom, page scroll and other special
     * actions.  And also this isn't called when the user doesn't use the
     * system wheel speed settings.
     *
     * @param aOriginalDelta   The delta value of the current mouse wheel
     *                         scrolling event.
     * @param aIsHorizontal    If TRUE, the scrolling direction is horizontal.
     *                         Otherwise, it's vertical.
     * @param aOverriddenDelta The overridden mouse scrolling speed.  This value
     *                         may be same as aOriginalDelta.
     */
    NS_IMETHOD OverrideSystemMouseScrollSpeed(PRInt32 aOriginalDelta,
                                              bool aIsHorizontal,
                                              PRInt32 &aOverriddenDelta) = 0;

    /**
     * Return true if this process shouldn't use platform widgets, and
     * so should use PuppetWidgets instead.  If this returns true, the
     * result of creating and using a platform widget is undefined,
     * and likely to end in crashes or other buggy behavior.
     */
    static bool
    UsePuppetWidgets()
    {
      return XRE_GetProcessType() == GeckoProcessType_Content;
    }

    /**
     * Allocate and return a "puppet widget" that doesn't directly
     * correlate to a platform widget; platform events and data must
     * be fed to it.  Currently used in content processes.  NULL is
     * returned if puppet widgets aren't supported in this build
     * config, on this platform, or for this process type.
     *
     * This function is called "Create" to match CreateInstance().
     * The returned widget must still be nsIWidget::Create()d.
     */
    static already_AddRefed<nsIWidget>
    CreatePuppetWidget(TabChild* aTabChild);

    /**
     * Reparent this widget's native widget.
     * @param aNewParent the native widget of aNewParent is the new native
     *                   parent widget
     */
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent) = 0;

    /**
     * Return the internal format of the default framebuffer for this
     * widget.
     */
    virtual PRUint32 GetGLFrameBufferFormat() { return 0; /*GL_NONE*/ }

    /**
     * Return true if widget has it's own GL context
     */
    virtual bool HasGLContext() { return false; }

    /**
     * Returns true to indicate that this widget paints an opaque background
     * that we want to be visible under the page, so layout should not force
     * a default background.
     */
    virtual bool WidgetPaintsBackground() { return false; }

    /**
     * Get the natural bounds of this widget.  This method is only
     * meaningful for widgets for which Gecko implements screen
     * rotation natively.  When this is the case, GetBounds() returns
     * the widget bounds taking rotation into account, and
     * GetNaturalBounds() returns the bounds *not* taking rotation
     * into account.
     *
     * No code outside of the composition pipeline should know or care
     * about this.  If you're not an agent of the compositor, you
     * probably shouldn't call this method.
     */
    virtual nsIntRect GetNaturalBounds() {
        nsIntRect bounds;
        GetBounds(bounds);
        return bounds;
    }

    /**
     * Set size constraints on the window size such that it is never less than
     * the specified minimum size and never larger than the specified maximum
     * size. The size constraints are sizes of the outer rectangle including
     * the window frame and title bar. Use 0 for an unconstrained minimum size
     * and NS_MAXSIZE for an unconstrained maximum size. Note that this method
     * does not necessarily change the size of a window to conform to this size,
     * thus Resize should be called afterwards.
     *
     * @param aConstraints: the size constraints in device pixels
     */
    virtual void SetSizeConstraints(const SizeConstraints& aConstraints) = 0;

    /**
     * Return the size constraints currently observed by the widget.
     *
     * @return the constraints in device pixels
     */
    virtual const SizeConstraints& GetSizeConstraints() const = 0;

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
