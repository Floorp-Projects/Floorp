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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/08/2000       IBM Corp.      Fix for trying to us an already freed mGammaTable.
 */

// ToDo: nothing
#include "nsGfxDefs.h"
#include <stdlib.h>

#include "nsDeviceContextOS2.h" // sigh...
#include "nsPaletteOS2.h"
#include "il_util.h"

// os2fe/palette.cpp lives! Sort of.
//
// There's just the one palette, which is shared by all the windows,
// DC's and whatever that get created.
//
// This makes apprunner vaguely usable!
//
// Printing might need some work.

// Common base
class nsPaletteOS2 : public nsIPaletteOS2
{
 protected:
   nsIDeviceContext *mContext; // don't hold a ref to avoid circularity
   PRUint8          *mGammaTable;

 public:
   virtual nsresult Init( nsIDeviceContext *aContext,
                          ULONG * = 0, ULONG = 0)
   {
      mContext = aContext;
      mContext->GetGammaTable( mGammaTable);
      return mContext == nsnull ? NS_ERROR_FAILURE : NS_OK;
   }

   long GetGPIColor( nsIDeviceContext *aContext, HPS hps, nscolor rgb)
   {
      if (mContext != aContext) {
         mContext = aContext;
         mContext->GetGammaTable( mGammaTable);
      }
      long gcolor = MK_RGB( mGammaTable[NS_GET_R(rgb)],
                            mGammaTable[NS_GET_G(rgb)],
                            mGammaTable[NS_GET_B(rgb)]);
      return GpiQueryColorIndex( hps, 0, gcolor);
   }

   virtual nsresult GetNSPalette( nsPalette &aPalette) const
   {
      aPalette = 0;
      return NS_OK;
   }

   NS_DECL_ISUPPORTS

   nsPaletteOS2()
   {
      NS_INIT_REFCNT();
      mContext = nsnull;
      mGammaTable = 0;
   }

   virtual ~nsPaletteOS2()
   {}
};

// this isn't really an xpcom object, so don't allow anyone to get anything
nsresult nsPaletteOS2::QueryInterface( const nsIID&, void**)
{
   return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsPaletteOS2)
NS_IMPL_RELEASE(nsPaletteOS2)

// Logical colour table, for 8bpp with no palette manager or explicit choice
class nsLCOLPaletteOS2 : public nsPaletteOS2
{
   ULONG *mTable;
   ULONG  mSize;

 public:
   nsresult Init( nsIDeviceContext *aContext,
                  ULONG *pEntries, ULONG cEntries)
   {
      mTable = pEntries;
      mSize = cEntries;
      return nsPaletteOS2::Init( aContext);
   }

   nsresult Select( HPS hps, nsIDeviceContext *)
   {
      BOOL rc = GpiCreateLogColorTable( hps, LCOL_RESET | LCOL_PURECOLOR,
                                        LCOLF_CONSECRGB, 0,
                                        mSize, (PLONG) mTable);
      if( !rc)
         PMERROR( "GpiCreateLogColorTable");
      return rc ? NS_OK : NS_ERROR_FAILURE;
   }

   nsresult Deselect( HPS hps)
   {
      BOOL rc = GpiCreateLogColorTable( hps, LCOL_RESET, 0, 0, 0, 0);
      return rc ? NS_OK : NS_ERROR_FAILURE;
   }

   nsLCOLPaletteOS2()
   {
      mTable = 0;
      mSize = 0;
   }

  ~nsLCOLPaletteOS2()
   {
      if( mTable) free( mTable);
   }
};

// Palette manager palette, for 8bpp with palette manager
class nsHPALPaletteOS2 : public nsPaletteOS2
{
   HPAL mHPal;

 public:
   nsresult Init( nsIDeviceContext *aContext,
                  ULONG *pEntries, ULONG cEntries)
   {
      mHPal = GpiCreatePalette( 0/*hab*/, LCOL_PURECOLOR, LCOLF_CONSECRGB,
                                cEntries, pEntries);
      free( pEntries);

      return nsPaletteOS2::Init( aContext);
   }

   nsresult GetNSPalette( nsPalette &aPalette) const
   {
      aPalette = (nsPalette) mHPal;
      return NS_OK;
   }

   nsresult Select( HPS hps, nsIDeviceContext *aContext)
   {
      HPAL rc = GpiSelectPalette( hps, mHPal);
      if( rc == (HPAL) PAL_ERROR)
      {
         PMERROR( "GpiSelectPalette");
         return NS_ERROR_FAILURE;
      }

      // okay, we could do with a window here.  Unfortunately there's
      // no guarantee that this is going to return anything sensible.
      nsNativeWidget wdg = ((nsDeviceContextOS2 *) aContext)->mWidget;
      if( wdg)
      {
         ULONG ulDummy = 0;
         WinRealizePalette( (HWND)wdg, hps, &ulDummy);
      }
      return NS_OK;
   }

   nsresult Deselect( HPS hps)
   {
      HPAL rc = GpiSelectPalette( hps, 0);
      return rc == ((HPAL)PAL_ERROR) ? NS_ERROR_FAILURE : NS_OK;
   }

   nsHPALPaletteOS2()
   {
      mHPal = 0;
   }

  ~nsHPALPaletteOS2()
   {
      if( mHPal)
         GpiDeletePalette( mHPal);
   }
};

// RGB colour table, for >8bpp
class nsRGBPaletteOS2 : public nsPaletteOS2
{
 public:
   nsresult Select( HPS hps, nsIDeviceContext *)
   {
      BOOL rc = GpiCreateLogColorTable( hps, LCOL_PURECOLOR,
                                        LCOLF_RGB, 0, 0, 0);
      if( !rc)
         PMERROR( "GpiCreateLogColorTable #2");
      return rc ? NS_OK : NS_ERROR_FAILURE;
   }

   nsresult Deselect( HPS hps)
   {
      BOOL rc = GpiCreateLogColorTable( hps, LCOL_RESET, 0, 0, 0, 0);
      return rc ? NS_OK : NS_ERROR_FAILURE;
   }

   nsRGBPaletteOS2() {}
  ~nsRGBPaletteOS2() {}
};

nsresult NS_CreatePalette( nsIDeviceContext *aContext, nsIPaletteOS2 *&aPalette)
{
   nsresult       rc = NS_OK;
   IL_ColorSpace *colorSpace = 0;

   nsPaletteOS2 *newPalette = 0;

   rc = aContext->GetILColorSpace( colorSpace);
   if( NS_SUCCEEDED(rc))
   {
      if( NI_PseudoColor == colorSpace->type)
      {
         PULONG pPalette = (PULONG) calloc( COLOR_CUBE_SIZE, sizeof( ULONG));
   
         // Now set the color cube entries.
         for( PRInt32 i = 0; i < COLOR_CUBE_SIZE; i++)
         {
            IL_RGB *map = colorSpace->cmap.map + i;
            pPalette[ i] = MK_RGB( map->red, map->green, map->blue);
         }
   
         // this works, sorta.  Should probably tell users,
         // or activate via a pref, or something.
         if( getenv( "MOZ_USE_LCOL"))
            newPalette = new nsLCOLPaletteOS2;
         else
            newPalette = new nsHPALPaletteOS2;
         rc = newPalette->Init( aContext, pPalette, COLOR_CUBE_SIZE);
      }
      else
      {
         newPalette = new nsRGBPaletteOS2;
         rc = newPalette->Init( aContext);
      }
   
      IL_ReleaseColorSpace( colorSpace);
   }

   if( NS_SUCCEEDED(rc))
   {
      NS_ADDREF(newPalette);
      aPalette = newPalette;
   }

   return rc;
}
