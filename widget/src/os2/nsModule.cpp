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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 06/28/2000       IBM Corp.       Fixed querying of system keyboard
 * 
 */

#define INCL_DOSDEVIOCTL // for keyboard layout

#include "nsWidgetDefs.h"
#include "nsHashtable.h"
#include "resid.h"
#include <stdio.h>
#include <stdlib.h>

// Module-level data & utility functions:
//       * unicode keycode & string conversion
//       * atom management
//       * printing bits & pieces (though not really any more)

#include "nsIFontRetrieverService.h"
#include "nsDragService.h"
#include "nsAppShell.h"

#include <unidef.h>

#ifdef XP_OS2_VACPP
#define KBD_CTRL KBD_CONTROL
#endif

nsWidgetModuleData::nsWidgetModuleData()
{}

// This is called when the first appshell is created.
void nsWidgetModuleData::Init( nsIAppShell *aPrimaevalAppShell)
{
   if( 0 != hModResources) return;

   char   buffer[CCHMAXPATH];
   APIRET rc;

   rc = DosLoadModule( buffer, CCHMAXPATH, "WDGTRES", &hModResources);

   if( rc)
   {
      printf( "Widget failed to load resource DLL. rc = %d, cause = %s\n",
              (int)rc, buffer);
      hModResources = 0;
   }

   szScreen.cx = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN);
   szScreen.cy = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN);

   bMouseSwitched = WinQuerySysValue( HWND_DESKTOP, SV_SWAPBUTTON);

   // Magic number -- height of normal text fields in pixels.
   //                 Both combobox and (atm) nsBrowserWindow depend on this.
   lHtEntryfield = 26;

   rc = WinLoadString( 0/*hab*/, hModResources, ID_STR_FONT, 256, buffer);
   if( !rc) strcpy( buffer, "8.Helv");
   pszFontNameSize = strdup( buffer);

   hptrSelect = hptrFrameIcon = 0;
   idSelect = 0;

   // Work out if the system is DBCS
   COUNTRYCODE cc = { 0 };
   DosQueryDBCSEnv( CCHMAXPATH, &cc, buffer);
   bIsDBCS = buffer[0] || buffer[1];

   fontService = nsnull;

   // XXXX KNOCKED OUT UNTIL nsDragService.cpp builds again
   //   dragService = new nsDragService;
   //   NS_ADDREF(dragService);

   // keep a ref beyond the client app so we shut ourselves down properly.
   appshell = aPrimaevalAppShell;
   NS_ADDREF(appshell);

   converter = 0;
   supplantConverter = FALSE;

#if 0
   mWindows = nsnull;
#endif
}

nsWidgetModuleData::~nsWidgetModuleData()
{
   if( converter)
      UniFreeUconvObject( converter);

   NS_IF_RELEASE(dragService);

   NS_IF_RELEASE(fontService);

   PRInt32  cAtoms = atoms.Count();
   HATOMTBL systbl = WinQuerySystemAtomTable();
   for( PRInt32 i = 0; i < cAtoms; i++)
      WinDeleteAtom( systbl, (ATOM) atoms.ElementAt(i));

   if( hptrSelect)
      WinDestroyPointer( hptrSelect);
   if( hptrFrameIcon)
      WinDestroyPointer( hptrFrameIcon);
   if( hModResources)
      DosFreeModule( hModResources);
   free( pszFontNameSize);
#if 0
   delete mWindows;
#endif

   // finally shut down the appshell.  No more PM.
   // (hope that gfxos2 has gone first!)
   NS_IF_RELEASE(appshell);
}

HPOINTER nsWidgetModuleData::GetPointer( nsCursor aCursor)
{
   ULONG idPtr = 0;

   switch( aCursor)
   {
      case eCursor_hyperlink:           idPtr = ID_PTR_SELECTURL  ; break;
      case eCursor_arrow_north:         idPtr = ID_PTR_ARROWNORTH ; break;
      case eCursor_arrow_north_plus:    idPtr = ID_PTR_ARROWNORTHP; break;
      case eCursor_arrow_south:         idPtr = ID_PTR_ARROWSOUTH ; break;
      case eCursor_arrow_south_plus:    idPtr = ID_PTR_ARROWSOUTHP; break;
      case eCursor_arrow_west:          idPtr = ID_PTR_ARROWWEST  ; break;
      case eCursor_arrow_west_plus:     idPtr = ID_PTR_ARROWWESTP ; break;
      case eCursor_arrow_east:          idPtr = ID_PTR_ARROWEAST  ; break;
      case eCursor_arrow_east_plus:     idPtr = ID_PTR_ARROWEASTP ; break;
      case eCursor_copy:                idPtr = ID_PTR_COPY       ; break;
      case eCursor_alias:               idPtr = ID_PTR_ALIAS      ; break;
      case eCursor_cell:                idPtr = ID_PTR_CELL       ; break;
      case eCursor_grab:                idPtr = ID_PTR_GRAB       ; break;
      case eCursor_grabbing:            idPtr = ID_PTR_GRABBING   ; break;
      case eCursor_spinning:            idPtr = ID_PTR_ARROWWAIT  ; break;

      case eCursor_crosshair:
      case eCursor_help:
      case eCursor_context_menu:
      case eCursor_count_up:
      case eCursor_count_down:
      case eCursor_count_up_down:
         break;

      default:
         NS_ASSERTION( 0, "Unknown cursor type");
         break;
   }

   if( idPtr == 0)
   {
      idPtr = ID_PTR_SELECTURL; // default to hyperlink cursor?
      printf( "\n*** Need to implement cursor type %d (see widget/src/os2/nsModule.cpp)\n\n", (int) aCursor);
   }

   // Use an array and indices here since we have all the pointers in place?
   if( idSelect != idPtr)
   {
      idSelect = idPtr;
      hptrSelect = WinLoadPointer( HWND_DESKTOP, hModResources, idSelect);
   }

   return hptrSelect;
}

HPOINTER nsWidgetModuleData::GetFrameIcon()
{
   if( !hptrFrameIcon)
      hptrFrameIcon = WinLoadPointer( HWND_DESKTOP,
                                      hModResources, ID_ICO_FRAME);
   return hptrFrameIcon;
}

int nsWidgetModuleData::CreateUcsConverter()
{
   // Create a converter from unicode to a codepage which PM can display.
   UniChar codepage[20];
   int unirc = UniMapCpToUcsCp( 0, codepage, 20);
   if( unirc == ULS_SUCCESS)
   {
      unirc = UniCreateUconvObject( codepage, &converter);
      // XXX do we need to set substitution options here?
#ifdef DEBUG
      if( unirc == ULS_SUCCESS)
      {
         printf( "Widget library created unicode converter for cp %s\n",
                 ConvertFromUcs( (PRUnichar *) codepage));
      }
#endif
   }
   if( unirc != ULS_SUCCESS)
   {
      printf( "Couldn't create widget unicode converter.\n");
   }

   return unirc;
}

// Conversion from appropriate codepage to unicode
PRUnichar *nsWidgetModuleData::ConvertToUcs( const char *szText, 
                                             PRUnichar *pBuffer, 
                                             ULONG ulSize)
{
   if( supplantConverter)
   {
      // We couldn't create a converter for some reason, so do this 'by hand'.
      // Note this algorithm is fine for most of most western charsets, but
      // fails dismally for various glyphs, baltic, points east...
      ULONG ulCount = 0;
      PRUnichar *pSave = pBuffer;
      while( *szText && ulCount < ulSize - 1) // (one for terminator)
      {
         *pBuffer = (PRUnichar)((SHORT)*szText & 0x00FF);
         pBuffer++;
         szText++;
         ulCount++;
      }

      // terminate string
      *pBuffer = (PRUnichar)0;

      return pSave;
   }

   if( !converter)
   {
      if( CreateUcsConverter() != ULS_SUCCESS)
      {
         supplantConverter = TRUE;
         return ConvertToUcs( szText, pBuffer, ulSize);
      }
   }

   // Have converter, now get it to work...

   char    *szString = (char *)szText;
   size_t   szLen = strlen( szText) + 1;
   size_t   ucsLen = ulSize;
   size_t   cSubs = 0;

   UniChar *tmp = pBuffer; // function alters the out pointer

   int unirc = UniUconvToUcs( converter, (void **)&szText, &szLen,
                              &tmp, &ucsLen, &cSubs);

   if( unirc == UCONV_E2BIG) // k3w1
   {
      // terminate output string (truncating)
      *(pBuffer + ulSize - 1) = (PRUnichar)0;
   }
   else if( unirc != ULS_SUCCESS)
   {
      printf( "UniUconvToUcs failed, rc %X\n", unirc);
      supplantConverter = TRUE;
      pBuffer = ConvertToUcs( szText, pBuffer, ulSize);
      supplantConverter = FALSE;
   }

   return pBuffer;
}

// Conversion from unicode to appropriate codepage
char *nsWidgetModuleData::ConvertFromUcs( const PRUnichar *pText,
                                          char *szBuffer, ULONG ulSize)
{
   if( supplantConverter)
   {
      // We couldn't create a converter for some reason, so do this 'by hand'.
      // Note this algorithm is fine for most of most western charsets, but
      // fails dismally for various glyphs, baltic, points east...
      ULONG ulCount = 0;
      char *szSave = szBuffer;
      while( *pText && ulCount < ulSize - 1) // (one for terminator)
      {
         *szBuffer = (char) *pText;
         szBuffer++;
         pText++;
         ulCount++;
      }

      // terminate string
      *szBuffer = '\0';

      return szSave;
   }

   if( !converter)
   {
      if( CreateUcsConverter() != ULS_SUCCESS)
      {
         supplantConverter = TRUE;
         return ConvertFromUcs( pText, szBuffer, ulSize);
      }
   }

   // Have converter, now get it to work...

   UniChar *ucsString = (UniChar*) pText;
   size_t   ucsLen = UniStrlen( ucsString) + 1;
   size_t   cplen = ulSize;
   size_t   cSubs = 0;

   char *tmp = szBuffer; // function alters the out pointer

   int unirc = UniUconvFromUcs( converter, &ucsString, &ucsLen,
                                (void**) &tmp, &cplen, &cSubs);

   if( unirc == UCONV_E2BIG) // k3w1
   {
      // terminate output string (truncating)
      *(szBuffer + ulSize - 1) = '\0';
   }
   else if( unirc != ULS_SUCCESS)
   {
      printf( "UniUconvFromUcs failed, rc %X\n", unirc);
      supplantConverter = TRUE;
      szBuffer = ConvertFromUcs( pText, szBuffer, ulSize);
      supplantConverter = FALSE;
   }

   return szBuffer;
}

char *nsWidgetModuleData::ConvertFromUcs( const nsString &aString,
                                          char *szBuffer, ULONG ulSize)
{
   char *szRet = 0;
   const PRUnichar *pUnicode = aString.GetUnicode();

   if( pUnicode)
      szRet = ConvertFromUcs( pUnicode, szBuffer, ulSize);
   else
      szRet = aString.ToCString( szBuffer, ulSize);

   return szRet;
}

const char *nsWidgetModuleData::ConvertFromUcs( const PRUnichar *pText)
{
   // This is probably okay; longer strings will be truncated but istr there's
   // a PM limit on things like windowtext
   // (which these routines are usually used for)

   static char buffer[1024]; // XXX (multithread)
   *buffer = '\0';
   return ConvertFromUcs( pText, buffer, 1024);
}

const char *nsWidgetModuleData::ConvertFromUcs( const nsString &aString)
{
   const char *szRet = 0;
   const PRUnichar *pUnicode = aString.GetUnicode();

   if( pUnicode)
      szRet = ConvertFromUcs( pUnicode);
   else
      szRet = aString.GetBuffer(); // hrm.

   return szRet;
}

ATOM nsWidgetModuleData::GetAtom( const char *atomname)
{
   ATOM atom = WinAddAtom( WinQuerySystemAtomTable(), atomname);
   atoms.AppendElement( (void*) atom);
   return atom;
}

ATOM nsWidgetModuleData::GetAtom( const nsString &atomname)
{
   return GetAtom( ConvertFromUcs( atomname));
}

// Printing stuff -- need to be able to generate a window of a given
// flavour and then do stuff to it in nsWindow::Paint()

#if 0

class nsWndClassKey : public nsHashKey
{
   PCSZ pszClassname;

public:
  nsWndClassKey( PCSZ pszName) : pszClassname( pszName)
     {}
  PRUint32 HashValue() const
     { return (PRUint32) pszClassname; }
  PRBool Equals( const nsHashKey *aKey) const
     { return HashValue() == aKey->HashValue(); }
  nsHashKey *Clone() const
     { return new nsWndClassKey( pszClassname); }
};


// Get a window to render its state
HWND nsWidgetModuleData::GetWindowForPrinting( PCSZ pszClass, ULONG ulStyle)
{
   if( mWindows == nsnull)
      mWindows = new nsHashtable;

   nsWndClassKey aKey( pszClass);

   if( mWindows->Get( &aKey) == nsnull)
   {
      HWND hwnd = WinCreateWindow( HWND_DESKTOP, pszClass, "", ulStyle,
                                   0, -4096, 0, 0, HWND_DESKTOP,
                                   HWND_TOP, 0, 0, 0);
      NS_ASSERTION( hwnd, "WinCreateWindow for printing failed");
      mWindows->Put( &aKey, (void*) hwnd);
   }

   return (HWND) mWindows->Get( &aKey);
}

#endif

nsWidgetModuleData gModuleData;
