/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsWindow.h"

#ifndef WindowCE_h__
#define WindowCE_h__

/*
 * nsWindowCE - Windows CE specific code related to nsWindow. 
 */

#ifdef WINCE

#include "aygshell.h"
#include "imm.h"
#ifdef WINCE_WINDOWS_MOBILE
#define WINCE_HAVE_SOFTKB
#include "tpcshell.h"
#else
#undef WINCE_HAVE_SOFTKB
#include "winuserm.h"
#endif

#define IDI_APPLICATION         MAKEINTRESOURCE(32512)

#define SetWindowLongPtrA       SetWindowLongW
#define SetWindowLongPtrW       SetWindowLongW
#define GetWindowLongPtrW       GetWindowLongW
#define GWLP_WNDPROC            GWL_WNDPROC
#define GetPropW                GetProp
#define SetPropW                SetProp
#define RemovePropW             RemoveProp

#define MapVirtualKeyEx(a,b,c)  MapVirtualKey(a,b)

#ifndef WINCE_WINDOWS_MOBILE
// Aliases to make nsFilePicker work.
#define BROWSEINFOW             BROWSEINFO
#define BFFM_SETSELECTIONW      BFFM_SETSELECTION
#define SHBrowseForFolderW(a)   SHBrowseForFolder(a)
#endif

// No-ops for non-existent ce global apis.
inline void FlashWindow(HWND window, BOOL ignore){}
inline BOOL IsIconic(HWND inWnd){return false;}

class nsWindowCE {
public:
  static BOOL EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam);

#if defined(WINCE_HAVE_SOFTKB)
  static void ToggleSoftKB(HWND wnd, PRBool show);
  static void CreateSoftKeyMenuBar(HWND wnd);
  static void OnSoftKbSettingsChange(HWND wnd, LPRECT = NULL);
  static PRBool sSIPInTransition;
  static TriStateBool sShowSIPButton;
  static void CheckKeyboardStatus();
  static TriStateBool GetSliderStateOpen();
  static void ResetSoftKB(HWND wnd);
protected:
  static PRBool sMenuBarShown;
  static HWND sSoftKeyMenuBarHandle;
private:
  static TriStateBool sHardKBPresence;
  static RECT sDefaultSIPRect;
  static HWND sMainWindowHandle;
#endif
  friend class nsWindow;
};

#endif /* WINCE */

#endif /* WindowCE_h__ */
