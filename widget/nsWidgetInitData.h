/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWidgetInitData_h__
#define nsWidgetInitData_h__

/**
 * Window types
 *
 * Don't alter previously encoded enum values - 3rd party apps may look at
 * these.
 */
enum nsWindowType {
  eWindowType_toplevel,   // default top level window
  eWindowType_dialog,     // top level window but usually handled differently
                          // by the OS
  eWindowType_sheet,      // MacOSX sheet (special dialog class)
  eWindowType_popup,      // used for combo boxes, etc
  eWindowType_child,      // child windows (contained inside a window on the
                          // desktop (has no border))
  eWindowType_invisible,  // windows that are invisible or offscreen
};

/**
 * Popup types
 *
 * For eWindowType_popup
 */
enum nsPopupType {
  ePopupTypePanel,
  ePopupTypeMenu,
  ePopupTypeTooltip,
  ePopupTypeAny = 0xF000  // used only to pass to
                          // nsXULPopupManager::GetTopPopup
};

/**
 * Popup levels specify the window ordering behaviour.
 */
enum nsPopupLevel {
  // the popup appears just above its parent and maintains its position
  // relative to the parent
  ePopupLevelParent,
  // the popup is a floating popup used for tool palettes. A parent window
  // must be specified, but a platform implementation need not use this.
  // On Windows, floating is generally equivalent to parent. On Mac, floating
  // puts the popup at toplevel, but it will hide when the application is
  // deactivated
  ePopupLevelFloating,
  // the popup appears on top of other windows, including those of other
  // applications
  ePopupLevelTop
};

/**
 * Border styles
 */
enum nsBorderStyle {
  eBorderStyle_none = 0,           // no border, titlebar, etc.. opposite of
                                   // all
  eBorderStyle_all = 1 << 0,       // all window decorations
  eBorderStyle_border = 1 << 1,    // enables the border on the window. these
                                   // are only for decoration and are not
                                   // resize handles
  eBorderStyle_resizeh = 1 << 2,   // enables the resize handles for the
                                   // window. if this is set, border is
                                   // implied to also be set
  eBorderStyle_title = 1 << 3,     // enables the titlebar for the window
  eBorderStyle_menu = 1 << 4,      // enables the window menu button on the
                                   // title bar. this being on should force
                                   // the title bar to display
  eBorderStyle_minimize = 1 << 5,  // enables the minimize button so the user
                                   // can minimize the window. turned off for
                                   // tranient windows since they can not be
                                   // minimized separate from their parent
  eBorderStyle_maximize = 1 << 6,  // enables the maxmize button so the user
                                   // can maximize the window
  eBorderStyle_close = 1 << 7,     // show the close button
  eBorderStyle_default = -1        // whatever the OS wants... i.e. don't do
                                   // anything
};

/**
 * Basic struct for widget initialization data.
 * @see Create member function of nsIWidget
 */

struct nsWidgetInitData {
  nsWidgetInitData()
      : mWindowType(eWindowType_child),
        mBorderStyle(eBorderStyle_default),
        mPopupHint(ePopupTypePanel),
        mPopupLevel(ePopupLevelTop),
        mScreenId(0),
        clipChildren(false),
        clipSiblings(false),
        mRTL(false),
        mNoAutoHide(false),
        mIsDragPopup(false),
        mIsAnimationSuppressed(false),
        mSupportTranslucency(false),
        mMouseTransparent(false),
        mHasRemoteContent(false),
        mAlwaysOnTop(false),
        mPIPWindow(false),
        mFissionWindow(false),
        mResizable(false),
        mIsPrivate(false) {}

  nsWindowType mWindowType;
  nsBorderStyle mBorderStyle;
  nsPopupType mPopupHint;
  nsPopupLevel mPopupLevel;
  // B2G multi-screen support. Screen ID is for differentiating screens of
  // windows, and due to the hardware limitation, it is platform-specific for
  // now, which align with the value of display type defined in HWC.
  uint32_t mScreenId;
  // when painting exclude area occupied by child windows and sibling windows
  bool clipChildren, clipSiblings;
  bool mRTL;
  bool mNoAutoHide;   // true for noautohide panels
  bool mIsDragPopup;  // true for drag feedback panels
  // true if window creation animation is suppressed, e.g. for session restore
  bool mIsAnimationSuppressed;
  // true if the window should support an alpha channel, if available.
  bool mSupportTranslucency;
  // true if the window should be transparent to mouse events. Currently this is
  // only valid for eWindowType_popup widgets
  bool mMouseTransparent;
  bool mHasRemoteContent;
  bool mAlwaysOnTop;
  // Is PictureInPicture window
  bool mPIPWindow;
  // True if fission is enabled for this window
  bool mFissionWindow;
  // True if the window is user-resizable.
  bool mResizable;
  bool mIsPrivate;
};

#endif  // nsWidgetInitData_h__
