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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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
 
#include "nsIWidget.h"
#include "nsDeviceContextSpecB.h"
 
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "prenv.h" /* for PR_GetEnv */ 
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsTArray.h"
#include "nsCRT.h"

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecG
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecG cleans it up
// If it gets created (via the Page Setup Dialog) but the user never prints anything
// then it will never be delete, so this class takes care of that.
class GlobalPrinters {
public:
  static GlobalPrinters* GetInstance()   { return &mGlobalPrinters; }
  ~GlobalPrinters()                      { FreeGlobalPrinters(); }

  void      FreeGlobalPrinters();
  nsresult  InitializeGlobalPrinters();

  PRBool    PrintersAreAllocated()       { return mGlobalPrinterList != nsnull; }
  PRInt32   GetNumPrinters()             { return mGlobalNumPrinters; }
  nsString* GetStringAt(PRInt32 aInx)    { return &mGlobalPrinterList->ElementAt(aInx); }

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsTArray<nsString>* mGlobalPrinterList;
  static int            mGlobalNumPrinters;

};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsTArray<nsString>* GlobalPrinters::mGlobalPrinterList = nsnull;
int            GlobalPrinters::mGlobalNumPrinters = 0;

nsDeviceContextSpecBeOS::nsDeviceContextSpecBeOS()
{
}

nsDeviceContextSpecBeOS::~nsDeviceContextSpecBeOS()
{
} 
 
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecBeOS, nsIDeviceContextSpec)
 
/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS::Init(nsIWidget *aWidget,
                                            nsIPrintSettings* aPS,
                                            PRBool aIsPrintPreview)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(nsnull != aPS, "No print settings.");

  mPrintSettings = aPS;
  
  // if there is a current selection then enable the "Selection" radio button
  if (aPS != nsnull) {
    PRBool isOn;
    aPS->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    nsCOMPtr<nsIPrefBranch> pPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
    }
  }

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return rv;
}
 
/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: ClosePrintManager()
{
  return NS_OK;
}  

//  Printer Enumerator
nsPrinterEnumeratorBeOS::nsPrinterEnumeratorBeOS()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorBeOS, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorBeOS::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nsnull;
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  int count = 0;
  while( count < numPrinters )
  {
    printers->AppendElement(*GlobalPrinters::GetInstance()->GetStringAt(count++));
  }
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP nsPrinterEnumeratorBeOS::GetDefaultPrinterName(PRUnichar * *aDefaultPrinterName)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);
  *aDefaultPrinterName = nsnull;
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorBeOS::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
    return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorBeOS::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  return NS_OK;
}

//----------------------------------------------------------------------
nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated())
    return NS_OK;

  mGlobalNumPrinters = 0;

#ifdef USE_POSTSCRIPT
  mGlobalPrinterList = new nsTArray<nsString>();
  if (!mGlobalPrinterList) 
    return NS_ERROR_OUT_OF_MEMORY;

  /* add an entry for the default printer (see nsPostScriptObj.cpp) */
  mGlobalPrinterList->AppendElement(
    nsString(NS_ConvertASCIItoUTF16(NS_POSTSCRIPT_DRIVER_NAME "default")));
  mGlobalNumPrinters++;

  /* get the list of printers */
  char *printerList = nsnull;
  
  /* the env var MOZILLA_PRINTER_LIST can "override" the prefs */
  printerList = PR_GetEnv("MOZILLA_PRINTER_LIST");
  
  if (!printerList) {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID,
                                                   &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->GetCharPref("print.printer_list", &printerList);
    }
  }  

  if (printerList) {
    char *tok_lasts;
    char *name;
    
    /* PL_strtok_r() will modify the string - copy it! */
    printerList = strdup(printerList);
    if (!printerList)
      return NS_ERROR_OUT_OF_MEMORY;    

    for( name = PL_strtok_r(printerList, " ", &tok_lasts) ; 
         name != nsnull ; 
         name = PL_strtok_r(nsnull, " ", &tok_lasts) )
    {
      mGlobalPrinterList->AppendElement(
        nsString(NS_ConvertASCIItoUTF16(NS_POSTSCRIPT_DRIVER_NAME)) + 
        nsString(NS_ConvertASCIItoUTF16(name)));
      mGlobalNumPrinters++;      
    }

    NS_Free(printerList);
  }
#endif /* USE_POSTSCRIPT */
      
  if (mGlobalNumPrinters == 0)
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE; 

  return NS_OK;
}

//----------------------------------------------------------------------
void GlobalPrinters::FreeGlobalPrinters()
{
  delete mGlobalPrinterList;
  mGlobalPrinterList = nsnull;
  mGlobalNumPrinters = 0;
}




