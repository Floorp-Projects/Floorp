/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rich Walsh <dragtext@e-vertise.com>
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

//=============================================================================
/*
 * os2FrameWindow is a helper class instantiated by nsWindow to handle
 * operations that are specific to OS/2 frame windows.  It never references
 * the frame's client window except to create it.  See nsWindow.h for details.
 * Note: nsWindow.h must be #included in *.cpp before #including this file.
 * 
 */
//=============================================================================

#ifndef _os2framewindow_h
#define _os2framewindow_h

//=============================================================================
//  os2FrameWindow
//=============================================================================

class os2FrameWindow
{
public:
  os2FrameWindow(nsWindow* aOwner);
  ~os2FrameWindow();

  HWND                  CreateFrameWindow(nsWindow* aParent,
                                          HWND aParentWnd,
                                          const nsIntRect& aRect,
                                          nsWindowType aWindowType,
                                          nsBorderStyle aBorderStyle);
  PRUint32              GetFCFlags(nsWindowType aWindowType,
                                   nsBorderStyle aBorderStyle);
  nsresult              Show(bool aState);
  void                  SetWindowListVisibility(bool aState);
  nsresult              GetBounds(nsIntRect& aRect);
  nsresult              Move(PRInt32 aX, PRInt32 aY);
  nsresult              Resize(PRInt32 aWidth, PRInt32 aHeight,
                               bool aRepaint);
  nsresult              Resize(PRInt32 aX, PRInt32 aY, PRInt32 w, PRInt32 h,
                               bool aRepaint);
  void                  ActivateTopLevelWidget();
  nsresult              SetSizeMode(PRInt32 aMode);
  nsresult              HideWindowChrome(bool aShouldHide);
  nsresult              SetTitle(const nsAString& aTitle); 
  nsresult              SetIcon(const nsAString& aIconSpec); 
  nsresult              ConstrainPosition(bool aAllowSlop,
                                          PRInt32* aX, PRInt32* aY);
  MRESULT               ProcessFrameMessage(ULONG msg, MPARAM mp1, MPARAM mp2);
  HWND                  GetFrameWnd()       {return mFrameWnd;}

  friend MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);

protected:
  nsWindow *    mOwner;             // the nsWindow that created this instance
  HWND          mFrameWnd;          // the frame's window handle
  HWND          mTitleBar;          // the frame controls that have
  HWND          mSysMenu;           //   to be hidden or shown when
  HWND          mMinMax;            //   HideWindowChrome() is called
  PRUint32      mSavedStyle;        // frame style saved by HideWindowChrome()
  HPOINTER      mFrameIcon;         // current frame icon
  bool          mChromeHidden;      // are frame controls hidden?
  bool          mNeedActivation;    // triggers activation when focus changes
  PFNWP         mPrevFrameProc;     // the frame's original wndproc
  nsIntRect     mFrameBounds;       // the frame's location & dimensions
};

#endif //_os2framewindow_h

//=============================================================================

