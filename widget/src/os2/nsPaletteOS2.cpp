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
 * The Original Code is an API for using the OS/2 Palette Manager.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsPaletteOS2.h"
#include "nsGfxDefs.h"
#ifdef DEBUG
#include <stdio.h>
#endif

HPAL nsPaletteOS2::hGlobalPalette = NULLHANDLE;
BOOL nsPaletteOS2::fPaletteInitialized = FALSE;
ULONG nsPaletteOS2::aulTable[256];

#define NUM_SYS_COLORS 24

typedef struct _MYRGB {
  BYTE red;
  BYTE green;
  BYTE blue;
} MYRGB;

MYRGB sysColors[NUM_SYS_COLORS] =
{
  0x00, 0x00, 0x00,   // CLR_BLACK
  0x00, 0x00, 0x80,   // CLR_DARKBLUE
  0x00, 0x80, 0x00,   // CLR_DARKGREEN
  0x00, 0x80, 0x80,   // CLR_DARKCYAN
  0x80, 0x00, 0x00,   // CLR_DARKRED
  0x80, 0x00, 0x80,   // CLR_DARKPINK
  0x80, 0x80, 0x00,   // CLR_BROWN
  0x80, 0x80, 0x80,   // CLR_DARKGRAY
  0xCC, 0xCC, 0xCC,   // CLR_PALEGRAY
  0x00, 0x00, 0xFF,   // CLR_BLUE
  0x00, 0xFF, 0x00,   // CLR_GREEN
  0x00, 0xFF, 0xFF,   // CLR_CYAN
  0xFF, 0x00, 0x00,   // CLR_RED
  0xFF, 0x00, 0xFF,   // CLR_PINK
  0xFF, 0xFF, 0x00,   // CLR_YELLOW
  0xFE, 0xFE, 0xFE,   // CLR_OFFWHITE - can only use white at index 255

  0xC0, 0xC0, 0xC0,   // Gray (Windows)
  0xFF, 0xFB, 0xF0,   // Pale Yellow (Windows)
  0xC0, 0xDC, 0xC0,   // Pale Green (Windows)
  0xA4, 0xC8, 0xF0,   // Light Blue (Windows)
  0xA4, 0xA0, 0xA4,   // Medium Gray (Windows)

  0xFF, 0xFF, 0xE4,   // Tooltip color - see nsLookAndFeel.cpp

  0x71, 0x71, 0x71,   // Interpolated color for entryfields
  0xEF, 0xEF, 0xEF    // Interpolated color for entryfields

};

void nsPaletteOS2::InitializeGlobalPalette()
{
  fPaletteInitialized = TRUE;
  LONG lCaps;
  HPS hps = ::WinGetScreenPS(HWND_DESKTOP);
  HDC hdc = ::GpiQueryDevice (hps);
  ::DevQueryCaps(hdc, CAPS_ADDITIONAL_GRAPHICS, 1, &lCaps);
  ::WinReleasePS(hps);

  if (lCaps & CAPS_PALETTE_MANAGER) {
    /* Create the color table */
    int i,j,k,l, ulCurTableEntry = 0;
  
    /* First add the system colors */
    for (i = 0; i < NUM_SYS_COLORS; i++) {
      aulTable[ulCurTableEntry] = MK_RGB(sysColors[i].red, sysColors[i].green, sysColors[i].blue);
      ulCurTableEntry++;
    }
  
    /* Then put the color cube into the table, excluding */
    /* any entry that is also in the system color table */
    for (i=0x00;i <= 0xff;i+=0x33) {
      for (j=0x00;j <= 0xff;j+=0x33) {
        for (k=0x00;k <= 0xff ;k+=0x33) {
          for (l=0;l<ulCurTableEntry;l++) {
            if (aulTable[l] == MK_RGB(i, j, k))
              break;
          }
          if (l == ulCurTableEntry) {
            aulTable[ulCurTableEntry] = MK_RGB(i, j, k);
            ulCurTableEntry++;
          }
        }
      }
    }
  
    /* Back current table entry up one so we overwrite the white that was written */
    /* by the color cube */
    ulCurTableEntry--;
  
    /* Then fudge the rest of the table to have entries that are neither white */
    /* nor black - we'll use an offwhite */
    while (ulCurTableEntry < 255) {
      aulTable[ulCurTableEntry] = MK_RGB(254, 254, 254);
      ulCurTableEntry++;
    }
  
    /* Finally, make the last entry white */
    aulTable[ulCurTableEntry] = MK_RGB(255, 255, 255);
  
#ifdef DEBUG_mikek
    for (i=0;i<256 ;i++ )
      printf("Entry[%d] in 256 color table is %x\n", i, aulTable[i]);
#endif

    /* Create the palette */
    hGlobalPalette = ::GpiCreatePalette ((HAB)0, 0,
                                         LCOLF_CONSECRGB, 256, aulTable);
  }
}

void nsPaletteOS2::FreeGlobalPalette()
{
  if (hGlobalPalette) {
    GpiDeletePalette(hGlobalPalette);
    hGlobalPalette = NULLHANDLE;
  }
}

void nsPaletteOS2::SelectGlobalPalette(HPS hps, HWND hwnd)
{
  if (!fPaletteInitialized)
    InitializeGlobalPalette();
  if (hGlobalPalette) {
    GpiSelectPalette(hps, hGlobalPalette);
    if (hwnd != NULLHANDLE) {
      ULONG cclr;
      WinRealizePalette(hwnd, hps, &cclr);
    }
  }
}

LONG nsPaletteOS2::QueryColorIndex(LONG lColor)
{
  for (int i=0;i<256;i++) {
     if (lColor == aulTable[i]) {
        return i;
     }
  }
  return 0;
}
