/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nswidgetdefs_h
#define _nswidgetdefs_h

// OS/2 defines; for user messages & warp4 stuff, plus module-global data

#include "nsIWidget.h"
#include "nsVoidArray.h"

#define INCL_PM
#define INCL_NLS
#define INCL_DOS
#define INCL_WINSTDFILE
#define INCL_DOSERRORS
#include <os2.h>
#include <uconv.h>  // Rather not have to include these two, but need types...
#include <unikbd.h> // 

#include "nsModule.h"

#ifndef MAX_PATH
#define MAX_PATH CCHMAXPATH
#endif

#ifndef OPENFILENAME
/* OPENFILENAME struct flags
 */

#define OFN_READONLY                 0x00000001
#define OFN_OVERWRITEPROMPT          0x00000002
#define OFN_HIDEREADONLY             0x00000004
#define OFN_NOCHANGEDIR              0x00000008
#define OFN_SHOWHELP                 0x00000010
#define OFN_ENABLEHOOK               0x00000020
#define OFN_ENABLETEMPLATE           0x00000040
#define OFN_ENABLETEMPLATEHANDLE     0x00000080
#define OFN_NOVALIDATE               0x00000100
#define OFN_ALLOWMULTISELECT         0x00000200
#define OFN_EXTENSIONDIFFERENT       0x00000400
#define OFN_PATHMUSTEXIST            0x00000800
#define OFN_FILEMUSTEXIST            0x00001000
#define OFN_CREATEPROMPT             0x00002000
#define OFN_SHAREAWARE               0x00004000
#define OFN_NOREADONLYRETURN         0x00008000
#define OFN_NOTESTFILECREATE         0x00010000
#define OFN_NONETWORKBUTTON          0x00020000
#define OFN_NOLONGNAMES              0x00040000

typedef struct _tagOFN {
   ULONG                             lStructSize;
   HWND                              hwndOwner;
   HMODULE                           hInstance;
   PCSZ                              lpstrFilter;
   PSZ                               lpstrCustomFilter;
   ULONG                             nMaxCustFilter;
   ULONG                             nFilterIndex;
   PSZ                               lpstrFile;
   ULONG                             nMaxFile;
   PSZ                               lpstrFileTitle;
   ULONG                             nMaxFileTitle;
   PCSZ                              lpstrInitialDir;
   PCSZ                              lpstrTitle;
   ULONG                             Flags;
   USHORT                            nFileOffset;
   USHORT                            nFileExtension;
   PCSZ                              lpstrDefExt;
   ULONG                             lCustData;
   PFN                               lpfnHook;
   PCSZ                              lpTemplateName;
} OPENFILENAME, *POPENFILENAME, *LPOPENFILENAME;

extern "C" BOOL APIENTRY DaxOpenSave(BOOL, LONG *, LPOPENFILENAME, PFNWP);
#endif

#define SUPPORT_NON_XPFE /* support for viewer.exe */

class nsDragService;
class nsIAppShell;

extern nsWidgetModuleData *gWidgetModuleData;

// messages - here to avoid duplication
#define WMU_CALLMETHOD   (WM_USER + 1)
#define WMU_SENDMSG      (WM_USER + 2)

// MP1 is "LONG lIndex", MP2 is reserved
#define WMU_SHOW_TOOLTIP (WM_USER + 3)

// MP1 & MP2 both reserved
#define WMU_HIDE_TOOLTIP (WM_USER + 4)

// DRM_MOZILLA messages

// WMU_GETFLAVOURLEN
//
// mp1 - ULONG ulItemID       Item ID from DRAGITEM
// mp2 - HATOM hAtomFlavour   Atom in the system table for the data flavour
//
// returns - ULONG ulSize     Size in bytes of transfer data, 0 for error
//
#define WMU_GETFLAVOURLEN  (WM_USER + 5)

typedef struct _WZDROPXFER
{
   ATOM hAtomFlavour;
   CHAR data[1];
} WZDROPXFER, *PWZDROPXFER;

// WMU_GETFLAVOURDATA
//
// mp1 - ULONG       ulItemID     Item ID from DRAGITEM
// mp2 - PWZDROPXFER pvData       Pointer to buffer to put data.
//                                Must DosFreeMem().
//
// returns - BOOL bSuccess    TRUE ok, FALSE error occurred.

#define WMU_GETFLAVOURDATA (WM_USER + 6)

#define WinIsKeyDown(vk) ((WinGetKeyState(HWND_DESKTOP,vk) & 0x8000) ? PR_TRUE : PR_FALSE)

// Tab control uses messages from TABM_BASE, which is currently (WM_USER+50).
// See tabapi.h for details.

// MP2 is the other window.
#ifndef WM_MOUSEENTER
#define WM_MOUSEENTER   0x041E
#endif
#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE   0x041F
#endif

#ifndef WM_FOCUSCHANGED
#define WM_FOCUSCHANGED 0x000E
#endif

#ifndef FCF_CLOSEBUTTON // defined in the Merlin toolkit
#define FCF_CLOSEBUTTON 0x04000000L
#endif

#ifndef FCF_DIALOGBOX
#define FCF_DIALOGBOX   0x40000000L
#endif

#ifndef DRT_URL
#define DRT_URL "UniformResourceLocator"
#endif

#define BASE_CONTROL_STYLE WS_TABSTOP

#define NS_MIT_END ((const PRUint32) MIT_END)

// A common pattern in widget is the 'Get/SetLabel' methods.  These macros
// can be used to fill in implementations who derive from nsWindow and just
// want to call WinSet/QueryWindowText.

#define NS_DECL_LABEL                           \
   NS_IMETHOD SetLabel( const nsString &aText); \
   NS_IMETHOD GetLabel( nsString &aBuffer);

#define NS_IMPL_LABEL(_clsname)                        \
   nsresult _clsname::SetLabel( const nsString &aText) \
   {                                                   \
      SetTitle( aText);                                \
      return NS_OK;                                    \
   }                                                   \
                                                       \
   nsresult _clsname::GetLabel( nsString &aBuffer)     \
   {                                                   \
      PRUint32 dummy;                                  \
      GetWindowText( aBuffer, &dummy);                 \
      return NS_OK;                                    \
   }

// can be used as an lvalue too.
#define lastchar(s) *((s) + strlen((s)) - 1)

#endif
