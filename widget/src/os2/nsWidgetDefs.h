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
#include <os2.h>
#include <uconv.h>  // Rather not have to include these two, but need types...
#include <unikbd.h> // 

#define SUPPORT_NON_XPFE /* support for viewer.exe */

class nsIFontRetrieverService;
class nsDragService;
class nsIAppShell;

// Module data -- anything that would be static, should be module-visible,
//                or any magic constants.
class nsWidgetModuleData
{
 public:
   HMODULE hModResources;   // resource module
   SIZEL   szScreen;        // size of screen in pixels
   BOOL    bMouseSwitched;  // true if MB1 is the RH mouse button
   LONG    lHtEntryfield;   // ideal height of an entryfield
   char   *pszFontNameSize; // fns for default widget font
   BOOL    bIsDBCS;         // true if system is dbcs

   // xptoolkit services we look after, & the primaeval appshell too.
   nsIFontRetrieverService *fontService;
   nsDragService           *dragService;
   nsIAppShell             *appshell;

   // We're caching resource-loaded things here too.  This may be
   // better-suited elsewhere, but there shouldn't be very many of them.
   HPOINTER GetPointer( nsCursor aCursor);
   HPOINTER GetFrameIcon();

   // local->Unicode cp. conversion
   PRUnichar *ConvertToUcs( const char *szText, PRUnichar *pBuffer, ULONG ulSize);

   // Unicode->local cp. conversions
   char *ConvertFromUcs( const PRUnichar *pText, char *szBuffer, ULONG ulSize);
   char *ConvertFromUcs( const nsString &aStr, char *szBuffer, ULONG ulSize);
   // these methods use a single static buffer
   const char *ConvertFromUcs( const PRUnichar *pText);
   const char *ConvertFromUcs( const nsString &aStr);

   // Atom service; clients don't need to bother about freeing them.
   ATOM GetAtom( const char *atomname);
   ATOM GetAtom( const nsString &atomname);

#if 0
   HWND     GetWindowForPrinting( PCSZ pszClass, ULONG ulStyle);
#endif

   nsWidgetModuleData();
  ~nsWidgetModuleData();

   void Init( nsIAppShell *aPrimaevalAppShell);

 private:
   ULONG        idSelect;    
   HPOINTER     hptrSelect;  // !! be more sensible about this...
   HPOINTER     hptrFrameIcon;
#if 0
   nsHashtable *mWindows;
#endif

   // Utility function for creating the Unicode conversion object
   int CreateUcsConverter();

   UconvObject  converter;
   BOOL         supplantConverter;

   nsVoidArray  atoms;
};

extern nsWidgetModuleData gModuleData;

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

// WMU'd to avoid potential clash with offical names.
// MP2 is the other window.
#define WMU_MOUSEENTER 0x041e
#define WMU_MOUSELEAVE 0x041f

#ifndef FCF_CLOSEBUTTON // defined in the Merlin toolkit
#define FCF_CLOSEBUTTON 0x04000000L
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
