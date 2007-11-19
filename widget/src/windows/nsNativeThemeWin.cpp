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
 *   Simon Bünzli (zeniko@gmail.com)
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
#include "nsWidgetAtoms.h"
#include <malloc.h>
#include "nsWindow.h"

#include "gfxPlatform.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsNativeDrawing.h"

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

// Toolbar constants
#define TP_SEPARATOR 5

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

// Vista only; implict hover state.
// BASE + 0 = UP, + 1 = DOWN, etc.
#define SP_BUTTON_IMPLICIT_HOVER_BASE   17

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

// Menu Constants
#define MENU_BARBACKGROUND 7
#define MENU_BARITEM 8
#define MENU_POPUPBACKGROUND 9
#define MENU_POPUPBORDERS 10
#define MENU_POPUPCHECK 11
#define MENU_POPUPCHECKBACKGROUND 12
#define MENU_POPUPGUTTER 13
#define MENU_POPUPITEM 14
#define MENU_POPUPSEPARATOR 15
#define MENU_POPUPSUBMENU 16
#define MENU_SYSTEMCLOSE 17
#define MENU_SYSTEMMAXIMIZE 18
#define MENU_SYSTEMMINIMIZE 19
#define MENU_SYSTEMRESTORE 20

#define MB_ACTIVE 1
#define MB_INACTIVE 2

#define MS_NORMAL    1
#define MS_SELECTED  2
#define MS_DEMOTED   3

#define MBI_NORMAL 1
#define MBI_HOT 2
#define MBI_PUSHED 3
#define MBI_DISABLED 4
#define MBI_DISABLEDHOT 5
#define MBI_DISABLEDPUSHED 6

#define MC_CHECKMARKNORMAL 1
#define MC_CHECKMARKDISABLED 2
#define MC_BULLETNORMAL 3
#define MC_BULLETDISABLED 4

#define MCB_DISABLED 1
#define MCB_NORMAL 2
#define MCB_BITMAP 3

#define MPI_NORMAL 1
#define MPI_HOT 2
#define MPI_DISABLED 3
#define MPI_DISABLEDHOT 4

#define MSM_NORMAL 1
#define MSM_DISABLED 2

// Theme size constants
// minimum size
#define TS_MIN 0
// size without stretching
#define TS_TRUE 1
// size that theme mgr will use to draw part
#define TS_DRAW 2

#define TMT_SIZINGMARGINS 3601
#define TMT_CONTENTMARGINS 3602
#define TMT_CAPTIONMARGINS 3603

// Rebar constants
#define RP_BAND              3

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

// Our extra constants for passing a little bit more info to the renderer.
#define DFCS_RTL             0x00010000

// Toolbar separator dimension which can't be gotten from Windows
#define TB_SEPARATOR_HEIGHT  2

NS_IMPL_ISUPPORTS1(nsNativeThemeWin, nsITheme)

struct MARGINS
{
  int cxLeftWidth;
  int cxRightWidth;
  int cyTopHeight;
  int cyBottomHeight;
};

typedef HANDLE (WINAPI*OpenThemeDataPtr)(HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (WINAPI*CloseThemeDataPtr)(HANDLE hTheme);
typedef HRESULT (WINAPI*DrawThemeBackgroundPtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pRect,
                                          const RECT* pClipRect);
typedef HRESULT (WINAPI*DrawThemeEdgePtr)(HANDLE hTheme, HDC hdc, int iPartId, 
                                          int iStateId, const RECT *pDestRect,
                                          uint uEdge, uint uFlags,
                                          const RECT* pContentRect);
typedef HRESULT (WINAPI*GetThemeContentRectPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                          int iStateId, const RECT* pRect,
                                          RECT* pContentRect);
typedef HRESULT (WINAPI*GetThemePartSizePtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                       int iStateId, RECT* prc, int ts,
                                       SIZE* psz);
typedef HRESULT (WINAPI*GetThemeSysFontPtr)(HANDLE hTheme, int iFontId, OUT LOGFONT* pFont);
typedef HRESULT (WINAPI*GetThemeColorPtr)(HANDLE hTheme, int iPartId,
                                   int iStateId, int iPropId, OUT COLORREF* pFont);
typedef HRESULT (WINAPI*GetThemeMarginsPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                         int iStateid, int iPropId,
                                         LPRECT prc, MARGINS *pMargins);

static OpenThemeDataPtr openTheme = NULL;
static CloseThemeDataPtr closeTheme = NULL;
static DrawThemeBackgroundPtr drawThemeBG = NULL;
static DrawThemeEdgePtr drawThemeEdge = NULL;
static GetThemeContentRectPtr getThemeContentRect = NULL;
static GetThemePartSizePtr getThemePartSize = NULL;
static GetThemeSysFontPtr getThemeSysFont = NULL;
static GetThemeColorPtr getThemeColor = NULL;
static GetThemeMarginsPtr getThemeMargins = NULL;

static const char kThemeLibraryName[] = "uxtheme.dll";

static inline bool IsCheckboxWidgetType(PRUint8 aWidgetType)
{
  return (aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_CHECKBOX_SMALL);
}

static inline bool IsRadioWidgetType(PRUint8 aWidgetType)
{
  return (aWidgetType == NS_THEME_RADIO || aWidgetType == NS_THEME_RADIO_SMALL);
}

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
  mMenuTheme = NULL;

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
    getThemeMargins = (GetThemeMarginsPtr)GetProcAddress(mThemeDLL, "GetThemeMargins");
  }
  mOsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&mOsVersion);

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

static PRBool IsTopLevelMenu(nsIFrame *aFrame)
{
  PRBool isTopLevel(PR_FALSE);
  nsIMenuFrame *menuFrame(nsnull);
  CallQueryInterface(aFrame, &menuFrame);

  if (menuFrame) {
    isTopLevel = menuFrame->IsOnMenuBar();
  }
  return isTopLevel;
}


static SIZE GetCheckboxSize(HANDLE theme, HDC hdc)
{
    SIZE checkboxSize;
    getThemePartSize(theme, hdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, NULL, TS_TRUE, &checkboxSize);

    MARGINS checkboxSizing;
    MARGINS checkboxContent;
    getThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, TMT_SIZINGMARGINS, NULL, &checkboxSizing);
    getThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, TMT_CONTENTMARGINS, NULL, &checkboxContent);

    int leftMargin = checkboxSizing.cxLeftWidth;
    int rightMargin = checkboxSizing.cxRightWidth;
    int topMargin = checkboxSizing.cyTopHeight;
    int bottomMargin = checkboxSizing.cyBottomHeight;

    int width = leftMargin + checkboxSize.cx + rightMargin;
    int height = topMargin + checkboxSize.cy + bottomMargin;
    SIZE ret;
    ret.cx = width;
    ret.cy = height;
    return ret;
}
static SIZE GetCheckboxBounds(HANDLE theme, HDC hdc)
{
    MARGINS checkboxSizing;
    MARGINS checkboxContent;
    getThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, TMT_SIZINGMARGINS, NULL, &checkboxSizing);
    getThemeMargins(theme, hdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, TMT_CONTENTMARGINS, NULL, &checkboxContent);

#define posdx(d) ((d) > 0 ? d : 0)

    int dx = posdx(checkboxContent.cxRightWidth - checkboxSizing.cxRightWidth) + posdx(checkboxContent.cxLeftWidth - checkboxSizing.cxLeftWidth);
    int dy = posdx(checkboxContent.cyTopHeight - checkboxSizing.cyTopHeight) + posdx(checkboxContent.cyBottomHeight - checkboxSizing.cyBottomHeight);

#undef posdx

    SIZE ret(GetCheckboxSize(theme,hdc));
    ret.cx += dx;
    ret.cy += dy;
    return ret;
}
static SIZE GetGutterSize(HANDLE theme, HDC hdc)
{
    SIZE gutterSize;
    getThemePartSize(theme, hdc, MENU_POPUPGUTTER, 0, NULL, TS_TRUE, &gutterSize);

    SIZE checkboxSize(GetCheckboxBounds(theme, hdc));

    SIZE itemSize;
    getThemePartSize(theme, hdc, MENU_POPUPITEM, MPI_NORMAL, NULL, TS_TRUE, &itemSize);

    int width = max(itemSize.cx, checkboxSize.cx + gutterSize.cx);
    int height = max(itemSize.cy, checkboxSize.cy);
    SIZE ret;
    ret.cx = width;
    ret.cy = height;
    return ret;
}

static PRBool IsFrameRTL(nsIFrame *frame)
{
  return frame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
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
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL: {
      if (!mButtonTheme)
        mButtonTheme = openTheme(NULL, L"Button");
      return mButtonTheme;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
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
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TOOLBAR_SEPARATOR: {
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
    case NS_THEME_SCROLLBAR_SMALL:
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
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUIMAGE:
    case NS_THEME_MENUITEMTEXT:
      if (!mMenuTheme)
        mMenuTheme = openTheme(NULL, L"Menu");
      return mMenuTheme;
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
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL: {
      bool isCheckbox = IsCheckboxWidgetType(aWidgetType);
      aPart = isCheckbox ? BP_CHECKBOX : BP_RADIO;

      // XXXdwh This check will need to be more complicated, since HTML radio groups
      // use checked, but XUL radio groups use selected.  There will need to be an
      // IsNodeOfType test for HTML vs. XUL here.
      nsIAtom* atom = isCheckbox ? nsWidgetAtoms::checked
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
      else if (isCheckbox ? IsChecked(aFrame) : IsSelected(aFrame))
        aState += 4; // 4 unchecked states, 4 checked states.
      return NS_OK;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
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
    case NS_THEME_TOOLBAR_SEPARATOR: {
      aPart = TP_SEPARATOR;
      aState = TS_NORMAL;
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
        nsIFrame *parent = aFrame->GetParent();
        PRInt32 parentState = GetContentState(parent, parent->GetStyleDisplay()->mAppearance);
        if (eventState & NS_EVENT_STATE_HOVER && eventState & NS_EVENT_STATE_ACTIVE)
          aState += TS_ACTIVE;
        else if (eventState & NS_EVENT_STATE_HOVER)
          aState += TS_HOVER;
        else if (GetWindowsVersion() >= VISTA_VERSION && parentState & NS_EVENT_STATE_HOVER)
          aState = (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP) + SP_BUTTON_IMPLICIT_HOVER_BASE;
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
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL: {
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
        // XXXzeniko hiding the first toolbar will result in an unwanted margin
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
      if (parentFrame->GetType() == nsWidgetAtoms::menuFrame ||
          (content && content->IsNodeOfType(nsINode::eHTML)))
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
    case NS_THEME_MENUPOPUP: {
      aPart = MENU_POPUPBACKGROUND;
      aState = MB_ACTIVE;
      return NS_OK;
    }
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM: 
    case NS_THEME_RADIOMENUITEM: {
      PRBool isTopLevel = PR_FALSE;
      PRBool isOpen = PR_FALSE;
      PRBool isHover = PR_FALSE;
      nsIMenuFrame *menuFrame;
      CallQueryInterface(aFrame, &menuFrame);

      isTopLevel = IsTopLevelMenu(aFrame);

      if (menuFrame)
        isOpen = menuFrame->IsOpen();
      isHover = CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive);

      if (isTopLevel) {
        aPart = MENU_BARITEM;

        if (isOpen)
          aState = MBI_PUSHED;
        else if (isHover)
          aState = MBI_HOT;
        else
          aState = MBI_NORMAL;

        // the disabled states are offset by 3
        if (IsDisabled(aFrame))
          aState += 3;
      } else {
        aPart = MENU_POPUPITEM;

        if (isHover)
          aState = MPI_HOT;
        else
          aState = MPI_NORMAL;

        // the disabled states are offset by 2
        if (IsDisabled(aFrame))
          aState += 2;
      }

      return NS_OK;
    }
    case NS_THEME_MENUSEPARATOR:
      aPart = MENU_POPUPSEPARATOR;
      aState = 0;
      return NS_OK;
    case NS_THEME_MENUARROW:
      aPart = MENU_POPUPSUBMENU;
      aState = IsDisabled(aFrame) ? MSM_DISABLED : MSM_NORMAL;
      return NS_OK;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        PRBool isChecked;
        PRBool isDisabled;

        isChecked = CheckBooleanAttr(aFrame, nsWidgetAtoms::checked);
        isDisabled = CheckBooleanAttr(aFrame, nsWidgetAtoms::disabled);

        aPart = MENU_POPUPCHECK;
        aState = MC_CHECKMARKNORMAL;

        // Radio states are offset by 2
        if (aWidgetType == NS_THEME_MENURADIO)
          aState += 2;

        // the disabled states are offset by 1
        if (isDisabled)
          aState += 1;

        return NS_OK;
      }
    case NS_THEME_MENUITEMTEXT:
    case NS_THEME_MENUIMAGE:
      aPart = -1;
      aState = 0;
      return NS_OK;
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

#ifndef WINCE
  if (aWidgetType == NS_THEME_TOOLTIP && mOsVersion.dwMajorVersion < 6) {
    // BUG #161600: When rendering a non-classic tooltip, check
    // for Windows prior to Vista, and if so, force a classic rendering
    return ClassicDrawWidgetBackground(aContext, aFrame, aWidgetType, aRect, aClipRect);
  }
#endif

  if (!drawThemeBG)
    return NS_ERROR_FAILURE;    

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDeviceContext> dc;
  aContext->GetDeviceContext(*getter_AddRefs(dc));
  gfxFloat p2a = gfxFloat(dc->AppUnitsPerDevPixel());
  RECT widgetRect;
  RECT clipRect;
  gfxRect tr(aRect.x, aRect.y, aRect.width, aRect.height),
          cr(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

  tr.ScaleInverse(p2a);
  cr.ScaleInverse(p2a);

  nsRefPtr<gfxContext> ctx = (gfxContext*)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);

  gfxWindowsNativeDrawing nativeDrawing(ctx, cr, GetWidgetNativeDrawingFlags(aWidgetType));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc)
    return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);
  nativeDrawing.TransformToNativeRect(cr, clipRect);

#if 0
  {
    fprintf (stderr, "xform: %f %f %f %f [%f %f]\n", m.xx, m.yx, m.xy, m.yy, m.x0, m.y0);
    fprintf (stderr, "tr: [%d %d %d %d]\ncr: [%d %d %d %d]\noff: [%f %f]\n",
             tr.x, tr.y, tr.width, tr.height, cr.x, cr.y, cr.width, cr.height,
             offset.x, offset.y);
  }
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
  else if (aWidgetType == NS_THEME_MENUCHECKBOX || aWidgetType == NS_THEME_MENURADIO)
  {
      PRBool isChecked = PR_FALSE;
      isChecked = CheckBooleanAttr(aFrame, nsWidgetAtoms::checked);

      if (isChecked)
      {
        int bgState = MCB_NORMAL;
        PRBool isDisabled = IsDisabled(aFrame);

        // the disabled states are offset by 1
        if (isDisabled)
          bgState += 1;

        SIZE checkboxSize(GetCheckboxSize(theme,hdc));

        RECT checkRect = widgetRect;
        checkRect.right = checkRect.left+checkboxSize.cx;

        // Center the checkbox vertically in the menuitem
        checkRect.top += (checkRect.bottom - checkRect.top)/2 - checkboxSize.cy/2;
        checkRect.bottom = checkRect.top + checkboxSize.cy;

        drawThemeBG(theme, hdc, MENU_POPUPCHECKBACKGROUND, bgState, &checkRect, &clipRect);
        drawThemeBG(theme, hdc, MENU_POPUPCHECK, state, &checkRect, &clipRect);
      }
  }
  else if (aWidgetType == NS_THEME_MENUPOPUP)
  {
    drawThemeBG(theme, hdc, MENU_POPUPBORDERS, /* state */ 0, &widgetRect, &clipRect);
    SIZE borderSize;
    getThemePartSize(theme, hdc, MENU_POPUPBORDERS, 0, NULL, TS_TRUE, &borderSize);

    RECT bgRect = widgetRect;
    bgRect.top += borderSize.cy;
    bgRect.bottom -= borderSize.cy;
    bgRect.left += borderSize.cx;
    bgRect.right -= borderSize.cx;

    drawThemeBG(theme, hdc, MENU_POPUPBACKGROUND, /* state */ 0, &bgRect, &clipRect);

    SIZE gutterSize(GetGutterSize(theme, hdc));

    RECT gutterRect;
    gutterRect.top = bgRect.top;
    gutterRect.bottom = bgRect.bottom;
    gutterRect.left = bgRect.left;
    gutterRect.right = gutterRect.left+gutterSize.cx;
    drawThemeBG(theme, hdc, MENU_POPUPGUTTER, /* state */ 0, &gutterRect, &clipRect);
  }
  else if (aWidgetType == NS_THEME_MENUSEPARATOR)
  {
    SIZE gutterSize(GetGutterSize(theme,hdc));

    RECT sepRect = widgetRect;
    sepRect.left += gutterSize.cx;
    
    drawThemeBG(theme, hdc, MENU_POPUPSEPARATOR, /* state */ 0, &sepRect, &clipRect);
  }
  // If part is negative, the element wishes us to not render a themed
  // background, instead opting to be drawn specially below.
  else if (part >= 0) {
    drawThemeBG(theme, hdc, part, state, &widgetRect, &clipRect);
  }

  // Draw focus rectangles for XP HTML checkboxes and radio buttons
  // XXX it'd be nice to draw these outside of the frame
  if ((IsCheckboxWidgetType(aWidgetType) || IsRadioWidgetType(aWidgetType)) &&
      aFrame->GetContent()->IsNodeOfType(nsINode::eHTML) ||
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
  else if (aWidgetType == NS_THEME_TOOLBAR && state == 0) {
    // Draw toolbar separator lines above all toolbars except the first one.
    // The lines are part of the Rebar theme, which is loaded for NS_THEME_TOOLBOX.
    theme = GetTheme(NS_THEME_TOOLBOX);
    if (!theme)
      return NS_ERROR_FAILURE;

    widgetRect.bottom = widgetRect.top + TB_SEPARATOR_HEIGHT;
    drawThemeEdge(theme, hdc, RP_BAND, 0, &widgetRect, EDGE_ETCHED, BF_TOP, NULL);
  }

  nativeDrawing.EndNativeDrawing();

  if (nativeDrawing.ShouldRenderAgain())
    goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

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
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL ||
      aWidgetType == NS_THEME_MENUITEM || aWidgetType == NS_THEME_CHECKMENUITEM ||
      aWidgetType == NS_THEME_RADIOMENUITEM || aWidgetType == NS_THEME_MENUPOPUP ||
      aWidgetType == NS_THEME_MENUIMAGE || aWidgetType == NS_THEME_MENUITEMTEXT ||
      aWidgetType == NS_THEME_TOOLBAR_SEPARATOR)
    return NS_OK; // Don't worry about it.

  if (!getThemeContentRect)
    return NS_ERROR_FAILURE;

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  if (aWidgetType == NS_THEME_TOOLBAR) {
    // make space for the separator line above all toolbars but the first
    if (state == 0)
      aResult->top = TB_SEPARATOR_HEIGHT;
    return NS_OK;
  }

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

  if (aFrame && (aWidgetType == NS_THEME_TEXTFIELD || aWidgetType == NS_THEME_TEXTFIELD_MULTILINE)) {
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
  HANDLE theme = GetTheme(aWidgetType);
  if (!theme && aWidgetType != NS_THEME_MENUITEMTEXT)
    return PR_FALSE;

  if (aWidgetType == NS_THEME_MENUPOPUP)
  {
    SIZE popupSize;
    getThemePartSize(theme, NULL, MENU_POPUPBORDERS, /* state */ 0, NULL, TS_TRUE, &popupSize);
    aResult->top = aResult->bottom = popupSize.cy;
    aResult->left = aResult->right = popupSize.cx;
    return PR_TRUE;
  }

  PRInt32 right, left, top, bottom;
  right = left = top = bottom = 0;
  switch (aWidgetType)
  {
    case NS_THEME_MENUIMAGE:
        right = 9;
        left = 3;
        break;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
        right = 8;
        left = 0;
        break;
    case NS_THEME_MENUITEMTEXT:
        if (!theme)
        {
          left = 18;
        }
        else
        {
          // There seem to be exactly 4 pixels from the edge
          // of the gutter to the text
          SIZE size(GetGutterSize(theme, NULL));
          left = size.cx + 4;
        }
        break;
    case NS_THEME_MENUSEPARATOR:
        {
          SIZE size(GetGutterSize(theme, NULL));
          left = size.cx + 5;
          top = 10;
          bottom = 7;
        }
        break;
    default:
        return PR_FALSE;
  }

  if (IsFrameRTL(aFrame))
  {
    aResult->right = left;
    aResult->left = right;
  }
  else
  {
    aResult->right = right;
    aResult->left = left;
  }
  
  return PR_TRUE;
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
      aWidgetType == NS_THEME_LISTBOX || aWidgetType == NS_THEME_TREEVIEW || aWidgetType == NS_THEME_MENUITEMTEXT)
    return NS_OK; // Don't worry about it.

  if (aWidgetType == NS_THEME_MENUITEM && IsTopLevelMenu(aFrame))
      return NS_OK; // Don't worry about it for top level menus

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
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      if(!IsTopLevelMenu(aFrame))
      {
        SIZE gutterSize(GetGutterSize(theme, NULL));
        aResult->width = gutterSize.cx;
        aResult->height = gutterSize.cy;
        return NS_OK;
      }
      break;
    case NS_THEME_MENUIMAGE:
      aResult->width = 1;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      {
        SIZE boxSize(GetGutterSize(theme, NULL));
        aResult->width += boxSize.cx+2;
        aResult->height = boxSize.cy;
        *aIsOverridable = PR_FALSE;
      }
    case NS_THEME_MENUITEMTEXT:
      return NS_OK;
    case NS_THEME_MENUARROW:
      aResult->width = 26;
      aResult->height = 16;
      return NS_OK;
  }

  if (aWidgetType == NS_THEME_SCALE_THUMB_HORIZONTAL ||
      aWidgetType == NS_THEME_SCALE_THUMB_VERTICAL) {
      *aIsOverridable = PR_FALSE;
  }
  else if (aWidgetType == NS_THEME_TOOLBAR_SEPARATOR) {
    // that's 2px left margin, 2px right margin and 2px separator
    // (the margin is drawn as part of the separator, though)
    aResult->width = 6;
    return NS_OK;
  }

  PRInt32 part, state;
  nsresult rv = GetThemePartAndState(aFrame, aWidgetType, part, state);
  if (NS_FAILED(rv))
    return rv;

  HDC hdc = (HDC)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
  if (!hdc)
    return NS_ERROR_FAILURE;

  PRInt32 sizeReq = 1; // Best-fit size. (TS_TRUE)
  if (aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL)
    sizeReq = 0; // Best-fit size for progress meters is too large for most 
                 // themes.
                 // In our app, we want these widgets to be able to really shrink down,
                 // so use the min-size request value (of 0).

  // We should let HTML buttons shrink to their min size.
  // FIXME bug 403934: We should probably really separate
  // GetPreferredWidgetSize from GetMinimumWidgetSize, so callers can
  // use the one they want.
  if (aWidgetType == NS_THEME_BUTTON &&
      aFrame->GetContent()->IsNodeOfType(nsINode::eHTML))
    sizeReq = 0; /* TS_MIN */

  SIZE sz;
  getThemePartSize(theme, hdc, part, state, NULL, sizeReq, &sz);
  aResult->width = sz.cx;
  aResult->height = sz.cy;

  if (aWidgetType == NS_THEME_SPINNER_UP_BUTTON ||
      aWidgetType == NS_THEME_SPINNER_DOWN_BUTTON) {
    aResult->width++;
    aResult->height = aResult->height / 2 + 1;
  }
  else if (aWidgetType == NS_THEME_MENUSEPARATOR)
  {
    SIZE gutterSize(GetGutterSize(theme,hdc));
    aResult->width += gutterSize.cx;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeWin::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBAR_PANEL ||
      aWidgetType == NS_THEME_STATUSBAR_RESIZER_PANEL ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_TAB_PANELS ||
      aWidgetType == NS_THEME_TAB_PANEL ||
      aWidgetType == NS_THEME_TOOLBAR_SEPARATOR) {
    *aShouldRepaint = PR_FALSE;
    return NS_OK;
  }

  // On Vista, the scrollbar buttons need to change state when the track has/doesn't have hover
  if (GetWindowsVersion() < VISTA_VERSION &&
      (aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL || 
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL)) {
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
  if (mMenuTheme) {
    closeTheme(mMenuTheme);
    mMenuTheme = NULL;
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
  if (gfxPlatform::UseGlitz())
    return PR_FALSE;
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
      IsRadioWidgetType(aWidgetType) ||
      IsCheckboxWidgetType(aWidgetType))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsNativeThemeWin::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType)
{
  return PR_FALSE;
}

PRBool
nsNativeThemeWin::ThemeNeedsComboboxDropmarker()
{
  return PR_TRUE;
}

/* Windows 9x/NT/2000/Classic XP Theme Support */

PRBool 
nsNativeThemeWin::ClassicThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
      // Classic non-flat menus are handled almost entirely through CSS.
      if (!mFlatMenus)
        return PR_FALSE;
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
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
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MENUITEMTEXT:
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
    case NS_THEME_BUTTON:
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 2; 
      break;
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
    case NS_THEME_TEXTFIELD_MULTILINE:
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
      (*aResult).top = (*aResult).left = (*aResult).bottom = (*aResult).right = 3;
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM: {
      PRInt32 part, state;
      PRBool focused;
      nsresult rv;

      rv = ClassicGetThemePartAndState(aFrame, aWidgetType, part, state, focused);
      if (NS_FAILED(rv))
        return rv;

      if (part == 1) { // top level menu
        if (mFlatMenus || !(state & DFCS_PUSHED)) {
          (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 2;
        }
        else {
          // make top-level menus look sunken when pushed in the Classic look
          (*aResult).top = (*aResult).left = 3;
          (*aResult).bottom = (*aResult).right = 1;
        }
      }
      else {
        (*aResult).top = 1;
        (*aResult).bottom = 3;
        (*aResult).left = (*aResult).right = 2;
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
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
      (*aResult).width = (*aResult).height = 13;
      break;
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
      (*aResult).width = ::GetSystemMetrics(SM_CXMENUCHECK);
      (*aResult).height = ::GetSystemMetrics(SM_CYMENUCHECK);
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
    case NS_THEME_TEXTFIELD_MULTILINE:
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
    case NS_THEME_MENUSEPARATOR:
    {
      aResult->width = 0;
      aResult->height = 10;
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
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL: {
      PRInt32 contentState ;
      aFocused = PR_FALSE;

      aPart = DFC_BUTTON;
      aState = (IsCheckboxWidgetType(aWidgetType)) ? DFCS_BUTTONCHECK : DFCS_BUTTONRADIO;
      nsIContent* content = aFrame->GetContent();
           
      if (content->IsNodeOfType(nsINode::eXUL)) {
        // XUL
        if (IsCheckboxWidgetType(aWidgetType)) {
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
        isTopLevel = menuFrame->IsOnMenuBar();
        isOpen = menuFrame->IsOpen();
        isContainer = menuFrame->IsMenu();
      }

      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;

      if (isTopLevel) {
        aPart = 1;
        if (isOpen)
          aState |= DFCS_PUSHED;
      }

      if (CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive))
        aState |= DFCS_HOT;

      return NS_OK;
    }
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
      aState = 0;
      if (IsDisabled(aFrame))
        aState |= DFCS_INACTIVE;
      if (CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive))
        aState |= DFCS_HOT;
      if (aWidgetType == NS_THEME_MENUCHECKBOX || aWidgetType == NS_THEME_MENURADIO) {
        if (IsCheckedButton(aFrame))
          aState |= DFCS_CHECKED;
      } else {
        if (aFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
          aState |= DFCS_RTL;
      }
      return NS_OK;
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
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
      if (parentFrame->GetType() == nsWidgetAtoms::menuFrame ||
          (content && content->IsNodeOfType(nsINode::eHTML)))
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
    case NS_THEME_MENUSEPARATOR:
      aPart = 0;
      aState = 0;
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

        // XXXzeniko Windows renders these 1px lower than you'd expect
        if (aComponent == DFCS_MENUCHECK || aComponent == DFCS_MENUBULLET)
          imgPos.y++;

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
  gfxFloat p2a = gfxFloat(dc->AppUnitsPerDevPixel());
  RECT widgetRect;
  gfxRect tr(aRect.x, aRect.y, aRect.width, aRect.height),
          cr(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

  tr.ScaleInverse(p2a);
  cr.ScaleInverse(p2a);

  nsRefPtr<gfxContext> ctx = (gfxContext*)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);

  gfxWindowsNativeDrawing nativeDrawing(ctx, cr, GetWidgetNativeDrawingFlags(aWidgetType));

RENDER_AGAIN:

  HDC hdc = nativeDrawing.BeginNativeDrawing();
  if (!hdc)
    return NS_ERROR_FAILURE;

  nativeDrawing.TransformToNativeRect(tr, widgetRect);

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
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
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
      if (focused && (IsCheckboxWidgetType(aWidgetType) || IsRadioWidgetType(aWidgetType))) {
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
    case NS_THEME_TEXTFIELD_MULTILINE:
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
      ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_WINDOWFRAME));
      InflateRect(&widgetRect, -1, -1);
      ::FillRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_INFOBK));

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
      NS_ASSERTION(mFlatMenus, "Classic menus are styled entirely through CSS");
      ::FillRect(hdc, &widgetRect, (HBRUSH) (COLOR_MENU+1));
      ::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_BTNSHADOW));
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
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
      break;
#ifndef WINCE
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
      if (!(state & DFCS_CHECKED))
        break; // nothin' to do
    case NS_THEME_MENUARROW: {
      PRUint32 color = COLOR_MENUTEXT;
      if ((state & DFCS_INACTIVE))
        color = COLOR_GRAYTEXT;
      else if ((state & DFCS_HOT))
        color = COLOR_HIGHLIGHTTEXT;
      
      if (aWidgetType == NS_THEME_MENUCHECKBOX)
        DrawMenuImage(hdc, widgetRect, DFCS_MENUCHECK, color);
      else if (aWidgetType == NS_THEME_MENURADIO)
        DrawMenuImage(hdc, widgetRect, DFCS_MENUBULLET, color);
      else if (aWidgetType == NS_THEME_MENUARROW)
        DrawMenuImage(hdc, widgetRect, 
                      (state & DFCS_RTL) ? DFCS_MENUARROWRIGHT : DFCS_MENUARROW,
                      color);
      break;
    }
    case NS_THEME_MENUSEPARATOR: {
      // separators are offset by a bit (see menu.css)
      widgetRect.left++;
      widgetRect.right--;

      // This magic number is brought to you by the value in menu.css
      widgetRect.top += 4;
      // Our rectangles are 1 pixel high (see border size in menu.css)
      widgetRect.bottom = widgetRect.top+1;
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_3DSHADOW+1));
      widgetRect.top++;
      widgetRect.bottom++;
      ::FillRect(hdc, &widgetRect, (HBRUSH)(COLOR_3DHILIGHT+1));
      break;
    }
#endif
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

  nativeDrawing.EndNativeDrawing();

  if (NS_FAILED(rv))
    return rv;

  if (nativeDrawing.ShouldRenderAgain())
    goto RENDER_AGAIN;

  nativeDrawing.PaintToContext();

  return rv;
}

PRUint32
nsNativeThemeWin::GetWidgetNativeDrawingFlags(PRUint8 aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXTFIELD:
      return
        gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
        gfxWindowsNativeDrawing::CAN_AXIS_ALIGNED_SCALE |
        gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;

    // need to check these others
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
    case NS_THEME_SPINNER_UP_BUTTON:
    case NS_THEME_SPINNER_DOWN_BUTTON:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
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
      break;

    // the dropdown button /almost/ renders correctly with scaling,
    // except that the graphic in the dropdown button (the downward arrow)
    // doesn't get scaled up.
    case NS_THEME_DROPDOWN_BUTTON:
    // these are definitely no; they're all graphics that don't get scaled up
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUCHECKBOX:
    case NS_THEME_MENURADIO:
    case NS_THEME_MENUARROW:
      return
        gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
        gfxWindowsNativeDrawing::CANNOT_AXIS_ALIGNED_SCALE |
        gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;
  }

  return
    gfxWindowsNativeDrawing::CANNOT_DRAW_TO_COLOR_ALPHA |
    gfxWindowsNativeDrawing::CANNOT_AXIS_ALIGNED_SCALE |
    gfxWindowsNativeDrawing::CANNOT_COMPLEX_TRANSFORM;
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
