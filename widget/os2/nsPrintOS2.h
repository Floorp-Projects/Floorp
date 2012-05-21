/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
HDC PrnOpenDC( PRTQUEUE *pPrintQueue, PCSZ pszApplicationName, int copies, int destination, char *file);

// Get the hardcopy caps for the selected form
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo);

#endif
