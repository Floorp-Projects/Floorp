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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Fairhurst <john_fairhurst@iname.com>
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

#ifndef nsPrintOS2_h___
#define nsPrintOS2_h___

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_SPLDOSPRINT
#define INCL_DEV
#define INCL_DEVDJP
#define INCL_GRE_DEVICE
#include <os2.h>
#include <pmddim.h>
#include "gfxCore.h"

//---------------------------------------------------------------------------
// OS/2 Printing   - was in libprint
//---------------------------------------------------------------------------
// Library init and term; job properties per queue are cached during run.
BOOL PrnInitialize(HMODULE hmodResources);
BOOL PrnTerminate(void);

// opaque type to describe a print queue (printer)
class PRTQUEUE;

#define MAX_PRINT_QUEUES  (128)

class PRINTDLG
{
public:
   PRINTDLG();
  ~PRINTDLG();
   void      RefreshPrintQueue();
   ULONG     GetNumPrinters();
   void      GetPrinter(ULONG printerNdx, char** printerName);
   PRTQUEUE* SetPrinterQueue(ULONG printerNdx);
   LONG      GetPrintDriverSize(ULONG printerNdx);
   PDRIVDATA GetPrintDriver(ULONG printerNdx);
   HDC       GetDCHandle(ULONG printerNdx);
   char*     GetDriverType(ULONG printerNdx);
   BOOL      ShowProperties(ULONG printerNdx);

private:
  ULONG      mQueueCount;
  PRTQUEUE*  mPQBuf[MAX_PRINT_QUEUES];
};


// Release app. resources associated with a printer
BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue);

// Get a DC for the selected printer.  Must supply the application name.
HDC PrnOpenDC( PRTQUEUE *pPrintQueue, PSZ pszApplicationName, int copies, int destination, char *file);

// Get the hardcopy caps for the selected form
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo);

#endif
