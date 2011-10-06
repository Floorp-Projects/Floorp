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
 *   Jim Mathies <jmathies@mozilla.com>.
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

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "nsWindowDbg.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT) || 1

typedef struct {
  char * mStr;
  long   mId;
} EventMsgInfo;

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
MSGFEventMsgInfo gMSGFEvents[] = {
  "MSGF_DIALOGBOX",      0,
  "MSGF_MESSAGEBOX",     1,
  "MSGF_MENU",           2,
  "MSGF_SCROLLBAR",      5,
  "MSGF_NEXTWINDOW",     6,
  "MSGF_MAX",            8,
  "MSGF_USER",           4096,
  NULL, 0};
#endif

EventMsgInfo gAllEvents[] = {
  {"WM_NULL",                             0x0000},
  {"WM_CREATE",                           0x0001},
  {"WM_DESTROY",                          0x0002},
  {"WM_MOVE",                             0x0003},
  {"WM_SIZE",                             0x0005},
  {"WM_ACTIVATE",                         0x0006},
  {"WM_SETFOCUS",                         0x0007},
  {"WM_KILLFOCUS",                        0x0008},
  {"WM_ENABLE",                           0x000A},
  {"WM_SETREDRAW",                        0x000B},
  {"WM_SETTEXT",                          0x000C},
  {"WM_GETTEXT",                          0x000D},
  {"WM_GETTEXTLENGTH",                    0x000E},
  {"WM_PAINT",                            0x000F},
  {"WM_CLOSE",                            0x0010},
  {"WM_QUERYENDSESSION",                  0x0011},
  {"WM_QUIT",                             0x0012},
  {"WM_QUERYOPEN",                        0x0013},
  {"WM_ERASEBKGND",                       0x0014},
  {"WM_SYSCOLORCHANGE",                   0x0015},
  {"WM_ENDSESSION",                       0x0016},
  {"WM_SHOWWINDOW",                       0x0018},
  {"WM_SETTINGCHANGE",                    0x001A},
  {"WM_DEVMODECHANGE",                    0x001B},
  {"WM_ACTIVATEAPP",                      0x001C},
  {"WM_FONTCHANGE",                       0x001D},
  {"WM_TIMECHANGE",                       0x001E},
  {"WM_CANCELMODE",                       0x001F},
  {"WM_SETCURSOR",                        0x0020},
  {"WM_MOUSEACTIVATE",                    0x0021},
  {"WM_CHILDACTIVATE",                    0x0022},
  {"WM_QUEUESYNC",                        0x0023},
  {"WM_GETMINMAXINFO",                    0x0024},
  {"WM_PAINTICON",                        0x0026},
  {"WM_ICONERASEBKGND",                   0x0027},
  {"WM_NEXTDLGCTL",                       0x0028},
  {"WM_SPOOLERSTATUS",                    0x002A},
  {"WM_DRAWITEM",                         0x002B},
  {"WM_MEASUREITEM",                      0x002C},
  {"WM_DELETEITEM",                       0x002D},
  {"WM_VKEYTOITEM",                       0x002E},
  {"WM_CHARTOITEM",                       0x002F},
  {"WM_SETFONT",                          0x0030},
  {"WM_GETFONT",                          0x0031},
  {"WM_SETHOTKEY",                        0x0032},
  {"WM_GETHOTKEY",                        0x0033},
  {"WM_QUERYDRAGICON",                    0x0037},
  {"WM_COMPAREITEM",                      0x0039},
  {"WM_GETOBJECT",                        0x003D},
  {"WM_COMPACTING",                       0x0041},
  {"WM_COMMNOTIFY",                       0x0044},
  {"WM_WINDOWPOSCHANGING",                0x0046},
  {"WM_WINDOWPOSCHANGED",                 0x0047},
  {"WM_POWER",                            0x0048},
  {"WM_COPYDATA",                         0x004A},
  {"WM_CANCELJOURNAL",                    0x004B},
  {"WM_NOTIFY",                           0x004E},
  {"WM_INPUTLANGCHANGEREQUEST",           0x0050},
  {"WM_INPUTLANGCHANGE",                  0x0051},
  {"WM_TCARD",                            0x0052},
  {"WM_HELP",                             0x0053},
  {"WM_USERCHANGED",                      0x0054},
  {"WM_NOTIFYFORMAT",                     0x0055},
  {"WM_CONTEXTMENU",                      0x007B},
  {"WM_STYLECHANGING",                    0x007C},
  {"WM_STYLECHANGED",                     0x007D},
  {"WM_DISPLAYCHANGE",                    0x007E},
  {"WM_GETICON",                          0x007F},
  {"WM_SETICON",                          0x0080},
  {"WM_NCCREATE",                         0x0081},
  {"WM_NCDESTROY",                        0x0082},
  {"WM_NCCALCSIZE",                       0x0083},
  {"WM_NCHITTEST",                        0x0084},
  {"WM_NCPAINT",                          0x0085},
  {"WM_NCACTIVATE",                       0x0086},
  {"WM_GETDLGCODE",                       0x0087},
  {"WM_SYNCPAINT",                        0x0088},
  {"WM_NCMOUSEMOVE",                      0x00A0},
  {"WM_NCLBUTTONDOWN",                    0x00A1},
  {"WM_NCLBUTTONUP",                      0x00A2},
  {"WM_NCLBUTTONDBLCLK",                  0x00A3},
  {"WM_NCRBUTTONDOWN",                    0x00A4},
  {"WM_NCRBUTTONUP",                      0x00A5},
  {"WM_NCRBUTTONDBLCLK",                  0x00A6},
  {"WM_NCMBUTTONDOWN",                    0x00A7},
  {"WM_NCMBUTTONUP",                      0x00A8},
  {"WM_NCMBUTTONDBLCLK",                  0x00A9},
  {"EM_GETSEL",                           0x00B0},
  {"EM_SETSEL",                           0x00B1},
  {"EM_GETRECT",                          0x00B2},
  {"EM_SETRECT",                          0x00B3},
  {"EM_SETRECTNP",                        0x00B4},
  {"EM_SCROLL",                           0x00B5},
  {"EM_LINESCROLL",                       0x00B6},
  {"EM_SCROLLCARET",                      0x00B7},
  {"EM_GETMODIFY",                        0x00B8},
  {"EM_SETMODIFY",                        0x00B9},
  {"EM_GETLINECOUNT",                     0x00BA},
  {"EM_LINEINDEX",                        0x00BB},
  {"EM_SETHANDLE",                        0x00BC},
  {"EM_GETHANDLE",                        0x00BD},
  {"EM_GETTHUMB",                         0x00BE},
  {"EM_LINELENGTH",                       0x00C1},
  {"EM_REPLACESEL",                       0x00C2},
  {"EM_GETLINE",                          0x00C4},
  {"EM_LIMITTEXT",                        0x00C5},
  {"EM_CANUNDO",                          0x00C6},
  {"EM_UNDO",                             0x00C7},
  {"EM_FMTLINES",                         0x00C8},
  {"EM_LINEFROMCHAR",                     0x00C9},
  {"EM_SETTABSTOPS",                      0x00CB},
  {"EM_SETPASSWORDCHAR",                  0x00CC},
  {"EM_EMPTYUNDOBUFFER",                  0x00CD},
  {"EM_GETFIRSTVISIBLELINE",              0x00CE},
  {"EM_SETREADONLY",                      0x00CF},
  {"EM_SETWORDBREAKPROC",                 0x00D0},
  {"EM_GETWORDBREAKPROC",                 0x00D1},
  {"EM_GETPASSWORDCHAR",                  0x00D2},
  {"EM_SETMARGINS",                       0x00D3},
  {"EM_GETMARGINS",                       0x00D4},
  {"EM_GETLIMITTEXT",                     0x00D5},
  {"EM_POSFROMCHAR",                      0x00D6},
  {"EM_CHARFROMPOS",                      0x00D7},
  {"EM_SETIMESTATUS",                     0x00D8},
  {"EM_GETIMESTATUS",                     0x00D9},
  {"SBM_SETPOS",                          0x00E0},
  {"SBM_GETPOS",                          0x00E1},
  {"SBM_SETRANGE",                        0x00E2},
  {"SBM_SETRANGEREDRAW",                  0x00E6},
  {"SBM_GETRANGE",                        0x00E3},
  {"SBM_ENABLE_ARROWS",                   0x00E4},
  {"SBM_SETSCROLLINFO",                   0x00E9},
  {"SBM_GETSCROLLINFO",                   0x00EA},
  {"WM_KEYDOWN",                          0x0100},
  {"WM_KEYUP",                            0x0101},
  {"WM_CHAR",                             0x0102},
  {"WM_DEADCHAR",                         0x0103},
  {"WM_SYSKEYDOWN",                       0x0104},
  {"WM_SYSKEYUP",                         0x0105},
  {"WM_SYSCHAR",                          0x0106},
  {"WM_SYSDEADCHAR",                      0x0107},
  {"WM_KEYLAST",                          0x0108},
  {"WM_IME_STARTCOMPOSITION",             0x010D},
  {"WM_IME_ENDCOMPOSITION",               0x010E},
  {"WM_IME_COMPOSITION",                  0x010F},
  {"WM_INITDIALOG",                       0x0110},
  {"WM_COMMAND",                          0x0111},
  {"WM_SYSCOMMAND",                       0x0112},
  {"WM_TIMER",                            0x0113},
  {"WM_HSCROLL",                          0x0114},
  {"WM_VSCROLL",                          0x0115},
  {"WM_INITMENU",                         0x0116},
  {"WM_INITMENUPOPUP",                    0x0117},
  {"WM_MENUSELECT",                       0x011F},
  {"WM_MENUCHAR",                         0x0120},
  {"WM_ENTERIDLE",                        0x0121},
  {"WM_MENURBUTTONUP",                    0x0122},
  {"WM_MENUDRAG",                         0x0123},
  {"WM_MENUGETOBJECT",                    0x0124},
  {"WM_UNINITMENUPOPUP",                  0x0125},
  {"WM_MENUCOMMAND",                      0x0126},
  {"WM_CHANGEUISTATE",                    0x0127},
  {"WM_UPDATEUISTATE",                    0x0128},
  {"WM_CTLCOLORMSGBOX",                   0x0132},
  {"WM_CTLCOLOREDIT",                     0x0133},
  {"WM_CTLCOLORLISTBOX",                  0x0134},
  {"WM_CTLCOLORBTN",                      0x0135},
  {"WM_CTLCOLORDLG",                      0x0136},
  {"WM_CTLCOLORSCROLLBAR",                0x0137},
  {"WM_CTLCOLORSTATIC",                   0x0138},
  {"CB_GETEDITSEL",                       0x0140},
  {"CB_LIMITTEXT",                        0x0141},
  {"CB_SETEDITSEL",                       0x0142},
  {"CB_ADDSTRING",                        0x0143},
  {"CB_DELETESTRING",                     0x0144},
  {"CB_DIR",                              0x0145},
  {"CB_GETCOUNT",                         0x0146},
  {"CB_GETCURSEL",                        0x0147},
  {"CB_GETLBTEXT",                        0x0148},
  {"CB_GETLBTEXTLEN",                     0x0149},
  {"CB_INSERTSTRING",                     0x014A},
  {"CB_RESETCONTENT",                     0x014B},
  {"CB_FINDSTRING",                       0x014C},
  {"CB_SELECTSTRING",                     0x014D},
  {"CB_SETCURSEL",                        0x014E},
  {"CB_SHOWDROPDOWN",                     0x014F},
  {"CB_GETITEMDATA",                      0x0150},
  {"CB_SETITEMDATA",                      0x0151},
  {"CB_GETDROPPEDCONTROLRECT",            0x0152},
  {"CB_SETITEMHEIGHT",                    0x0153},
  {"CB_GETITEMHEIGHT",                    0x0154},
  {"CB_SETEXTENDEDUI",                    0x0155},
  {"CB_GETEXTENDEDUI",                    0x0156},
  {"CB_GETDROPPEDSTATE",                  0x0157},
  {"CB_FINDSTRINGEXACT",                  0x0158},
  {"CB_SETLOCALE",                        0x0159},
  {"CB_GETLOCALE",                        0x015A},
  {"CB_GETTOPINDEX",                      0x015b},
  {"CB_SETTOPINDEX",                      0x015c},
  {"CB_GETHORIZONTALEXTENT",              0x015d},
  {"CB_SETHORIZONTALEXTENT",              0x015e},
  {"CB_GETDROPPEDWIDTH",                  0x015f},
  {"CB_SETDROPPEDWIDTH",                  0x0160},
  {"CB_INITSTORAGE",                      0x0161},
  {"CB_MSGMAX",                           0x0162},
  {"LB_ADDSTRING",                        0x0180},
  {"LB_INSERTSTRING",                     0x0181},
  {"LB_DELETESTRING",                     0x0182},
  {"LB_SELITEMRANGEEX",                   0x0183},
  {"LB_RESETCONTENT",                     0x0184},
  {"LB_SETSEL",                           0x0185},
  {"LB_SETCURSEL",                        0x0186},
  {"LB_GETSEL",                           0x0187},
  {"LB_GETCURSEL",                        0x0188},
  {"LB_GETTEXT",                          0x0189},
  {"LB_GETTEXTLEN",                       0x018A},
  {"LB_GETCOUNT",                         0x018B},
  {"LB_SELECTSTRING",                     0x018C},
  {"LB_DIR",                              0x018D},
  {"LB_GETTOPINDEX",                      0x018E},
  {"LB_FINDSTRING",                       0x018F},
  {"LB_GETSELCOUNT",                      0x0190},
  {"LB_GETSELITEMS",                      0x0191},
  {"LB_SETTABSTOPS",                      0x0192},
  {"LB_GETHORIZONTALEXTENT",              0x0193},
  {"LB_SETHORIZONTALEXTENT",              0x0194},
  {"LB_SETCOLUMNWIDTH",                   0x0195},
  {"LB_ADDFILE",                          0x0196},
  {"LB_SETTOPINDEX",                      0x0197},
  {"LB_GETITEMRECT",                      0x0198},
  {"LB_GETITEMDATA",                      0x0199},
  {"LB_SETITEMDATA",                      0x019A},
  {"LB_SELITEMRANGE",                     0x019B},
  {"LB_SETANCHORINDEX",                   0x019C},
  {"LB_GETANCHORINDEX",                   0x019D},
  {"LB_SETCARETINDEX",                    0x019E},
  {"LB_GETCARETINDEX",                    0x019F},
  {"LB_SETITEMHEIGHT",                    0x01A0},
  {"LB_GETITEMHEIGHT",                    0x01A1},
  {"LB_FINDSTRINGEXACT",                  0x01A2},
  {"LB_SETLOCALE",                        0x01A5},
  {"LB_GETLOCALE",                        0x01A6},
  {"LB_SETCOUNT",                         0x01A7},
  {"LB_INITSTORAGE",                      0x01A8},
  {"LB_ITEMFROMPOINT",                    0x01A9},
  {"LB_MSGMAX",                           0x01B0},
  {"WM_MOUSEMOVE",                        0x0200},
  {"WM_LBUTTONDOWN",                      0x0201},
  {"WM_LBUTTONUP",                        0x0202},
  {"WM_LBUTTONDBLCLK",                    0x0203},
  {"WM_RBUTTONDOWN",                      0x0204},
  {"WM_RBUTTONUP",                        0x0205},
  {"WM_RBUTTONDBLCLK",                    0x0206},
  {"WM_MBUTTONDOWN",                      0x0207},
  {"WM_MBUTTONUP",                        0x0208},
  {"WM_MBUTTONDBLCLK",                    0x0209},
  {"WM_MOUSEWHEEL",                       0x020A},
  {"WM_MOUSEHWHEEL",                      0x020E},
  {"WM_PARENTNOTIFY",                     0x0210},
  {"WM_ENTERMENULOOP",                    0x0211},
  {"WM_EXITMENULOOP",                     0x0212},
  {"WM_NEXTMENU",                         0x0213},
  {"WM_SIZING",                           0x0214},
  {"WM_CAPTURECHANGED",                   0x0215},
  {"WM_MOVING",                           0x0216},
  {"WM_POWERBROADCAST",                   0x0218},
  {"WM_DEVICECHANGE",                     0x0219},
  {"WM_MDICREATE",                        0x0220},
  {"WM_MDIDESTROY",                       0x0221},
  {"WM_MDIACTIVATE",                      0x0222},
  {"WM_MDIRESTORE",                       0x0223},
  {"WM_MDINEXT",                          0x0224},
  {"WM_MDIMAXIMIZE",                      0x0225},
  {"WM_MDITILE",                          0x0226},
  {"WM_MDICASCADE",                       0x0227},
  {"WM_MDIICONARRANGE",                   0x0228},
  {"WM_MDIGETACTIVE",                     0x0229},
  {"WM_MDISETMENU",                       0x0230},
  {"WM_ENTERSIZEMOVE",                    0x0231},
  {"WM_EXITSIZEMOVE",                     0x0232},
  {"WM_DROPFILES",                        0x0233},
  {"WM_MDIREFRESHMENU",                   0x0234},
  {"WM_IME_SETCONTEXT",                   0x0281},
  {"WM_IME_NOTIFY",                       0x0282},
  {"WM_IME_CONTROL",                      0x0283},
  {"WM_IME_COMPOSITIONFULL",              0x0284},
  {"WM_IME_SELECT",                       0x0285},
  {"WM_IME_CHAR",                         0x0286},
  {"WM_IME_REQUEST",                      0x0288},
  {"WM_IME_KEYDOWN",                      0x0290},
  {"WM_IME_KEYUP",                        0x0291},
  {"WM_NCMOUSEHOVER",                     0x02A0},
  {"WM_MOUSEHOVER",                       0x02A1},
  {"WM_MOUSELEAVE",                       0x02A3},
  {"WM_CUT",                              0x0300},
  {"WM_COPY",                             0x0301},
  {"WM_PASTE",                            0x0302},
  {"WM_CLEAR",                            0x0303},
  {"WM_UNDO",                             0x0304},
  {"WM_RENDERFORMAT",                     0x0305},
  {"WM_RENDERALLFORMATS",                 0x0306},
  {"WM_DESTROYCLIPBOARD",                 0x0307},
  {"WM_DRAWCLIPBOARD",                    0x0308},
  {"WM_PAINTCLIPBOARD",                   0x0309},
  {"WM_VSCROLLCLIPBOARD",                 0x030A},
  {"WM_SIZECLIPBOARD",                    0x030B},
  {"WM_ASKCBFORMATNAME",                  0x030C},
  {"WM_CHANGECBCHAIN",                    0x030D},
  {"WM_HSCROLLCLIPBOARD",                 0x030E},
  {"WM_QUERYNEWPALETTE",                  0x030F},
  {"WM_PALETTEISCHANGING",                0x0310},
  {"WM_PALETTECHANGED",                   0x0311},
  {"WM_HOTKEY",                           0x0312},
  {"WM_PRINT",                            0x0317},
  {"WM_PRINTCLIENT",                      0x0318},
  {"WM_THEMECHANGED",                     0x031A},
  {"WM_HANDHELDFIRST",                    0x0358},
  {"WM_HANDHELDLAST",                     0x035F},
  {"WM_AFXFIRST",                         0x0360},
  {"WM_AFXLAST",                          0x037F},
  {"WM_PENWINFIRST",                      0x0380},
  {"WM_PENWINLAST",                       0x038F},
  {"WM_APP",                              0x8000},
  {"WM_DWMCOMPOSITIONCHANGED",            0x031E},
  {"WM_DWMNCRENDERINGCHANGED",            0x031F},
  {"WM_DWMCOLORIZATIONCOLORCHANGED",      0x0320},
  {"WM_DWMWINDOWMAXIMIZEDCHANGE",         0x0321},
  {"WM_DWMSENDICONICTHUMBNAIL",           0x0323},
  {"WM_DWMSENDICONICLIVEPREVIEWBITMAP",   0x0326},
  {"WM_TABLET_QUERYSYSTEMGESTURESTATUS",  0x02CC},
  {"WM_GESTURE",                          0x0119},
  {"WM_GESTURENOTIFY",                    0x011A},
  {"WM_GETTITLEBARINFOEX",                0x033F},
  {NULL, 0x0}
};

static long gEventCounter = 0;
static long gLastEventMsg = 0;

void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves)
{
  int inx = 0;
  while (gAllEvents[inx].mId != (long)msg && gAllEvents[inx].mStr != NULL) {
    inx++;
  }
  if (aShowAllEvents || (!aShowAllEvents && gLastEventMsg != (long)msg)) {
    if (aShowMouseMoves || (!aShowMouseMoves && msg != 0x0020 && msg != 0x0200 && msg != 0x0084)) {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
             ("%6d - 0x%04X %s\n", gEventCounter++, msg, 
              gAllEvents[inx].mStr ? gAllEvents[inx].mStr : "Unknown"));
      gLastEventMsg = msg;
    }
  }
}

#endif // defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT)

#ifdef DEBUG
void DDError(const char *msg, HRESULT hr)
{
  /*XXX make nicer */
  PR_LOG(gWindowsLog, PR_LOG_ERROR,
         ("direct draw error %s: 0x%08lx\n", msg, hr));
}
#endif

#ifdef DEBUG_VK
PRBool is_vk_down(int vk)
{
   SHORT st = GetKeyState(vk);
#ifdef DEBUG
   PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("is_vk_down vk=%x st=%x\n",vk, st));
#endif
   return (st < 0);
}
#endif
