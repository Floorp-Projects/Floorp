/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecOS2_h___
#define nsDeviceContextSpecOS2_h___

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_SPLDOSPRINT
#define INCL_DEV
#define INCL_DEVDJP
#define INCL_GRE_DEVICE

#include "nsCOMPtr.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSettings.h"
#include "nsPrintdOS2.h"
#include <os2.h>
#include <pmddim.h>

#include "nsPrintOS2.h"
//---------------------------------------------------------------------
// nsDeviceContextSpecOS2
//---------------------------------------------------------------------

class nsDeviceContextSpecOS2 : public nsIDeviceContextSpec
{
public:
/**
 * Construct a nsDeviceContextSpecOS2, which is an object which contains and manages a mac printrecord
 * @update  dc 12/02/98
 */
  nsDeviceContextSpecOS2();

  NS_DECL_ISUPPORTS

/**
 * Initialize the nsDeviceContextSpecOS2 for use.  This will allocate a printrecord for use
 * @update   dc 2/16/98
 * @param aWidget         Unused
 * @param aPS             Settings for this print job
 * @param aIsPrintPreview if true, creating Spec for PrintPreview
 * @return error status
 */
  NS_IMETHOD Init(nsIWidget *aWidget, nsIPrintSettings* aPS, bool aIsPrintPreview);
  
  NS_IMETHOD ClosePrintManager();

  NS_IMETHOD GetDestination ( int &aDestination ); 

  NS_IMETHOD GetPrinterName ( char **aPrinter );

  NS_IMETHOD GetCopies ( int &aCopies );

  NS_IMETHOD GetPath ( char **aPath );    

  NS_IMETHOD GetUserCancelled( bool &aCancel );      

  NS_IMETHOD GetPRTQUEUE(PRTQUEUE *&p);

  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface);
  NS_IMETHOD BeginDocument(const nsAString& aTitle, PRUnichar* aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage);
  NS_IMETHOD EndDocument();
  NS_IMETHOD BeginPage();
  NS_IMETHOD EndPage();

/**
 * Destructor for nsDeviceContextSpecOS2, this will release the printrecord
 * @update  dc 2/16/98
 */
  virtual ~nsDeviceContextSpecOS2();

  static PRINTDLG PrnDlg;
  static nsresult SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, ULONG printer);

protected:
  OS2PrData mPrData;
  PRTQUEUE *mQueue;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  HDC mPrintDC;
  bool mPrintingStarted;
};

//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorOS2 : public nsIPrinterEnumerator
{
public:
  nsPrinterEnumeratorOS2();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR

protected:
};


#endif

