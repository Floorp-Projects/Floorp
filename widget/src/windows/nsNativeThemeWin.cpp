/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * David Hyatt (hyatt@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Hill (tim@prismelite.com)
 *   James Ross (silver@warwickcompsoc.co.uk)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <windows.h>
#include "nsNativeThemeWin.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsILookAndFeel.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIMenuFrame.h"
#include "nsIMenuParent.h"
#include "nsWidgetAtoms.h"
#include <malloc.h>

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxWindowsSurface.h"
#endif
/* 
 * The following constants are used to determine how a widget is drawn using
 * Windows' Theme API. For more information on theme parts and states see
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/topics/partsandstates.asp
 */
#define THEME_COLOR 204
#define THEME_FONT  210

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4
#define TS_FOCUSED   5

// These constants are reversed for the trackbar (scale) thumb
#define TKP_FOCUSED   4
#define TKP_DISABLED  5

// Toolbarbutton constants
#define TB_CHECKED       5
#define TB_HOVER_CHECKED 6

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

// Treeview/listbox constants
#define TREEVIEW_BODY 1

// Scrollbar constants
#define SP_BUTTON          1
#define SP_THUMBHOR        2
#define SP_THUMBVERT       3
#define SP_TRACKSTARTHOR   4
#define SP_TRACKENDHOR     5
#define SP_TRACKSTARTVERT  6
#define SP_TRACKENDVERT    7
#define SP_GRIPPERHOR      8
#define SP_GRIPPERVERT     9

// Scale constants
#define TKP_TRACK          1
#define TKP_TRACKVERT      2
#define TKP_THUMB          3
#define TKP_THUMBVERT      6

// Spin constants
#define SPNP_UP            1
#define SPNP_DOWN          2

// Progress bar constants
#define PP_BAR             1
#define PP_BARVERT         2
#define PP_CHUNK           3
#define PP_CHUNKVERT       4

// Tab constants
#define TABP_TAB             4
#define TABP_TAB_SELECTED    5
#define TABP_PANELS          9
#define TABP_PANEL           10

// Tooltip constants
#define TTP_STANDARD         1

// Dropdown constants
#define CBP_DROPMARKER       1

// Constants only found in new (98+, 2K+, XP+, etc.) Windows.
#ifdef DFCS_HOT
#undef DFCS_HOT
#endif
#define DFCS_HOT             0x00001000

#ifdef COLOR_MENUHILIGHT
#undef COLOR_MENUHILIGHT
#endif
#define COLOR_MENUHILIGHT    29

#ifdef SPI_GETFLATMENU
#undef SPI_GETFLATMENU
#endif
#define SPI_GETFLATMENU      0x1022

// Our extra constants for passing a little but more info to the renderer.
#define DFCS_RTL             0x00010000
#define DFCS_CONTAINER       0x00020000

NS_IMPL_ISUPPORTS1(nsNativeThemeWin, nsITheme)

typedef HANDLE (WINAPI*OpenThemeDataPtr)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI*CloseThemeDataPtr)(HANDLE hTheme);
typedef HRESULT (WINAPI*DrawThemeBackgroundPtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*DrawThemeEdgePtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          uint uEdge, uint uFlags,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*GetThemeContentRectPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                          int iStateId, const RECT* pRect,
                                          RECT* pContentRect);
typedef HRESULT (WINAPI*GetThemePartSizePtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                       int iStateId, RECT* prc, int ts,
                                       SIZE* psz);
typedef HRESULT (WINAPI*GetThemeSysFontPtr)(HANDLE hTheme, int iFontId, OUT LOGFONT* pFont);
typedef HRESULT (WINAPI*GetThemeColorPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                   int iStateId, int iPropId, OUT COLORREF* pFont);

static OpenThemeDataPtr openTheme = NULL;
static CloseThemeDataPtr closeTheme = NULL;
static DrawThemeBackgroundPtr drawThemeBG = NULL;
static DrawThemeEdgePtr drawThemeEdge = NULL;
static GetThemeContentRectPtr getThemeContentRect = NULL;
static GetThemePartSizePtr getThemePartSize = NULL;
static GetThemeSysFontPtr getThemeSysFont = NULL;
static GetThemeColorPtr getThemeColor = NULL;

static const char kThemeLibraryName[] = "uxtheme.dll";

nsNativeThemeWin::nsNativeThemeWin() {
  mThemeDLL = NULL;
  mButtonTheme = NULL;
  mTextFieldTheme = NULL;
  mTooltipTheme = NULL;
  mToolbarTheme = NULL;
  mRebarTheme = NULL;
  mProgressTheme = NULL;
  mScrollbarTheme = NULL;
  mSpinTheme = NULL;
  mScaleTheme = NULL;
  mStatusbarTheme = NULL;
  mTabTheme = NULL;
  mTreeViewTheme = NULL;
  mComboBoxTheme = NULL;
  mHeaderTheme = NULL;

  mThemeDLL = ::LoadLibrary(kThemeLibraryName);
  if (mThemeDLL) {
    openTheme = (OpenThemeDataPtr)GetProcAddress(mThemeDLL, "OpenThemeData");
    closeTheme = (CloseThemeDataPtr)GetProcAddress(mThemeDLL, "CloseThemeData");
    drawThemeBG = (DrawThemeBackgroundPtr)GetProcAddress(mThemeDLL, "DrawThemeBackground");
    drawThemeEdge = (DrawThemeEdgePtr)GetProcAddress(mThemeDLL, "DrawThemeEdge");
    getThemeContentRect = (GetThemeContentRectPtr)GetProcAddress(mThemeDLL, "GetThemeBackgroundContentRect");
    getThemePartSize = (GetThemePartSizePtr)GetProcAddress(mThemeDLL, "GetThemePartSize");
    getThemeSysFont = (GetThemeSysFontPtr)GetProcAddress(mThemeDLL, "GetThemeSysFont");
    getThemeColor = (GetThemeColorPtr)GetProcAddress(mThemeDLL, "GetThemeColor");
  }

  UpdateConfig();

  // If there is a relevant change in forms.css for windows platform,
  // static widget style variables (e.g. sButtonBorderSize) should be 
  // reinitialized here.
}

nsNativeThemeWin::~nsNativeThemeWin() {
  if (!mThemeDLL)
    return;

  CloseData();
  
  if (mThemeDLL)
    ::FreeLibrary(mThemeDLL);
}

static void GetNativeRect(const nsRect& aSrc, RECT& aDst) 
{
  aDst.top = aSrc.y;
  aDst.bottom = aSrc.y + aSrc.height;
  aDst.left = aSrc.x;
  aDst.right = aSrc.x + aSrc.width;
}

void
nsNativeThemeWin::UpdateConfig()
{
  // On Windows 2000 this SystemParametersInfo call will fail
  //   and we get non-flat as desired.
  BOOL useFlat = PR_FALSE;
  mFlatMenus = ::SystemParametersInfo(SPI_GETFLATMENU, 0, &useFlat, 0) ?
                   useFlat : PR_FALSE;
}

HANDLE
nsNativeThemeWin::GetTheme(PRUint8 aWidgetType)
{ 
  if (!mThemeDLL)
    return NULL;

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
      if (!mButtonTheme)
        mButtonTheme = openTheme(NULL, L"Button");
      return mButtonTheme;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN: {
      if (!mTextFieldTheme)
        mTextFieldTheme = openTheme(NULL, L"Edit");
      return mTextFieldTheme;
    }
    case NS_THEME_TOOLTIP: {
      if (!mTooltipTheme)
        mTooltipTheme = openTheme(NULL, L"Tooltip");
      return mTooltipTheme;
    }
    case NS_THEME_TOOLBOX: {
      if (!mRebarTheme)
        mRebarTheme = openTheme(NULL, L"Rebar");
      return mRebarTheme;
    }
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON: {
      if (!mToolbarTheme)
        mToolbarTheme = openTheme(NULL, L"Toolbar");
      return mToolbarTheme;
    }
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL: {
      if (!mProgressTheme)
        mProgressTheme = openTheme(NULL, L"Progress");
      return mProgressTheme;
    }
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS: {
      if (!mTabTheme)
        mTabTheme = openTheme(NULL, L"Tab");
      return mTabTheme;
    }
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    {
      if (!mScrollbarTheme)
        mScrollbarTheme = openTheme(NULL, L"Scrollbar");
      return mScrollbarTheme;
    }
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    {
      if (!mScaleTheme)
        mScaleTheme = openTheme(NULL, L"Trackbar");
      return mScaleTheme;
    }
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    {
      if (!mSpinTheme)
        mSpinTheme = openTheme(NULL, L"Spin");
      return mSpinTheme;
    }
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
    {
      if (!mStatusbarTheme)
        mStatusbarTheme = openTheme(NULL, L"Status");
      return mStatusbarTheme;
    }
    case NS_THEME_DROPDOWN_BUTTON: {
      if (!mComboBoxTheme)
        mComboBoxTheme = openTheme(NULL, L"Combobox");
      return mComboBoxTheme;
    }
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW: {
      if (!mHeaderTheme)
        mHeaderTheme = openTheme(NULL, L"Header");
      return mHeaderTheme;
    }
    case NS_THEME_LISTBOX:
    case NS_THEME_LISTBOX_LISTITEM:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW_TREEITEM: {
      if (!mTreeViewTheme)
        mTreeViewTheme = openTheme(NULL, L"Listview");
      return mTreeViewTheme;
    }
  }
  return NULL;
}

nsresult 
nsNativeThemeWin::GetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                       PRInt32& aPart, PRInt32& aState)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      // Check for default dialog buttons.  These buttons should always look
      // focused.
      if (aState == TS_NORMAL && IsDefaultButton(aFrame))
        aState = TS_FOCUSED;
      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      aPart = (aWidgetType == NS_THEME_CHECKBOX) ? BP_CHECKBOX : BP_RADIO; 

      // XXXdwh This check will need to be more complicated, since HTML radio groups
      // use checked, but XUL radio groups use selected.  There will need to be an
      // IsNodeOfType test for HTML vs. XUL here.
      nsIAtom* atom = (aWidgetType == NS_THEME_CHECKBOX) ? nsWidgetAtoms::checked
                                                         : nsWidgetAtoms::selected;

      PRBool isHTML = PR_FALSE;
      PRBool isHTMLChecked = PR_FALSE;
      PRBool isXULCheckboxRadio = PR_FALSE;
      
      if (!aFrame)
        aState = TS_NORMAL;
      else {
        // For XUL checkboxes and radio buttons, the state of the parent
        // determines our state.
        nsIContent* content = aFrame->GetContent();
        PRBool isXULCheckboxRadio = content->IsNodeOfType(nsINode::eXUL);
        if (!isXULCheckboxRadio) {
          // Attempt a QI.
          nsCOMPtr<nsIDOMHTMLInputElement> inputElt(do_QueryInterface(content));
          if (inputElt) {
            inputElt->GetChecked(&isHTMLChecked);
            isHTML = PR_TRUE;
          }
        }

        if (IsDisabled(isXULCheckboxRadio ? aFrame->GetParent(): aFrame))
          aState = TS_DISABLED;
        else {
          PRInt32 eventState = GetContentState(aFrame, aWidgetType);
          if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
            aState = TS_ACTIVE;
          else if (eventState & NS_EVENT_STATE_HOVER)
            aState = TS_HOVER;
          else 
            aState = TS_NORMAL;
        }
      }

      if (isHTML) {
        if (isHTMLChecked)
          aState += 4;
      }
      else if (aWidgetType == NS_THEME_CHECKBOX ?
               IsChecked(aFrame) : IsSelected(aFrame))
        aState += 4; // 4 unchecked states, 4 checked states.
      return NS_OK;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN: {
      aPart = TFP_TEXTFIELD;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsReadOnly(aFrame)) {
        aState = TFS_READONLY;
        return NS_OK;
      }

      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      return NS_OK;
    }
    case NS_THEME_TOOLTIP: {
      aPart = TTP_STANDARD;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR: {
      aPart = PP_BAR;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_CHUNK: {
      aPart = PP_CHUNK;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_VERTICAL: {
      aPart = PP_BARVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL: {
      aPart = PP_CHUNKVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TOOLBAR_BUTTON: {
      aPart = BP_BUTTON;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }

      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_HOVER) {
        if (IsCheckedButton(aFrame))
          aState = TB_HOVER_CHECKED;
        else
          aState = TS_HOVER;
      }
      else {
        if (IsCheckedButton(aFrame))
          aState = TB_CHECKED;
        else
          aState = TS_NORMAL;
      }
     
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      aPart = SP_BUTTON;
      aState = (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP)*4;
      if (!aFrame)
        aState += TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState += TS_DISABLED;
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState += TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState += TS_HOVER;
        else
          aState += TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL) ?
              SP_TRACKSTARTHOR : SP_TRACKSTARTVERT;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) ?
              SP_THUMBHOR : SP_THUMBVERT;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_ACTIVE) // Hover is not also a requirement for
                                                // the thumb, since the drag is not canceled
                                                // when you move outside the thumb.
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL: {
      aPart = (aWidgetType == NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL) ?
              SP_GRIPPERHOR : SP_GRIPPERVERT;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {
        // XXXdwh The gripper needs to get a hover attribute set on it, since it
        // never goes into :hover.
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_ACTIVE) // Hover is not also a requirement for
                                                // the gripper, since the drag is not canceled
                                                // when you move outside the gripper.
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCALE_HORIZONTAL) ?
              TKP_TRACK : TKP_TRACKVERT;

      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL: {
      aPart = (aWidgetType == NS_THEME_SCALE_THUMB_HORIZONTAL) ?
              TKP_THUMB : TKP_THUMBVERT;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame)) {
        aState = TKP_DISABLED;
      }
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_ACTIVE) // Hover is not also a requirement for
                                                // the thumb, since the drag is not canceled
                                                // when you move outside the thumb.
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_FOCUS)
          aState = TKP_FOCUSED;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else
          aState = TS_NORMAL;
      }
      return NS_OK;
    }
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON: {
      aPart = (aWidgetType == NS_THEME_SPINNER_UP_BUTTON) ?
              SPNP_UP : SPNP_DOWN;
      if (!aFrame)
        aState = TS_NORMAL;
      else if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
      }
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else
          aState = TS_NORMAL;
      }
      return NS_OK;    
    }
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR:
    case NS_THEME_SCROLLBAR: {
      aPart = aState = 0;
      return NS_OK; // These have no part or state.
    }
    case NS_THEME_TOOLBAR: {
      // Use -1 to indicate we don't wish to have the theme background drawn
      // for this item. We will pass any nessessary information via aState,
      // and will render the item using separate code.
      aPart = -1;
      aState = 0;
      if (aFrame) {
        nsIContent* content = aFrame->GetContent();
        nsIContent* parent = content->GetParent();
        if (parent && parent->GetChildAt(0) == content) {
          aState = 1;
        }
      }
      return NS_OK;
    }
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER: {
      aPart = (aWidgetType - NS_THEME_STATUSBAR_PANEL) + 1;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW:
    case NS_THEME_LISTBOX: {
      aPart = TREEVIEW_BODY;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANELS: {
      aPart = TABP_PANELS;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB_PANEL: {
      aPart = TABP_PANEL;
      aState = TS_NORMAL;
      return NS_OK;
    }
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE: {
      aPart = TABP_TAB;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      if (IsDisabled(aFrame)) {
        aState = TS_DISABLED;
        return NS_OK;
      }

      if (IsSelectedTab(aFrame)) {
        aPart = TABP_TAB_SELECTED;
        aState = TS_ACTIVE; // The selected tab is always "pressed".
      }
      else {
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_FOCUS)
          aState = TS_FOCUSED;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }
      
      return NS_OK;
    }
    case NS_THEME_TREEVIEW_HEADER_SORTARROW: {
      // XXX Probably will never work due to a bug in the Luna theme.
      aPart = 4;
      aState = 1;
      return NS_OK;
    }
    case NS_THEME_TREEVIEW_HEADER_CELL: {
      aPart = 1;
      if (!aFrame) {
        aState = TS_NORMAL;
        return NS_OK;
      }
      
      PRInt32 eventState = GetContentState(aFrame, aWidgetType);
      if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
        aState = TS_ACTIVE;
      else if (eventState & NS_EVENT_STATE_FOCUS)
        aState = TS_FOCUSED;
      else if (eventState & NS_EVENT_STATE_HOVER)
        aState = TS_HOVER;
      else 
        aState = TS_NORMAL;
      
      return NS_OK;
    }
    case NS_THEME_DROPDOWN_BUTTON: {
      aPart = CBP_DROPMARKER;

      nsIContent* content = aFrame->GetContent();

      nsIFrame* parentFrame = aFrame->GetParent();
      nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(parentFrame));
      if (menuFrame || (content && content->IsNodeOfType(nsINode::eHTML)) )
         // XUL menu lists and HTML selects get state from parent         
         aFrame = parentFrame;

      if (IsDisabled(aFrame))
        aState = TS_DISABLED;
      else {     
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState = TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState = TS_HOVER;
        else 
          aState = TS_NORMAL;
      }

      return NS_OK;
    }
  }

  aPart = 0;
  aState = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNativeThemeWin::DrawWidgetBackground(nsIRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect)
{
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicDrawWidgetBackground(aContext, aFrame, aWidgetType, aRect, aClipRect); 

  if (!drawThemeBG)
    return NS_ERROR_FAILURE;    

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDeviceContext> dc;
  aContext->GetDeviceContext(*getter_AddRefs(dc));
  PRInt32 p2a = dc->AppUnitsPerDevPixel();
  RECT widgetRect;
  RECT clipRect;
  nsRect tr(aRect);
  nsRect cr(aClipRect);

#ifdef MOZ_CAIRO_GFX
  tr.x = NSAppUnitsToIntPixels(tr.x, p2a);
  tr.y = NSAppUnitsToIntPixels(tr.y, p2a);
  tr.width  = NSAppUnitsToIntPixels(tr.width, p2a);
  tr.height = NSAppUnitsToIntPixels(tr.height, p2a);

  cr.x = NSAppUnitsToIntPixels(cr.x, p2a);
  cr.y = NSAppUnitsToIntPixels(cr.y, p2a);
  cr.width  = NSAppUnitsToIntPixels(cr.width, p2a);
  cr.height = NSAppUnitsToIntPixels(cr.height, p2a);

  nsRefPtr<gfxContext> ctx = (gfxContext*)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);

  gfxPoint offset;
  nsRefPtr<gfxASurface> surf = ctx->CurrentSurface(&offset.x, &offset.y);

  HDC hdc = NS_STATIC_CAST(gfxWindowsSurface*, NS_STATIC_CAST(gfxASurface*, surf.get()))->GetDC();

  /* Need to force the clip to be set */
  ctx->UpdateSurfaceClip();

  /* Covert the current transform to a world transform */
  XFORM oldWorldTransform;
  gfxMatrix m = ctx->CurrentMatrix();
  if (m.HasNonTranslation()) {
    GetWorldTransform(hdc, &oldWorldTransform);

    SetGraphicsMode(hdc, GM_ADVANCED);
    XFORM xform;
    xform.eM11 = (FLOAT) m.xx;
    xform.eM12 = (FLOAT) m.yx;
    xform.eM21 = (FLOAT) m.xy;
    xform.eM22 = (FLOAT) m.yy;
    xform.eDx  = (FLOAT) m.x0;
    xform.eDy  = (FLOAT) m.y0;
    SetWorldTransform (hdc, &xform);
  } else {
    gfxPoint pos(m.GetTranslation());

    tr.x += NSToCoordRound(pos.x);
    tr.y += NSToCoordRound(pos.y);
    cr.x += NSToCoordRound(pos.x);
    cr.y += NSToCoordRound(pos.y);
  }

#if 0
  {
  double dm[6];
  m.ToValues(&dm[0], &dm[1], &dm[2], &dm[3], &dm[4], &dm[5]);
  fprintf (stderr, "xform: %f %f %f %f [%f %f]\n", dm[0], dm[1], dm[2], dm[3], dm[4], dm[5]);
  fprintf (stderr, "tr: [%d %d %d %d]\ncr: [%d %d %d %d]\noff: [%f %f]\n",
           tr.x, tr.y, tr.width, tr.height, cr.x, cr.y, cr.width, cr.height,
           offset.x, offset.y);
  fflush (stderr);
  }
#endif

  /* Set the device offsets as appropriate */
  POINT origViewportOrigin, origBrushOrigin;
  GetViewportOrgEx(hdc, &origViewportOrigin);
  SetViewportOrgEx(hdc, origViewportOrigin.x + (int)offset.x, origViewportOrigin.y + (int)offset.y, NULL);

#else /* non-MOZ_CAIRO_GFX */

  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);

  transformMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  transformMatrix->TransformCoord(&cr.x,&cr.y,&cr.width,&cr.height);

  HDC hdc = (HDC)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
  if (!hdc)
    return NS_ERROR_FAILURE;

#ifndef WINCE
  SetGraphicsMode(hdc, GM_ADVANCED);
#endif

#endif /* MOZ_CAIRO_GFX */

  GetNativeRect(tr, widgetRect);
  GetNativeRect(cr, clipRect);

#if 0
  fprintf (stderr, "widget: [%d %d %d %d]\nclip: [%d %d %d %d]\n",
           widgetRect.left, widgetRect.top, widgetRect.right, widgetRect.bottom,
           clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
  fflush (stderr);
#endif

  // For left edge and right edge tabs, we need to adjust the widget
  // rects and clip rects so that the edges don't get drawn.
  if (aWidgetType == NS_THEME_TAB_LEFT_EDGE || aWidgetType == NS_THEME_TAB_RIGHT_EDGE) {
    // HACK ALERT: There appears to be no way to really obtain this value, so we're forced
    // to just use the default value for Luna (which also happens to be correct for
    // all the other skins I've tried).
    PRInt32 edgeSize = 2;
    
    // Armed with the size of the edge, we now need to either shift to the left or to the
    // right.  The clip rect won't include this extra area, so we know that we're
    // effectively shifting the edge out of view (such that it won't be painted).
    if (aWidgetType == NS_THEME_TAB_LEFT_EDGE)
      // The right edge should not be drawn.  Extend our rect by the edge size.
      widgetRect.right += edgeSize;
    else
      // The left edge should not be drawn.  Move the widget rect's left coord back.
      widgetRect.left -= edgeSize;
  }
  else if (aWidgetType == NS_THEME_TOOLBOX) {
    // The toolbox's toolbar elements will show a 1px border top and bottom.
    // We want the toolbox's background to end right up against the bottom
    // border of the last toolbar, so we simply make it leave a 1px gap at the
    // bottom. This gap will get the bottom border of the last toolbar in it.
    widgetRect.bottom -= 1;
  }

  // widgetRect is the bounding box for a widget, yet the scale track is only
  // a small portion of this size, so the edges of the scale need to be
  // adjusted to the real size of the track.
  if (aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_VERTICAL) {
    RECT contentRect;
    getThemeContentRect(theme, hdc, part, state, &widgetRect, &contentRect);

    SIZE siz;
    getThemePartSize(theme, hdc, part, state, &widgetRect, 1, &siz);

    if (aWidgetType == NS_THEME_SCALE_HORIZONTAL) {
      PRInt32 adjustment = (contentRect.bottom - contentRect.top - siz.cy) / 2 + 1;
      contentRect.top += adjustment;
      contentRect.bottom -= adjustment;
    }
    else {
      PRInt32 adjustment = (contentRect.right - contentRect.left - siz.cx) / 2 + 1;
      // need to subtract one from the left position, otherwise the scale's
      // border isn't visible
      contentRect.left += adjustment - 1;
      contentRect.right -= adjustment;
    }

    drawThemeBG(theme, hdc, part, state, &contentRect, &clipRect);
  }
  // If part is negative, the element wishes us to not render a themed
  // background, instead opting to be drawn specially below.
  else if (part >= 0) {
    drawThemeBG(theme, hdc, part, state, &widgetRect, &clipRect);
  }

  // Draw focus rectangles for XP HTML checkboxes and radio buttons
  // XXX it'd be nice to draw these outside of the frame
  if ((aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO)
      && aFrame->GetContent()->IsNodeOfType(nsINode::eHTML) ||
      aWidgetType == NS_THEME_SCALE_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_VERTICAL) {
      PRInt32 contentState ;
      contentState = GetContentState(aFrame, aWidgetType);  
            
      if (contentState & NS_EVENT_STATE_FOCUS) {
        // setup DC to make DrawFocusRect draw correctly
        POINT vpOrg;
        ::GetViewportOrgEx(hdc, &vpOrg);
        ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top, NULL);
        PRInt32 oldColor;
        oldColor = ::SetTextColor(hdc, 0);
        // draw focus rectangle
        ::DrawFocusRect(hdc, &widgetRect);
        ::SetTextColor(hdc, oldColor);
      }
  }
  else if (aWidgetType == NS_THEME_TOOLBAR) {
    // state == 1 iff this toolbar is the first inside the toolbox, which
    // means we should omit the top border for correct rendering.
    if (state == 1) {
      drawThemeEdge(theme, hdc, 0, 0, &widgetRect, BDR_RAISEDINNER, BF_BOTTOM, &clipRect);
    } else {
      drawThemeEdge(theme, hdc, 0, 0, &widgetRect, BDR_RAISEDINNER, BF_TOP | BF_BOTTOM, &clipRect);
    }
  }

#ifdef MOZ_CAIRO_GFX
  SetViewportOrgEx(hdc, origViewportOrigin.x, origViewportOrigin.y, NULL);

  if (m.HasNonTranslation())
    SetWorldTransform(hdc, &oldWorldTransform);

  surf->MarkDirty();
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::GetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetWidgetBorder(aContext, aFrame, aWidgetType, aResult); 

  (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

  if (!WidgetIsContainer(aWidgetType) ||
      aWidgetType == NS_THEME_TOOLBOX || 
      aWidgetType == NS_THEME_STATUSBAR || 
      aWidgetType == NS_THEME_RESIZER || aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL)
    return NS_OK; // Don't worry about it.

  if (aWidgetType == NS_THEME_TOOLBAR) {
    // A normal toolbar has a 1px border above and below it, with 2px of
    // space either size. If it is the first toolbar, no top border is needed.
    aResult->top = aResult->bottom = 1;
    aResult->left = 2;
    if (aFrame) {
      nsIContent* content = aFrame->GetContent();
      nsIContent* parent = content->GetParent();
      if (parent && parent->GetChildAt(0) == content) {
        aResult->top = 0;
      }
    }
    return NS_OK;
  }

  if (!getThemeContentRect)
    return NS_ERROR_FAILURE;

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  // Get our info.
  RECT outerRect; // Create a fake outer rect.
  outerRect.top = outerRect.left = 100;
  outerRect.right = outerRect.bottom = 200;
  RECT contentRect(outerRect);
  HRESULT res = getThemeContentRect(theme, NULL, part, state, &outerRect, &contentRect);
  
  if (FAILED(res))
    return NS_ERROR_FAILURE;

  // Now compute the delta in each direction and place it in our
  // nsMargin struct.
  aResult->top = contentRect.top - outerRect.top;
  aResult->bottom = outerRect.bottom - contentRect.bottom;
  aResult->left = contentRect.left - outerRect.left;
  aResult->right = outerRect.right - contentRect.right;

  // Remove the edges for tabs that are before or after the selected tab,
  if (aWidgetType == NS_THEME_TAB_LEFT_EDGE)
    // Remove the right edge, since we won't be drawing it.
    aResult->right = 0;
  else if (aWidgetType == NS_THEME_TAB_RIGHT_EDGE)
    // Remove the left edge, since we won't be drawing it.
    aResult->left = 0;

  if (aFrame && aWidgetType == NS_THEME_TEXTFIELD) {
    nsIContent* content = aFrame->GetContent();
    if (content && content->IsNodeOfType(nsINode::eHTML)) {
      // We need to pad textfields by 1 pixel, since the caret will draw
      // flush against the edge by default if we don't.
      aResult->top++;
      aResult->left++;
      aResult->bottom++;
      aResult->right++;
    }
  }

  return NS_OK;
}

PRBool
nsNativeThemeWin::GetWidgetPadding(nsIDeviceContext* aContext, 
                                   nsIFrame* aFrame,
                                   PRUint8 aWidgetType,
                                   nsMargin* aResult)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsNativeThemeWin::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       nsSize* aResult, PRBool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = PR_TRUE;

  HANDLE theme = GetTheme(aWidgetType);
  if (!theme)
    return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);
 
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR || 
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_TAB_PANELS || aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_LISTBOX || aWidgetType == NS_THEME_TREEVIEW)
    return NS_OK; // Don't worry about it.

  if (!getThemePartSize)
    return NS_ERROR_FAILURE;
  
  // Call GetSystemMetrics to determine size for WinXP scrollbars
  // (GetThemeSysSize API returns the optimal size for the theme, but 
  //  Windows appears to always use metrics when drawing standard scrollbars)
  switch (aWidgetType) {
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_DROPDOWN_BUTTON: 
      return ClassicGetMinimumWidgetSize(aContext, aFrame, aWidgetType, aResult, aIsOverridable);
  }

  if (aWidgetType == NS_THEME_SCALE_THUMB_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_THUMB_VERTICAL) {
      *aIsOverridable = PR_FALSE;
  }

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  HDC hdc = (HDC)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
  if (!hdc)
    return NS_ERROR_FAILURE;

  PRInt32 sizeReq = 1; // Best-fit size.
  if (aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL)
    sizeReq = 0; // Best-fit size for progress meters is too large for most 
                 // themes.
                 // In our app, we want these widgets to be able to really shrink down,
                 // so use the min-size request value (of 0).

  SIZE sz;
  getThemePartSize(theme, hdc, part, state, NULL, sizeReq, &sz);
  aResult->width = sz.cx;
  aResult->height = sz.cy;

  if (aWidgetType == NS_THEME_SPINNER_UP_BUTTON ||
      aWidgetType == NS_THEME_SPINNER_DOWN_BUTTON) {
    aResult->width++;
    aResult->height = aResult->height / 2 + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL || 
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL || 
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBAR_PANEL ||
      aWidgetType == NS_THEME_STATUSBAR_RESIZER_PANEL ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_TAB_PANELS ||
      aWidgetType == NS_THEME_TAB_PANEL) {
    *aShouldRepaint = PR_FALSE;
    return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == nsWidgetAtoms::disabled ||
        aAttribute == nsWidgetAtoms::checked ||
        aAttribute == nsWidgetAtoms::selected ||
        aAttribute == nsWidgetAtoms::readonly ||
        aAttribute == nsWidgetAtoms::mozmenuactive)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}

void
nsNativeThemeWin::CloseData()
{
  if (mToolbarTheme) {
    closeTheme(mToolbarTheme);
    mToolbarTheme = NULL;
  }
  if (mScrollbarTheme) {
    closeTheme(mScrollbarTheme);
    mScrollbarTheme = NULL;
  }
  if (mScaleTheme) {
    closeTheme(mScaleTheme);
    mScaleTheme = NULL;
  }
  if (mSpinTheme) {
    closeTheme(mSpinTheme);
    mSpinTheme = NULL;
  }
  if (mRebarTheme) {
    closeTheme(mRebarTheme);
    mRebarTheme = NULL;
  }
  if (mProgressTheme) {
    closeTheme(mProgressTheme);
    mProgressTheme = NULL;
  }
  if (mButtonTheme) {
    closeTheme(mButtonTheme);
    mButtonTheme = NULL;
  }
  if (mTextFieldTheme) {
    closeTheme(mTextFieldTheme);
    mTextFieldTheme = NULL;
  }
  if (mTooltipTheme) {
    closeTheme(mTooltipTheme);
    mTooltipTheme = NULL;
  }
  if (mStatusbarTheme) {
    closeTheme(mStatusbarTheme);
    mStatusbarTheme = NULL;
  }
  if (mTabTheme) {
    closeTheme(mTabTheme);
    mTabTheme = NULL;
  }
  if (mTreeViewTheme) {
    closeTheme(mTreeViewTheme);
    mTreeViewTheme = NULL;
  }
  if (mComboBoxTheme) {
    closeTheme(mComboBoxTheme);
    mComboBoxTheme = NULL;
  }
  if (mHeaderTheme) {
    closeTheme(mHeaderTheme);
    mHeaderTheme = NULL;
  }
}

NS_IMETHODIMP
nsNativeThemeWin::ThemeChanged()
{
  CloseData();
  UpdateConfig();
  return NS_OK;
}

PRBool 
nsNativeThemeWin::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  // XXXdwh We can go even further and call the API to ask if support exists for
  // specific widgets.

  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return PR_FALSE;

  HANDLE theme = NULL;
  if (aWidgetType == NS_THEME_CHECKBOX_CONTAINER)
    theme = GetTheme(NS_THEME_CHECKBOX);
  else if (aWidgetType == NS_THEME_RADIO_CONTAINER)
    theme = GetTheme(NS_THEME_RADIO);
  else
    theme = GetTheme(aWidgetType);

  if ((theme) || (!theme && ClassicThemeSupportsWidget(aPresContext, aFrame, aWidgetType)))
    // turn off theming for some HTML widgets styled by the page
    return (!IsWidgetStyled(aPresContext, aFrame, aWidgetType));
  
  return PR_FALSE;
}

PRBool 
nsNativeThemeWin::WidgetIsContainer(PRUint8 aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON || 
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_CHECKBOX)
    return PR_FALSE;
  return PR_TRUE;
}


/* Windows 9x/NT/2000/Classic XP Theme Support */

PRBool 
nsNativeThemeWin::ClassicThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_DROPDOWN:
    case NS_THEME_TOOLTIP:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_RESIZER:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsNativeThemeWin::ClassicGetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      const nsStyleUserInterface *uiData = aFrame->GetStyleUserInterface();
      if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE) {
        // use different padding for non-focusable buttons
        (*aResult).top = (*aResult).left = 1;
        (*aResult).bottom = (*aResult).right = 2;
      }
      else
        (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 3; 
      break;  
    }
    case NS_THEME_STATUSBAR:
      (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      (*aResult).top = 2;
      break;
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TEXTFIELD:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2;
      break;
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      (*aResult).top = 1;      
      (*aResult).left = 1;
      (*aResult).bottom = 1;
      (*aResult).right = aFrame->GetNextSibling() ? 3 : 1;
      break;
    }    
    case NS_THEME_TOOLTIP:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      break;
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 1;
      break;
    case NS_THEME_MENUBAR:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 0;
      break;
    case NS_THEME_MENUPOPUP:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2;
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      PRBool isTopLevel = PR_FALSE;
      nsIMenuFrame *menuFrame = nsnull;
      CallQueryInterface(aFrame, &menuFrame);

      if (menuFrame) {
        // If this is a real menu item, we should check if it is part of the
        // main menu bar or not, as this affects rendering.
        nsIMenuParent *menuParent = menuFrame->GetMenuParent();
        if (menuParent)
          menuParent->IsMenuBar(isTopLevel);
      }

      // These values are obtained from visual inspection of equivelant
      // native components.
      if (isTopLevel) {
        (*aResult).top = (*aResult).bottom = 1;
        (*aResult).left = 3;
        (*aResult).right = 4;
      } else {
        (*aResult).top = 0;
        (*aResult).bottom = (*aResult).left = (*aResult).right = 1;
      }
      break;
    }
    default:
      (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;
      break;
  }
  return NS_OK;
}

nsresult
nsNativeThemeWin::ClassicGetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       nsSize* aResult, PRBool* aIsOverridable)
{
  (*aResult).width = (*aResult).height = 0;
  *aIsOverridable = PR_TRUE;
  switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: 
      (*aResult).width = (*aResult).height = 13;
      break;
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYVSCROLL);
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      (*aResult).width = ::GetSystemMetrics(SM_CXHSCROLL);
      (*aResult).height = ::GetSystemMetrics(SM_CYHSCROLL);
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      // XXX HACK We should be able to have a minimum height for the scrollbar
      // track.  However, this causes problems when uncollapsing a scrollbar
      // inside a tree.  See bug 201379 for details.

        //      (*aResult).height = ::GetSystemMetrics(SM_CYVTHUMB) << 1;
      break;
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
      (*aResult).width = 12;
      (*aResult).height = 20;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCALE_THUMB_VERTICAL:
      (*aResult).width = 20;
      (*aResult).height = 12;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_DROPDOWN_BUTTON:
      (*aResult).width = ::GetSystemMetrics(SM_CXVSCROLL);
      break;
    case NS_THEME_DROPDOWN:
    case NS_THEME_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:          
    case NS_THEME_DROPDOWN_TEXTFIELD:      
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:      
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
      // no minimum widget size
      break;
    case NS_THEME_RESIZER: {     
#ifndef WINCE
      NONCLIENTMETRICS nc;
      nc.cbSize = sizeof(nc);
      if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nc), &nc, 0))
        (*aResult).width = (*aResult).height = abs(nc.lfStatusFont.lfHeight) + 4;
      else
#endif
        (*aResult).width = (*aResult).height = 15;
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:        
      (*aResult).width = ::GetSystemMetrics(SM_CYVTHUMB);
      (*aResult).height = (*aResult).width >> 1;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      (*aResult).height = ::GetSystemMetrics(SM_CXHTHUMB);
      (*aResult).width = (*aResult).height >> 1;
      *aIsOverridable = PR_FALSE;
      break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
      (*aResult).width = ::GetSystemMetrics(SM_CXHTHUMB) << 1;
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }  
  return NS_OK;
}


nsresult nsNativeThemeWin::ClassicGetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType,
                                 PRInt32& aPart, PRInt32& aState, PRBool& aFocused)
{  
  switch (aWidgetType) {
    case NS_THEME_BUTTON: {
      PRInt32 contentState;

      aPart = DFC_BUTTON;
      aState = DFCS_BUTTONPUSH;
      aFocused = PR_FALSE;

      contentState = GetContentState(aFrame, aWidgetType);
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        if (contentState & NS_EVENT_STATE_ACTIVE && contentState & NS_EVENT_STATE_HOVER) {
          aState |= DFCS_PUSHED;
          const nsStyleUserInterface *uiData = aFrame->GetStyleUserInterface();
          // The down state is flat if the button is focusable
          if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
#ifndef WINCE
            if (!aFrame->GetContent()->IsNodeOfType(nsINode::eHTML))
              aState |= DFCS_FLAT;
#endif
            aFocused = PR_TRUE;
          }
        }
        if ((contentState & NS_EVENT_STATE_FOCUS) || 
          (aState == DFCS_BUTTONPUSH && IsDefaultButton(aFrame))) {
          aFocused = PR_TRUE;          
        }

      }

      return NS_OK;
    }
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO: {
      PRInt32 contentState ;
      aFocused = PR_FALSE;

      aPart = DFC_BUTTON;
      aState = (aWidgetType == NS_THEME_CHECKBOX) ? DFCS_BUTTONCHECK : DFCS_BUTTONRADIO;
      nsIContent* content = aFrame->GetContent();
           
      if (content->IsNodeOfType(nsINode::eXUL)) {
        // XUL
        if (aWidgetType == NS_THEME_CHECKBOX) {
          if (IsChecked(aFrame))
            aState |= DFCS_CHECKED;
        }
        else
          if (IsSelected(aFrame))
            aState |= DFCS_CHECKED;
        contentState = GetContentState(aFrame, aWidgetType);
      }
      else {
        // HTML

        nsCOMPtr<nsIDOMHTMLInputElement> inputElt(do_QueryInterface(content));
        if (inputElt) {
          PRBool isChecked = PR_FALSE;
          inputElt->GetChecked(&isChecked);
          if (isChecked)
            aState |= DFCS_CHECKED;
        }
        contentState = GetContentState(aFrame, aWidgetType);
        if (contentState & NS_EVENT_STATE_FOCUS)
          aFocused = PR_TRUE;
      }

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else if (contentState & NS_EVENT_STATE_ACTIVE && contentState & NS_EVENT_STATE_HOVER)
        aState |= DFCS_PUSHED;
      
      return NS_OK;
    }
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      PRBool isTopLevel = PR_FALSE;
      PRBool isOpen = PR_FALSE;
      PRBool isContainer = PR_FALSE;
      nsIMenuFrame *menuFrame = nsnull;
      CallQueryInterface(aFrame, &menuFrame);

      // We indicate top-level-ness using aPart. 0 is a normal menu item,
      // 1 is a top-level menu item. The state of the item is composed of
      // DFCS_* flags only.
      aPart = 0;
      aState = 0;

      if (menuFrame) {
        // If this is a real menu item, we should check if it is part of the
        // main menu bar or not, and if it is a container, as these affect
        // rendering.
        nsIMenuParent *menuParent = menuFrame->GetMenuParent();
        if (menuParent)
          menuParent->IsMenuBar(isTopLevel);
        menuFrame->MenuIsOpen(isOpen);
        menuFrame->MenuIsContainer(isContainer);
      }

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;

      if (isTopLevel) {
        aPart = 1;
        if (isOpen)
          aState |= DFCS_PUSHED;
      } else {
        if (isContainer)
          aState |= DFCS_CONTAINER;
        if (aFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
          aState |= DFCS_RTL;
      }

      if (CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive))
        aState |= DFCS_HOT;

      // Only menu items of the appropriate type may have tick or bullet marks.
      if (aWidgetType == NS_THEME_CHECKMENUITEM ||
          aWidgetType == NS_THEME_RADIOMENUITEM) {
        if (IsCheckedButton(aFrame))
          aState |= DFCS_CHECKED;
      }

      return NS_OK;
    }
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:     
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:      
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
      // these don't use DrawFrameControl
      return NS_OK;
    case NS_THEME_DROPDOWN_BUTTON: {

      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLCOMBOBOX;
      
      nsIContent* content = aFrame->GetContent();
      nsIFrame* parentFrame = aFrame->GetParent();
      nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(parentFrame));
      if (menuFrame || (content && content->IsNodeOfType(nsINode::eHTML)) )
         // XUL menu lists and HTML selects get state from parent         
         aFrame = parentFrame;
         // XXX the button really shouldn't depress when clicking the 
         // parent, but the button frame is never :active for these controls..

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {     
        PRInt32 eventState = GetContentState(aFrame, aWidgetType);
#ifndef WINCE
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState |= DFCS_PUSHED | DFCS_FLAT;
#endif
      }

      return NS_OK;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
      PRInt32 contentState;

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SCROLLBAR_BUTTON_UP:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_DOWN:
          aState = DFCS_SCROLLDOWN;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_LEFT:
          aState = DFCS_SCROLLLEFT;
          break;
        case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
          aState = DFCS_SCROLLRIGHT;
          break;
      }      
      
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        contentState = GetContentState(aFrame, aWidgetType);
#ifndef WINCE
        if (contentState & NS_EVENT_STATE_HOVER && contentState & NS_EVENT_STATE_ACTIVE)
          aState |= DFCS_PUSHED | DFCS_FLAT;      
#endif
      }

      return NS_OK;
    }
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON: {
      PRInt32 contentState;

      aPart = DFC_SCROLL;
      switch (aWidgetType) {
        case NS_THEME_SPINNER_UP_BUTTON:
          aState = DFCS_SCROLLUP;
          break;
        case NS_THEME_SPINNER_DOWN_BUTTON:
          aState = DFCS_SCROLLDOWN;
          break;
      }      
      
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      else {
        contentState = GetContentState(aFrame, aWidgetType);
        if (contentState & NS_EVENT_STATE_HOVER && contentState & NS_EVENT_STATE_ACTIVE)
          aState |= DFCS_PUSHED;
      }

      return NS_OK;    
    }
    case NS_THEME_RESIZER:    
      aPart = DFC_SCROLL;
      aState = DFCS_SCROLLSIZEGRIP;
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// Draw classic Windows tab
// (no system API for this, but DrawEdge can draw all the parts of a tab)
static void DrawTab(HDC hdc, const RECT& R, PRInt32 aPosition, PRBool aSelected,
                    PRBool aDrawLeft, PRBool aDrawRight)
{
  PRInt32 leftFlag, topFlag, rightFlag, lightFlag, shadeFlag;  
  RECT topRect, sideRect, bottomRect, lightRect, shadeRect;
  PRInt32 selectedOffset, lOffset, rOffset;

  selectedOffset = aSelected ? 1 : 0;
  lOffset = aDrawLeft ? 2 : 0;
  rOffset = aDrawRight ? 2 : 0;

  // Get info for tab orientation/position (Left, Top, Right, Bottom)
  switch (aPosition) {
    case BF_LEFT:
      leftFlag = BF_TOP; topFlag = BF_LEFT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2, R.top, R.right-2+selectedOffset, R.bottom);
      ::SetRect(&bottomRect, R.right-2, R.top, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);
      ::SetRect(&shadeRect, R.left+1, R.bottom-2, R.left+2, R.bottom-1);
      break;
    case BF_TOP:    
      leftFlag = BF_LEFT; topFlag = BF_TOP;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPRIGHT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMRIGHT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2, R.right, R.bottom-1+selectedOffset);
      ::SetRect(&bottomRect, R.left, R.bottom-1, R.right, R.bottom);
      ::SetRect(&lightRect, R.left, R.top, R.left+3, R.top+3);      
      ::SetRect(&shadeRect, R.right-2, R.top+1, R.right-1, R.top+2);      
      break;
    case BF_RIGHT:    
      leftFlag = BF_TOP; topFlag = BF_RIGHT;
      rightFlag = BF_BOTTOM;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left, R.top+lOffset, R.right, R.bottom-rOffset);
      ::SetRect(&sideRect, R.left+2-selectedOffset, R.top, R.right-2, R.bottom);
      ::SetRect(&bottomRect, R.left, R.top, R.left+2, R.bottom);
      ::SetRect(&lightRect, R.right-3, R.top, R.right-1, R.top+2);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
    case BF_BOTTOM:    
      leftFlag = BF_LEFT; topFlag = BF_BOTTOM;
      rightFlag = BF_RIGHT;
      lightFlag = BF_DIAGONAL_ENDTOPLEFT;
      shadeFlag = BF_DIAGONAL_ENDBOTTOMLEFT;

      ::SetRect(&topRect, R.left+lOffset, R.top, R.right-rOffset, R.bottom);
      ::SetRect(&sideRect, R.left, R.top+2-selectedOffset, R.right, R.bottom-2);
      ::SetRect(&bottomRect, R.left, R.top, R.right, R.top+2);
      ::SetRect(&lightRect, R.left, R.bottom-3, R.left+2, R.bottom-1);
      ::SetRect(&shadeRect, R.right-2, R.bottom-3, R.right, R.bottom-1);
      break;
  }

  // Background
  ::FillRect(hdc, &R, (HBRUSH) (COLOR_3DFACE+1) );

  // Tab "Top"
  ::DrawEdge(hdc, &topRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Bottom"
  if (!aSelected)
    ::DrawEdge(hdc, &bottomRect, EDGE_RAISED, BF_SOFT | topFlag);

  // Tab "Sides"
  if (!aDrawLeft)
    leftFlag = 0;
  if (!aDrawRight)
    rightFlag = 0;
  ::DrawEdge(hdc, &sideRect, EDGE_RAISED, BF_SOFT | leftFlag | rightFlag);

  // Tab Diagonal Corners
  if (aDrawLeft)
    ::DrawEdge(hdc, &lightRect, EDGE_RAISED, BF_SOFT | lightFlag);

  if (aDrawRight)
    ::DrawEdge(hdc, &shadeRect, EDGE_RAISED, BF_SOFT | shadeFlag);
}

#ifndef WINCE
static void DrawMenuImage(HDC hdc, const RECT& rc, PRInt32 aComponent, PRUint32 aColor)
{
  // This procedure creates a memory bitmap to contain the check mark, draws
  // it into the bitmap (it is a mask image), then composes it onto the menu
  // item in appropriate colors.
  HDC hMemoryDC = ::CreateCompatibleDC(hdc);
  if (hMemoryDC) {
    // XXXjgr We should ideally be caching these, but we wont be notified when
    // they change currently, so we can't do so easily. Same for the bitmap.
    int checkW = ::GetSystemMetrics(SM_CXMENUCHECK);
    int checkH = ::GetSystemMetrics(SM_CYMENUCHECK);

    HBITMAP hMonoBitmap = ::CreateBitmap(checkW, checkH, 1, 1, NULL);
    if (hMonoBitmap) {

      HBITMAP hPrevBitmap = (HBITMAP) ::SelectObject(hMemoryDC, hMonoBitmap);
      if (hPrevBitmap) {

        // XXXjgr This will go pear-shaped if the image is bigger than the
        // provided rect. What should we do?
        RECT imgRect = { 0, 0, checkW, checkH };
        POINT imgPos = {
              rc.left + (rc.right  - rc.left - checkW) / 2,
              rc.top  + (rc.bottom - rc.top  - checkH) / 2
            };

        ::DrawFrameControl(hMemoryDC, &imgRect, DFC_MENU, aComponent);
        COLORREF oldTextCol = ::SetTextColor(hdc, 0x00000000);
        COLORREF oldBackCol = ::SetBkColor(hdc, 0x00FFFFFF);
        ::BitBlt(hdc, imgPos.x, imgPos.y, checkW, checkH, hMemoryDC, 0, 0, SRCAND);
        ::SetTextColor(hdc, ::GetSysColor(aColor));
        ::SetBkColor(hdc, 0x00000000);
        ::BitBlt(hdc, imgPos.x, imgPos.y, checkW, checkH, hMemoryDC, 0, 0, SRCPAINT);
        ::SetTextColor(hdc, oldTextCol);
        ::SetBkColor(hdc, oldBackCol);
        ::SelectObject(hMemoryDC, hPrevBitmap);
      }
      ::DeleteObject(hMonoBitmap);
    }
    ::DeleteDC(hMemoryDC);
  }
}
#endif

void nsNativeThemeWin::DrawCheckedRect(HDC hdc, const RECT& rc, PRInt32 fore, PRInt32 back,
                                       HBRUSH defaultBack)
{
  static WORD patBits[8] = {
    0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55
  };
        
  HBITMAP patBmp = ::CreateBitmap(8, 8, 1, 1, patBits);
  if (patBmp) {
    HBRUSH brush = (HBRUSH) ::CreatePatternBrush(patBmp);
    if (brush) {        
      COLORREF oldForeColor = ::SetTextColor(hdc, ::GetSysColor(fore));
      COLORREF oldBackColor = ::SetBkColor(hdc, ::GetSysColor(back));
      POINT vpOrg;

#ifndef WINCE
      ::UnrealizeObject(brush);
#endif
      ::GetViewportOrgEx(hdc, &vpOrg);
      ::SetBrushOrgEx(hdc, vpOrg.x + rc.left, vpOrg.y + rc.top, NULL);
      HBRUSH oldBrush = (HBRUSH) ::SelectObject(hdc, brush);
      ::FillRect(hdc, &rc, brush);
      ::SetTextColor(hdc, oldForeColor);
      ::SetBkColor(hdc, oldBackColor);
      ::SelectObject(hdc, oldBrush);
      ::DeleteObject(brush);          
    }
    else
      ::FillRect(hdc, &rc, defaultBack);
  
    ::DeleteObject(patBmp);
  }
}

nsresult nsNativeThemeWin::ClassicDrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect)
{         
  PRInt32 part, state;
  PRBool focused;
  nsresult rv;
  rv = ClassicGetThemePartAndState(aFrame, aWidgetType, part, state, focused);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDeviceContext> dc;
  aContext->GetDeviceContext(*getter_AddRefs(dc));
  PRInt32 p2a = dc->AppUnitsPerDevPixel();
  RECT widgetRect;
  nsRect tr(aRect);

#ifdef MOZ_CAIRO_GFX
  tr.x = NSAppUnitsToIntPixels(tr.x, p2a);
  tr.y = NSAppUnitsToIntPixels(tr.y, p2a);
  tr.width  = NSAppUnitsToIntPixels(tr.width, p2a);
  tr.height = NSAppUnitsToIntPixels(tr.height, p2a);

  nsRefPtr<gfxContext> ctx = (gfxContext*)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);

  gfxPoint offset;
  nsRefPtr<gfxASurface> surf = ctx->CurrentSurface(&offset.x, &offset.y);
  HDC hdc = NS_STATIC_CAST(gfxWindowsSurface*, NS_STATIC_CAST(gfxASurface*, surf.get()))->GetDC();

  /* Need to force the clip to be set */
  ctx->UpdateSurfaceClip();

  /* Covert the current transform to a world transform */
  XFORM oldWorldTransform;
  gfxMatrix m = ctx->CurrentMatrix();
  if (m.HasNonTranslation()) {
    GetWorldTransform(hdc, &oldWorldTransform);

    SetGraphicsMode(hdc, GM_ADVANCED);
    XFORM xform;
    xform.eM11 = (FLOAT) m.xx;
    xform.eM12 = (FLOAT) m.yx;
    xform.eM21 = (FLOAT) m.xy;
    xform.eM22 = (FLOAT) m.yy;
    xform.eDx  = (FLOAT) m.x0;
    xform.eDy  = (FLOAT) m.y0;
    SetWorldTransform (hdc, &xform);
  } else {
    gfxPoint pos(m.GetTranslation());

    tr.x += NSToCoordRound(pos.x);
    tr.y += NSToCoordRound(pos.y);
  }

  /* Set the device offsets as appropriate */
  POINT origViewportOrigin;
  GetViewportOrgEx(hdc, &origViewportOrigin);
  SetViewportOrgEx(hdc, origViewportOrigin.x + (int)offset.x, origViewportOrigin.y + (int)offset.y, NULL);

#else /* non-MOZ_CAIRO_GFX */

  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);
  transformMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

  HDC hdc = (HDC)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);

#endif /* MOZ_CAIRO_GFX */

  GetNativeRect(tr, widgetRect); 

  rv = NS_OK;
  switch (aWidgetType) { 
    // Draw button
    case NS_THEME_BUTTON: {
      if (focused) {
        // draw dark button focus border first
        HBRUSH brush;        
        brush = ::GetSysColorBrush(COLOR_3DDKSHADOW);
        if (brush)
          ::FrameRect(hdc, &widgetRect, brush);
        InflateRect(&widgetRect, -1, -1);
      }
      // fall-through...
    }
    // Draw controls supported by DrawFrameControl
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_RESIZER: {
      PRInt32 oldTA;
      // setup DC to make DrawFrameControl draw correctly
      oldTA = ::SetTextAlign(hdc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
      ::DrawFrameControl(hdc, &widgetRect, part, state);
      ::SetTextAlign(hdc, oldTA);

      // Draw focus rectangles for HTML checkboxes and radio buttons
      // XXX it'd be nice to draw these outside of the frame
      if (focused && (aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO)) {
        // setup DC to make DrawFocusRect draw correctly
        POINT vpOrg;
        ::GetViewportOrgEx(hdc, &vpOrg);
        ::SetBrushOrgEx(hdc, vpOrg.x + widgetRect.left, vpOrg.y + widgetRect.top, NULL);
        PRInt32 oldColor;
        oldColor = ::SetTextColor(hdc, 0);
        // draw focus rectangle
        ::DrawFocusRect(hdc, &widgetRect);
        ::SetTextColor(hdc, oldColor);
      }
      break;
    }
    // Draw controls with 2px 3D inset border
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in background
      if (IsDisabled(aFrame) ||
          (aFrame->GetContent()->IsNodeOfType(nsINode::eXUL) &&
           IsReadOnly(aFrame)))
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));
      else
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));

      break;
    }
    case NS_THEME_TREEVIEW: {
      // Draw inset edge
      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

      // Fill in window color background
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_WINDOW+1));

      break;
    }
    // Draw ToolTip background
    case NS_THEME_TOOLTIP:
      HBRUSH brush;
      brush = ::GetSysColorBrush(COLOR_3DDKSHADOW);
      if (brush)
        ::FrameRect(hdc, &widgetRect, brush);
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_INFOBK+1));

      break;
    // Draw 3D face background controls
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      // Draw 3D border
      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);
      InflateRect(&widgetRect, -1, -1);
      // fall through
    case NS_THEME_TAB_PANEL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_RESIZER_PANEL: {
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_BTNFACE+1));

      break;
    }
    // Draw 3D inset statusbar panel
    case NS_THEME_STATUSBAR_PANEL: {
      if (aFrame->GetNextSibling())
        widgetRect.right -= 2; // space between sibling status panels

      ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT | BF_MIDDLE);

      break;
    }
    // Draw scrollbar thumb
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);

      break;
    case NS_THEME_SCALE_THUMB_VERTICAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_SOFT | BF_MIDDLE | BF_ADJUST);
      if (IsDisabled(aFrame)) {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DFACE, COLOR_3DHILIGHT,
                        (HBRUSH) COLOR_3DHILIGHT);
      }

      break;
    // Draw scrollbar track background
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {

      // Windows fills in the scrollbar track differently 
      // depending on whether these are equal
      DWORD color3D, colorScrollbar, colorWindow;

      color3D = ::GetSysColor(COLOR_3DFACE);      
      colorWindow = ::GetSysColor(COLOR_WINDOW);
      colorScrollbar = ::GetSysColor(COLOR_SCROLLBAR);
      
      if ((color3D != colorScrollbar) && (colorWindow != colorScrollbar))
        // Use solid brush
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_SCROLLBAR+1));
      else
      {
        DrawCheckedRect(hdc, widgetRect, COLOR_3DHILIGHT, COLOR_3DFACE,
                        (HBRUSH) COLOR_SCROLLBAR+1);
      }
      // XXX should invert the part of the track being clicked here
      // but the track is never :active

      break;
    }
    // Draw scale track background
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_HORIZONTAL: {
      if (aWidgetType == NS_THEME_SCALE_HORIZONTAL) {
        PRInt32 adjustment = (widgetRect.bottom - widgetRect.top) / 2 - 2;
        widgetRect.top += adjustment;
        widgetRect.bottom -= adjustment;
      }
      else {
        PRInt32 adjustment = (widgetRect.right - widgetRect.left) / 2 - 2;
        widgetRect.left += adjustment;
        widgetRect.right -= adjustment;
      }

      ::DrawEdge(hdc, &widgetRect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
      ::FillRect(hdc, &widgetRect, (HBRUSH) GetStockObject(GRAY_BRUSH));
 
      break;
    }
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));

      break;
    // Draw Tab
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE: {
      DrawTab(hdc, widgetRect,
        IsBottomTab(aFrame) ? BF_BOTTOM : BF_TOP, 
        IsSelectedTab(aFrame),
        aWidgetType != NS_THEME_TAB_RIGHT_EDGE,
        aWidgetType != NS_THEME_TAB_LEFT_EDGE);      

      break;
    }
    case NS_THEME_TAB_PANELS:
      ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_SOFT | BF_MIDDLE |
          BF_LEFT | BF_RIGHT | BF_BOTTOM);

      break;
    case NS_THEME_MENUBAR:
      break;
    case NS_THEME_MENUPOPUP:
      if (mFlatMenus) {
        ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_MENU+1));
        ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_BTNSHADOW));
      } else {
        ::DrawEdge(hdc, &widgetRect, EDGE_RAISED, BF_RECT | BF_MIDDLE);
      }
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      // part == 0 for normal items
      // part == 1 for top-level menu items
      if (mFlatMenus) {
        // Not disabled and hot/pushed.
        if ((state & (DFCS_HOT | DFCS_PUSHED)) != 0) {
          ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_MENUHILIGHT+1));
          ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_HIGHLIGHT));
        }
      } else {
        if (part == 1) {
          if ((state & DFCS_INACTIVE) == 0) {
            if ((state & DFCS_PUSHED) != 0) {
              ::DrawEdge(hdc, &widgetRect, BDR_SUNKENOUTER, BF_RECT);
            } else if ((state & DFCS_HOT) != 0) {
              ::DrawEdge(hdc, &widgetRect, BDR_RAISEDINNER, BF_RECT);
            }
          }
        } else {
          if ((state & (DFCS_HOT | DFCS_PUSHED)) != 0) {
            ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_HIGHLIGHT+1));
          }
        }
      }
      if (((state & DFCS_CHECKED) != 0) || ((state & DFCS_CONTAINER) != 0)) {
        RECT menuRectStart, menuRectEnd;
        PRUint32 color = COLOR_MENUTEXT;

        if ((state & DFCS_INACTIVE) != 0)
          color = COLOR_GRAYTEXT;
        else if ((state & DFCS_HOT) != 0)
          color = COLOR_HIGHLIGHTTEXT;

        ::CopyRect(&menuRectStart, &widgetRect);
        ::InflateRect(&menuRectStart, -1, -1);
        ::CopyRect(&menuRectEnd, &menuRectStart);

        // WARNING: This value of 15 must match the value in menu.css for the min-width of .menu-iconic-left
        if ((state & DFCS_RTL) == 0) {
          menuRectStart.right = menuRectStart.left  + 15;  // Left box
          menuRectEnd.left    = menuRectEnd.right   - 15;  // Right box
        } else {
          menuRectStart.left  = menuRectStart.right - 15;  // Right box
          menuRectEnd.right   = menuRectEnd.left    + 15;  // left box
        }

#ifndef WINCE
        if ((state & DFCS_CHECKED) != 0) {
          if (aWidgetType == NS_THEME_CHECKMENUITEM) {
            DrawMenuImage(hdc, menuRectStart, DFCS_MENUCHECK, color);
          } else if (aWidgetType == NS_THEME_RADIOMENUITEM) {
            DrawMenuImage(hdc, menuRectStart, DFCS_MENUBULLET, color);
          }
        }
        if ((state & DFCS_CONTAINER) != 0) {
          if ((state & DFCS_RTL) == 0)
            DrawMenuImage(hdc, menuRectEnd, DFCS_MENUARROW, color);
          else
            DrawMenuImage(hdc, menuRectEnd, DFCS_MENUARROWRIGHT, color);
        }
#endif
      }
      break;
    }
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

#ifdef MOZ_CAIRO_GFX
  SetViewportOrgEx(hdc, origViewportOrigin.x, origViewportOrigin.y, NULL);

  if (m.HasNonTranslation())
    SetWorldTransform(hdc, &oldWorldTransform);

  surf->MarkDirty();
#endif

  return rv;
}


///////////////////////////////////////////
// Creation Routine
///////////////////////////////////////////
NS_METHOD NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsNativeThemeWin* theme = new nsNativeThemeWin();
  if (!theme)
    return NS_ERROR_OUT_OF_MEMORY;
  return theme->QueryInterface(aIID, aResult);
}
