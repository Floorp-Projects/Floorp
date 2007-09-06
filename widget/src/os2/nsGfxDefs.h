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

#ifndef _nsgfxdefs_h
#define _nsgfxdefs_h

// nsGfxDefs.h - common includes etc. for gfx library

#include "nscore.h"

#define INCL_PM
#define INCL_DOS
#include <os2.h>

#include <uconv.h> // XXX hack XXX

#define COLOR_CUBE_SIZE 216

void PMERROR(const char *str);

class nsString;
class nsIDeviceContext;

// Module data
struct nsGfxModuleData
{
   HMODULE hModResources;
   HPS     hpsScreen;
   LONG    lDisplayDepth;

   nsGfxModuleData();
  ~nsGfxModuleData();

   // XXX XXX XXX this is a hack copied from the widget library (where it's
   //             not a hack but perfectly valid) until font-switching comes
   //             on-line.
   // Unicode->local cp. conversions
   char *ConvertFromUcs( const PRUnichar *pText, ULONG ulLength, char *szBuffer, ULONG ulSize);
   char *ConvertFromUcs( const nsString &aStr, char *szBuffer, ULONG ulSize);
   // these methods use a single static buffer
   const char *ConvertFromUcs( const PRUnichar *pText, ULONG ulLength);
   const char *ConvertFromUcs( const nsString &aStr);

   UconvObject  converter;
   BOOL         supplantConverter;
   PRUint32     renderingHints;
   ULONG        ulCodepage;
   // XXX XXX XXX end hack

   void Init();
};

extern nsGfxModuleData gModuleData;

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define MK_RGB(r,g,b) ((r) * 65536) + ((g) * 256) + (b)

#endif
