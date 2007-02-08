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
#include "nsVoidArray.h"
#ifdef USE_XPRINT
#include "nsIDeviceContextSpecXPrint.h"
#endif /* USE_XPRINT */
#include "nsPrintdOS2.h"
#include <os2.h>
#include <pmddim.h>

#include "nsPrintOS2.h"
//---------------------------------------------------------------------
// nsDeviceContextSpecOS2
//---------------------------------------------------------------------

class nsDeviceContextSpecOS2 : public nsIDeviceContextSpec
#ifdef USE_XPRINT
                             , public nsIDeviceContextSpecXp
#endif
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
 * @param aIsPrintPreview if PR_TRUE, creating Spec for PrintPreview
 * @return error status
 */
  NS_IMETHOD Init(nsIWidget *aWidget, nsIPrintSettings* aPS, PRBool aIsPrintPreview);
  
  NS_IMETHOD ClosePrintManager();

  NS_IMETHOD GetDestination ( int &aDestination ); 

  NS_IMETHOD GetPrinterName ( char **aPrinter );

  NS_IMETHOD GetCopies ( int &aCopies );

  NS_IMETHOD GetPath ( char **aPath );    

  NS_IMETHOD GetUserCancelled( PRBool &aCancel );      

  NS_IMETHOD GetPRTQUEUE(PRTQUEUE *&p);

#ifdef MOZ_CAIRO_GFX
  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface);
  NS_IMETHOD BeginDocument(PRUnichar* aTitle, PRUnichar* aPrintToFileName,
                           PRInt32 aStartPage, PRInt32 aEndPage);
  NS_IMETHOD EndDocument();
  NS_IMETHOD BeginPage();
  NS_IMETHOD EndPage();
#endif

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

