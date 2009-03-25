/* vim: se cin sw=2 ts=2 et : */
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Arnold <robarnold@mozilla.com> (Original Author)
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

/* 
 * The following constants are used to determine how a widget is drawn using
 * Windows' Theme API. For more information on theme parts and states see
 * http://msdn.microsoft.com/en-us/library/bb773210(VS.85).aspx
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
#define TP_BUTTON 1
#define TP_SEPARATOR 5

// Toolbarbutton constants
#define TB_CHECKED       5
#define TB_HOVER_CHECKED 6

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3
#define BP_GROUPBOX  4

// Textfield constants
/* This is the EP_EDITTEXT part */
#define TFP_TEXTFIELD 1
#define TFP_EDITBORDER_NOSCROLL 6
#define TFS_READONLY  6

/* These are the state constants for the EDITBORDER parts */
#define TFS_EDITBORDER_NORMAL 1
#define TFS_EDITBORDER_HOVER 2
#define TFS_EDITBORDER_FOCUSED 3
#define TFS_EDITBORDER_DISABLED 4

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
#define CBP_DROPBORDER       4
/* This is actually the 'READONLY' style */
#define CBP_DROPFRAME        5
#define CBP_DROPMARKER_VISTA 6

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

// From tmschema.h in the Vista SDK
#define TMT_TEXTCOLOR 3803
#define TMT_SIZINGMARGINS 3601
#define TMT_CONTENTMARGINS 3602
#define TMT_CAPTIONMARGINS 3603

// Rebar constants
#define RP_BAND              3
#define RP_BACKGROUND        6

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
#ifndef SPI_GETMENUSHOWDELAY
#define SPI_GETMENUSHOWDELAY      106
#endif //SPI_GETMENUSHOWDELAY
#ifndef WS_EX_LAYOUTRTL 
#define WS_EX_LAYOUTRTL         0x00400000L // Right to left mirroring
#endif


// Our extra constants for passing a little bit more info to the renderer.
#define DFCS_RTL             0x00010000

// Toolbar separator dimension which can't be gotten from Windows
#define TB_SEPARATOR_HEIGHT  2

