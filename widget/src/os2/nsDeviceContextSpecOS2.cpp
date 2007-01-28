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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Weilbacher <mozilla@weilbacher.org>
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

#include <stdlib.h>
#include "nsDeviceContextSpecOS2.h"

#include "nsReadableUtils.h"
#include "nsISupportsArray.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsPrintfCString.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"
#include "nsStringFwd.h"

#include "nsOS2Uni.h"

PRINTDLG nsDeviceContextSpecOS2::PrnDlg;

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecOS2
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecOS2 cleans it up
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
  nsString* GetStringAt(PRInt32 aInx)    { return mGlobalPrinterList->StringAt(aInx); }
  void      GetDefaultPrinterName(PRUnichar*& aDefaultPrinterName);

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsStringArray* mGlobalPrinterList;
  static ULONG          mGlobalNumPrinters;

};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsStringArray* GlobalPrinters::mGlobalPrinterList = nsnull;
ULONG          GlobalPrinters::mGlobalNumPrinters = 0;
//---------------

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecOS2
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecOS2 :: nsDeviceContextSpecOS2()
{
  mQueue = nsnull;
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecOS2
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecOS2 :: ~nsDeviceContextSpecOS2()
{
  if( mQueue)
     PrnClosePrinter( mQueue);
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
#ifdef USE_XPRINT
static NS_DEFINE_IID(kIDeviceContextSpecXPIID, NS_IDEVICE_CONTEXT_SPEC_XP_IID);
#endif

void SetupDevModeFromSettings(ULONG printer, nsIPrintSettings* aPrintSettings)
{
  if (aPrintSettings) {
    int bufferSize = 3 * sizeof(DJP_ITEM);
    PBYTE pDJP_Buffer = new BYTE[bufferSize];
    memset(pDJP_Buffer, 0, bufferSize);
    PDJP_ITEM pDJP = (PDJP_ITEM) pDJP_Buffer;

    HDC hdc = nsDeviceContextSpecOS2::PrnDlg.GetDCHandle(printer);
    char* driver = nsDeviceContextSpecOS2::PrnDlg.GetDriverType(printer);

    // Setup Orientation
    PRInt32 orientation;
    aPrintSettings->GetOrientation(&orientation);
    if (!strcmp(driver, "LASERJET"))
      pDJP->lType = DJP_ALL;
    else
      pDJP->lType = DJP_CURRENT;
    pDJP->cb = sizeof(DJP_ITEM);
    pDJP->ulNumReturned = 1;
    pDJP->ulProperty = DJP_SJ_ORIENTATION;
    pDJP->ulValue = orientation == nsIPrintSettings::kPortraitOrientation?DJP_ORI_PORTRAIT:DJP_ORI_LANDSCAPE;
    pDJP++;

    // Setup Number of Copies
    PRInt32 copies;
    aPrintSettings->GetNumCopies(&copies);
    pDJP->cb = sizeof(DJP_ITEM);
    pDJP->lType = DJP_CURRENT;
    pDJP->ulNumReturned = 1;
    pDJP->ulProperty = DJP_SJ_COPIES;
    pDJP->ulValue = copies;
    pDJP++;

    pDJP->cb = sizeof(DJP_ITEM);
    pDJP->lType = DJP_NONE;
    pDJP->ulNumReturned = 1;
    pDJP->ulProperty = 0;
    pDJP->ulValue = 0;

    LONG driverSize = nsDeviceContextSpecOS2::PrnDlg.GetPrintDriverSize(printer);
    GreEscape (hdc, DEVESC_SETJOBPROPERTIES, bufferSize, pDJP_Buffer, 
               &driverSize, PBYTE(nsDeviceContextSpecOS2::PrnDlg.GetPrintDriver(printer)));

    delete [] pDJP_Buffer;
    DevCloseDC(hdc);
  }
}

nsresult nsDeviceContextSpecOS2::SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, ULONG printer)
{
  if (aPrintSettings == nsnull)
    return NS_ERROR_FAILURE;

  int bufferSize = 3 * sizeof(DJP_ITEM);
  PBYTE pDJP_Buffer = new BYTE[bufferSize];
  memset(pDJP_Buffer, 0, bufferSize);
  PDJP_ITEM pDJP = (PDJP_ITEM) pDJP_Buffer;

  HDC hdc = nsDeviceContextSpecOS2::PrnDlg.GetDCHandle(printer);

  //Get Number of Copies from Job Properties
  pDJP->lType = DJP_CURRENT;
  pDJP->cb = sizeof(DJP_ITEM);
  pDJP->ulNumReturned = 1;
  pDJP->ulProperty = DJP_SJ_COPIES;
  pDJP->ulValue = 1;
  pDJP++;

  //Get Orientation from Job Properties
  pDJP->lType = DJP_CURRENT;
  pDJP->cb = sizeof(DJP_ITEM);
  pDJP->ulNumReturned = 1;
  pDJP->ulProperty = DJP_SJ_ORIENTATION;
  pDJP->ulValue = 1;
  pDJP++;

  pDJP->lType = DJP_NONE;
  pDJP->cb = sizeof(DJP_ITEM);
  pDJP->ulNumReturned = 1;
  pDJP->ulProperty = 0;
  pDJP->ulValue = 0;

  LONG driverSize = nsDeviceContextSpecOS2::PrnDlg.GetPrintDriverSize(printer);
  LONG rc = GreEscape(hdc, DEVESC_QUERYJOBPROPERTIES, bufferSize, pDJP_Buffer, 
                      &driverSize, PBYTE(nsDeviceContextSpecOS2::PrnDlg.GetPrintDriver(printer)));

  pDJP = (PDJP_ITEM) pDJP_Buffer;
  if ((rc == DEV_OK) || (rc == DEV_WARNING)) { 
    while (pDJP->lType != DJP_NONE) {
      if ((pDJP->ulProperty == DJP_SJ_ORIENTATION) && (pDJP->lType > 0)){
        if ((pDJP->ulValue == DJP_ORI_PORTRAIT) || (pDJP->ulValue == DJP_ORI_REV_PORTRAIT))
          aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
        else
         aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
      }
      if ((pDJP->ulProperty == DJP_SJ_COPIES) && (pDJP->lType > 0)){
        aPrintSettings->SetNumCopies(PRInt32(pDJP->ulValue));
      }
      pDJP = DJP_NEXT_STRUCTP(pDJP);
    }
  }
  
  delete [] pDJP_Buffer;
  DevCloseDC(hdc);  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecOS2 :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDeviceContextSpecIID))
  {
    nsIDeviceContextSpec* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

#ifdef USE_XPRINT
  if (aIID.Equals(kIDeviceContextSpecXPIID))
  {
    nsIDeviceContextSpecXp *tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
#endif /* USE_XPRINT */

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDeviceContextSpec* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDeviceContextSpecOS2)
NS_IMPL_RELEASE(nsDeviceContextSpecOS2)

  
/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecOS2
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 *
 * gisburn: Please note that this function exists as 1:1 copy in other
 * toolkits including:
 * - GTK+-toolkit:
 *   file:     mozilla/gfx/src/gtk/nsDeviceContextSpecG.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecGTK::Init(PRBool aPrintPreview )
 * - Xlib-toolkit: 
 *   file:     mozilla/gfx/src/xlib/nsDeviceContextSpecXlib.cpp 
 *   function: NS_IMETHODIMP nsDeviceContextSpecXlib::Init(PRBool aPrintPreview )
 * - Qt-toolkit:
 *   file:     mozilla/gfx/src/qt/nsDeviceContextSpecQT.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aPrintPreview )
 * 
 * ** Please update the other toolkits when changing this function.
 */
NS_IMETHODIMP nsDeviceContextSpecOS2::Init(nsIWidget *aWidget,
                                           nsIPrintSettings* aPS,
                                           PRBool aIsPrintPreview)
{
  nsresult rv = NS_ERROR_FAILURE;

  mPrintSettings = aPS;
  NS_ASSERTION(aPS, "Must have a PrintSettings!");

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }
 
  if (aPS) {
    PRBool     tofile         = PR_FALSE;
    PRInt32    copies         = 1;
    PRUnichar *printer        = nsnull;
    PRUnichar *printfile      = nsnull;

    mPrintSettings->GetPrinterName(&printer);
    mPrintSettings->GetToFileName(&printfile);
    mPrintSettings->GetPrintToFile(&tofile);
    mPrintSettings->GetNumCopies(&copies);

    if ((copies == 0)  ||  (copies > 999)) {
       GlobalPrinters::GetInstance()->FreeGlobalPrinters();
       return NS_ERROR_FAILURE;
    }

    if (printfile != nsnull) {
      // ToDo: Use LocalEncoding instead of UTF-8 (see bug 73446)
      strcpy(mPrData.path,    NS_ConvertUTF16toUTF8(printfile).get());
    }
    if (printer != nsnull) 
      strcpy(mPrData.printer, NS_ConvertUTF16toUTF8(printer).get());  

    if (aIsPrintPreview) 
      mPrData.destination = printPreview; 
    else if (tofile)  
      mPrData.destination = printToFile;
    else  
      mPrData.destination = printToPrinter;
    mPrData.copies = copies;

    rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
    if (NS_FAILED(rv))
      return rv;

    const nsAFlatString& printerUCS2 = NS_ConvertUTF8toUTF16(mPrData.printer);
    ULONG numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
    if (numPrinters) {
       for(ULONG i = 0; (i < numPrinters) && !mQueue; i++) {
          if ((GlobalPrinters::GetInstance()->GetStringAt(i)->Equals(printerUCS2, nsCaseInsensitiveStringComparator()))) {
             SetupDevModeFromSettings(i, aPS);
             mQueue = PrnDlg.SetPrinterQueue(i);
          }
       }
    }

    if (printfile != nsnull) 
      nsMemory::Free(printfile);
  
    if (printer != nsnull) 
      nsMemory::Free(printer);
  }

  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  return rv;
}


NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetDestination( int &aDestination )     
{
  aDestination = mPrData.destination;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetPrinterName ( char **aPrinter )
{
   *aPrinter = &mPrData.printer[0];
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetCopies ( int &aCopies )
{
   aCopies = mPrData.copies;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetPath ( char **aPath )      
{
  *aPath = &mPrData.path[0];
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetUserCancelled( PRBool &aCancel )     
{
  aCancel = mPrData.cancel;
  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecOS2 :: ClosePrintManager()
{
  return NS_OK;
}

nsresult nsDeviceContextSpecOS2::GetPRTQUEUE( PRTQUEUE *&p)
{
   p = mQueue;
   return NS_OK;
}

#ifdef MOZ_CAIRO_GFX
NS_IMETHODIMP nsDeviceContextSpecOS2::GetSurfaceForPrinter(gfxASurface **nativeSurface)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDeviceContextSpecOS2::BeginDocument(PRUnichar* aTitle,
                                                    PRUnichar* aPrintToFileName,
                                                    PRInt32 aStartPage,
                                                    PRInt32 aEndPage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDeviceContextSpecOS2::EndDocument()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

//  Printer Enumerator
nsPrinterEnumeratorOS2::nsPrinterEnumeratorOS2()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorOS2, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorOS2::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  if (aCount) 
    *aCount = 0;
  else 
    return NS_ERROR_NULL_POINTER;
  
  if (aResult) 
    *aResult = nsnull;
  else 
    return NS_ERROR_NULL_POINTER;

  nsDeviceContextSpecOS2::PrnDlg.RefreshPrintQueue();
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  ULONG numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(numPrinters * sizeof(PRUnichar*));
  if (!array && numPrinters > 0) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ULONG count = 0;
  while( count < numPrinters )
  {
    PRUnichar *str = ToNewUnicode(*GlobalPrinters::GetInstance()->GetStringAt(count));

    if (!str) {
      for (ULONG i = 0 ; i < count ; i++)
        nsMemory::Free(array[i]);
      
      nsMemory::Free(array);

      GlobalPrinters::GetInstance()->FreeGlobalPrinters();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[count++] = str;
    
  }
  *aCount = count;
  *aResult = array;
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorOS2::GetDefaultPrinterName(PRUnichar * *aDefaultPrinterName)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);
  GlobalPrinters::GetInstance()->GetDefaultPrinterName(*aDefaultPrinterName);
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorOS2::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
   NS_ENSURE_ARG_POINTER(aPrinterName);
   NS_ENSURE_ARG_POINTER(aPrintSettings);

   if (!*aPrinterName) 
     return NS_OK;

  if (NS_FAILED(GlobalPrinters::GetInstance()->InitializeGlobalPrinters())) 
    return NS_ERROR_FAILURE;

  ULONG numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  for(ULONG i = 0; i < numPrinters; i++) {
    if ((GlobalPrinters::GetInstance()->GetStringAt(i)->Equals(aPrinterName, nsCaseInsensitiveStringComparator()))) 
      nsDeviceContextSpecOS2::SetPrintSettingsFromDevMode(aPrintSettings, i);
  }

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  aPrintSettings->SetIsInitializedFromPrinter(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorOS2::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  ULONG numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  for(ULONG i = 0; i < numPrinters; i++) {
    if ((GlobalPrinters::GetInstance()->GetStringAt(i)->Equals(aPrinter, nsCaseInsensitiveStringComparator()))) {
       SetupDevModeFromSettings(i, aPrintSettings);
       if ( nsDeviceContextSpecOS2::PrnDlg.ShowProperties(i) ) {
          nsDeviceContextSpecOS2::SetPrintSettingsFromDevMode(aPrintSettings, i);
          return NS_OK;
       } else {
          return NS_ERROR_FAILURE;
       }
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated()) 
    return NS_OK;

  mGlobalNumPrinters = 0;
  mGlobalNumPrinters = nsDeviceContextSpecOS2::PrnDlg.GetNumPrinters();
  if (!mGlobalNumPrinters) 
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE; 

  mGlobalPrinterList = new nsStringArray();
  if (!mGlobalPrinterList) 
     return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  BOOL prefFailed = NS_FAILED(rv); // don't return on failure, optional feature

  for (ULONG i = 0; i < mGlobalNumPrinters; i++) {
    nsXPIDLCString printer;
    nsDeviceContextSpecOS2::PrnDlg.GetPrinter(i, getter_Copies(printer));

    nsAutoChar16Buffer printerName;
    PRInt32 printerNameLength;
    rv = MultiByteToWideChar(0, printer, strlen(printer),
                             printerName, printerNameLength);
    mGlobalPrinterList->AppendString(nsDependentString(printerName.get()));

    // store printer description in prefs for the print dialog
    if (!prefFailed) {
       nsCAutoString printerDescription;
       printerDescription = nsCAutoString(nsDeviceContextSpecOS2::PrnDlg.GetPrintDriver(i)->szDeviceName);
       printerDescription += " (";
       printerDescription += nsCAutoString(nsDeviceContextSpecOS2::PrnDlg.GetDriverType(i));
       printerDescription += ")";
       pPrefs->SetCharPref(nsPrintfCString(256,
                                           "print.printer_%s.printer_description",
                                           printer.get()).get(),
                           printerDescription.get());
    }
  } 
  return NS_OK;
}

void GlobalPrinters::GetDefaultPrinterName(PRUnichar*& aDefaultPrinterName)
{
  aDefaultPrinterName = nsnull;

  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) 
     return;

  if (GetNumPrinters() == 0)
     return;

  // the default printer is always index 0
  nsXPIDLCString printer;
  nsDeviceContextSpecOS2::PrnDlg.GetPrinter(0, getter_Copies(printer));

  nsAutoChar16Buffer printerName;
  PRInt32 printerNameLength;
  MultiByteToWideChar(0, printer, strlen(printer), printerName,
                      printerNameLength);
  aDefaultPrinterName = ToNewUnicode(nsDependentString(printerName.get()));

  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}

void GlobalPrinters::FreeGlobalPrinters()
{
  delete mGlobalPrinterList;
  mGlobalPrinterList = nsnull;
  mGlobalNumPrinters = 0;
}

