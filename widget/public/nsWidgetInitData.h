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

#ifndef nsWidgetInitData_h__
#define nsWidgetInitData_h__

#include "prtypes.h"

/**
 * Window types
 *
 * Don't alter previously encoded enum values - 3rd party apps may look at
 * these.
 */
enum nsWindowType {
  eWindowType_toplevel,  // default top level window
  eWindowType_dialog,    // top level window but usually handled differently
                         // by the OS
  eWindowType_popup,     // used for combo boxes, etc
  eWindowType_child,     // child windows (contained inside a window on the
                         // desktop (has no border))
  eWindowType_invisible, // windows that are invisible or offscreen
  eWindowType_plugin,    // plugin window
  eWindowType_sheet      // MacOSX sheet (special dialog class)
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
  ePopupTypeAny      = 0xF000 // used only to pass to
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
  // puts the popup at toplevel, but it will hide when the application is deactivated
  ePopupLevelFloating,
  // the popup appears on top of other windows, including those of other applications
  ePopupLevelTop
};

/**
 * Border styles
 */
enum nsBorderStyle {
  eBorderStyle_none     = 0,      // no border, titlebar, etc.. opposite of
                                  // all
  eBorderStyle_all      = 1 << 0, // all window decorations
  eBorderStyle_border   = 1 << 1, // enables the border on the window. these
                                  // are only for decoration and are not
                                  // resize handles
  eBorderStyle_resizeh  = 1 << 2, // enables the resize handles for the
                                  // window. if this is set, border is
                                  // implied to also be set
  eBorderStyle_title    = 1 << 3, // enables the titlebar for the window
  eBorderStyle_menu     = 1 << 4, // enables the window menu button on the
                                  // title bar. this being on should force
                                  // the title bar to display
  eBorderStyle_minimize = 1 << 5, // enables the minimize button so the user
                                  // can minimize the window. turned off for
                                  // tranient windows since they can not be
                                  // minimized separate from their parent
  eBorderStyle_maximize = 1 << 6, // enables the maxmize button so the user
                                  // can maximize the window
  eBorderStyle_close    = 1 << 7, // show the close button
  eBorderStyle_default  = -1      // whatever the OS wants... i.e. don't do
                                  // anything
};

/**
 * Basic struct for widget initialization data.
 * @see Create member function of nsIWidget
 */

struct nsWidgetInitData {
  nsWidgetInitData() :
      mWindowType(eWindowType_child),
      mBorderStyle(eBorderStyle_default),
      mPopupHint(ePopupTypePanel),
      mPopupLevel(ePopupLevelTop),
      clipChildren(PR_FALSE), 
      clipSiblings(PR_FALSE), 
      mDropShadow(PR_FALSE),
      mListenForResizes(PR_FALSE),
      mUnicode(PR_TRUE),
      mRTL(PR_FALSE),
      mNoAutoHide(PR_FALSE)
  {
  }

  nsWindowType  mWindowType;
  nsBorderStyle mBorderStyle;
  nsPopupType   mPopupHint;
  nsPopupLevel  mPopupLevel;
  // when painting exclude area occupied by child windows and sibling windows
  PRPackedBool  clipChildren, clipSiblings, mDropShadow;
  PRPackedBool  mListenForResizes;
  PRPackedBool  mUnicode;
  PRPackedBool  mRTL;
  PRPackedBool  mNoAutoHide; // true for noautohide panels
  PRPackedBool  mIsDragPopup;  // true for drag feedback panels
};

#endif // nsWidgetInitData_h__
