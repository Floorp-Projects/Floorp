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
#define INCL_DOSERRORS

#include "nsWidgetDefs.h"
#include "nsHashtable.h"
#include "resid.h"
#include <stdio.h>
#include <stdlib.h>

// Module-level data & utility functions:
//       * unicode keycode & string conversion
//       * atom management
//       * printing bits & pieces (though not really any more)

#include "nsDragService.h"
#include "nsAppShell.h"

#include <unidef.h>

#ifdef XP_OS2_VACPP
#define KBD_CTRL KBD_CONTROL
#endif

NS_IMPL_ISUPPORTS0(nsWidgetModuleData)

nsWidgetModuleData::nsWidgetModuleData()
{
   hModResources = NULLHANDLE;
}

// This is called when the first appshell is created.
void nsWidgetModuleData::Init( nsIAppShell *aPrimaevalAppShell)
{
   if( 0 != hModResources) return;

   char   buffer[CCHMAXPATH];
   APIRET rc;

   rc = DosLoadModule( buffer, CCHMAXPATH, "WDGTRES", &hModResources);

   if( rc)
   {
#ifdef DEBUG
      printf( "Widget failed to load resource DLL. rc = %d, cause = %s\n",
              (int)rc, buffer);
#endif
      hModResources = 0;
   }

   szScreen.cx = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN);
   szScreen.cy = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN);

   bMouseSwitched = WinQuerySysValue( HWND_DESKTOP, SV_SWAPBUTTON);

   // Magic number -- height of normal text fields in pixels.
   //                 Both combobox and (atm) nsBrowserWindow depend on this.
   lHtEntryfield = 26;

   hptrSelect = hptrFrameIcon = 0;
   idSelect = 0;

   // Work out if the system is DBCS
   COUNTRYCODE cc = { 0 };
   DosQueryDBCSEnv( CCHMAXPATH, &cc, buffer);
   bIsDBCS = buffer[0] || buffer[1];

   // XXXX KNOCKED OUT UNTIL nsDragService.cpp builds again
   //   dragService = new nsDragService;
   //   NS_ADDREF(dragService);

   // keep a ref beyond the client app so we shut ourselves down properly.
   // don't do this for embedding where the appshell pointer is nsnull
   appshell = aPrimaevalAppShell;
   if (appshell != nsnull)
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
#if 0
   delete mWindows;
#endif

   // finally shut down the appshell.  No more PM.
   // (hope that gfxos2 has gone first!)
   // don't do this if appshell is nsnull for embedding
   if (appshell != nsnull)
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
#ifdef DEBUG
      printf( "\n*** Need to implement cursor type %d (see widget/src/os2/nsModule.cpp)\n\n", (int) aCursor);
#endif
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
#ifdef DEBUG
   if( unirc != ULS_SUCCESS)
   {
      printf( "Couldn't create widget unicode converter.\n");
   }
#endif

   return unirc;
}

// Conversion from appropriate codepage to unicode
ULONG nsWidgetModuleData::ConvertToUcs( const char *szText, 
                                        PRUnichar *pBuffer, 
                                        ULONG ulSize)
{
   if( supplantConverter)
   {
      // We couldn't create a converter for some reason, so do this 'by hand'.
      // Note this algorithm is fine for most of most western charsets, but
      // fails dismally for various glyphs, baltic, points east...
      ULONG ulCount = 0;
      PRUnichar *pTmp = pBuffer;
      while( *szText && ulCount < ulSize - 1) // (one for terminator)
      {
         *pTmp = (PRUnichar)((SHORT)*szText & 0x00FF);
         pTmp++;
         szText++;
         ulCount++;
      }

      // terminate string
      *pTmp = (PRUnichar)0;

      return ulCount;
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

   UniChar *tmp = NS_REINTERPRET_CAST(UniChar *,pBuffer); // function alters the out pointer

   int unirc = UniUconvToUcs( converter, (void **)&szText, &szLen,
                              &tmp, &ucsLen, &cSubs);

   if( unirc == UCONV_E2BIG) // k3w1
   {
      // terminate output string (truncating)
      *(pBuffer + ulSize - 1) = (PRUnichar)0;
      ucsLen = ulSize - 1;
   }
   else if( unirc != ULS_SUCCESS)
   {
#ifdef DEBUG
      printf( "UniUconvToUcs failed, rc %X\n", unirc);
#endif
      supplantConverter = TRUE;
      ucsLen = ConvertToUcs( szText, pBuffer, ulSize);
      supplantConverter = FALSE;
   }
   else
   {
      // ucsLen is the number of unused Unichars in the output buffer,
      // but we want the number of converted unicode characters
      ucsLen = ulSize - ucsLen - 1;
      *(pBuffer + ucsLen) = (PRUnichar)0;
   }

   return (ULONG)ucsLen;
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

   UniChar *ucsString = NS_REINTERPRET_CAST(UniChar *,pText);
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
#ifdef DEBUG
      printf( "UniUconvFromUcs failed, rc %X\n", unirc);
#endif
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
   const PRUnichar *pUnicode = aString.get();

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
   return ConvertFromUcs( aString.get());
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

ULONG nsWidgetModuleData::GetCurrentDirectory(ULONG bufLen, PSZ dirString)
{
   ULONG drivemap,currentdisk;
   const char * strPtr = dirString +3;

   // We need extra space for the 'x:\'.
   if (bufLen < 3)
   {
      return 0;
   }

   ULONG len = bufLen - 3;

   APIRET rc = DosQueryCurrentDir(0, (PBYTE)strPtr, &len);
   if (rc != ERROR_BUFFER_OVERFLOW)
   {
      if (rc)
         return 0;
      len = strlen(strPtr) + 3;

      // Directory does not start with 'x:\', so add it.
      DosQueryCurrentDisk (&currentdisk, &drivemap);

      // Follow the case of the first letter in the path.
      if (isupper(dirString[3]))
         dirString[0] = (char)('A' - 1 + currentdisk);
      else
         dirString[0] = (char)('a' - 1 + currentdisk);
      dirString[1] = ':';
      dirString[2] = '\\';
   }
   else
      len += 3;   // Add three extra spaces for the 'x:\'.
   return len;
}

int nsWidgetModuleData::WideCharToMultiByte( int CodePage, const PRUnichar *pText, ULONG ulLength, char* szBuffer, ULONG ulSize )
{
  UconvObject* pConverter = 0;
  /* Free any converters that were created */
  for (int i=0; i < 15 /* eCharSet_COUNT from nsFontMetricsOS2.cpp */ ; i++ ) {
    if (gUconvInfo[i].mCodePage == CodePage) {
      if (!gUconvInfo[i].mConverter) {
        UniChar codepage[20];
        int unirc = UniMapCpToUcsCp( CodePage, codepage, 20);
        UniCreateUconvObject( codepage, &gUconvInfo[i].mConverter);
        break;
      } /* endif */
      pConverter = &gUconvInfo[i].mConverter;
    } /* endif */
  } /* endfor */
  if (!pConverter) {
      pConverter = &gUconvInfo[0].mConverter;
  } /* endif */

  UniChar *ucsString = (UniChar*) pText;
  size_t   ucsLen = ulLength;
  size_t   cplen = ulSize;
  size_t   cSubs = 0;

  char *tmp = szBuffer; // function alters the out pointer

   int unirc = UniUconvFromUcs( *pConverter, &ucsString, &ucsLen,
                                (void**) &tmp, &cplen, &cSubs);

  if( unirc == UCONV_E2BIG) // k3w1
  {
    // terminate output string (truncating)
    *(szBuffer + ulSize - 1) = '\0';
  }
#ifdef DEBUG
  else if( unirc != ULS_SUCCESS)
  {
     printf("very bad");
  }
#endif
  return ulSize - cplen;
}

nsWidgetModuleData *gWidgetModuleData = nsnull;
