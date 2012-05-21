/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

