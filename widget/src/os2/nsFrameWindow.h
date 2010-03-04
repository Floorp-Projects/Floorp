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
 * nsFrameWindow is a subclass of nsWindow and is created when NS_WINDOW_CID
 * is specified.  It represents a top-level widget and is implemented as two
 * native windows, a frame (WC_FRAME) and a client (MozillaWindowClass).
 *
 * Most methods inherited from nsIWidget are handled by nsWindow.  For those
 * which require slightly different implementations for top-level and child
 * widgets, nsWindow relies on a flag or on virtual helper methods to handle
 * the differences properly.
 * 
 * There are two items where these differences are particularly important:
 * - mWnd identifies the frame's client which is seldom acted upon directly;
 *   instead, most operations involve mFrameWnd.
 * - mBounds contains the dimensions of mFrameWnd, not mWnd whose width and
 *   height are stored in mSizeClient.
 *
 */
//=============================================================================

#ifndef _nsframewindow_h
#define _nsframewindow_h

#include "nsWindow.h"
#include "nssize.h"

//=============================================================================
//  nsFrameWindow
//=============================================================================

class nsFrameWindow : public nsWindow
{
public:
  nsFrameWindow();
  virtual ~nsFrameWindow();

  // from nsIWidget
  virtual nsresult      CreateWindow(nsWindow* aParent,
                                     HWND aParentWnd,
                                     const nsIntRect& aRect,
                                     PRUint32 aStyle);
  NS_IMETHOD            Show(PRBool aState);
  NS_IMETHOD            GetClientBounds(nsIntRect& aRect);

protected:
  // from nsWindow
  virtual HWND          GetMainWindow()     const {return mFrameWnd;}
  virtual void          ActivateTopLevelWidget();
  virtual PRBool        OnReposition(PSWP pSwp);
  virtual PRInt32       GetClientHeight()   {return mSizeClient.height;}

  // nsFrameWindow
  PRUint32              GetFCFlags();
  void                  UpdateClientSize();
  void                  SetWindowListVisibility(PRBool aState);
  MRESULT               FrameMessage(ULONG msg, MPARAM mp1, MPARAM mp2);

  friend MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);

  PFNWP         mPrevFrameProc;
  nsSize        mSizeClient;
  PRBool        mNeedActivation;
};

#endif //_nsframewindow_h

//=============================================================================

