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

#include "prenv.h" /* for PR_GetEnv */

#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"

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
  static int            mGlobalNumPrinters;

};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsStringArray* GlobalPrinters::mGlobalPrinterList = nsnull;
int            GlobalPrinters::mGlobalNumPrinters = 0;
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

void SetupDevModeFromSettings(int printer, nsIPrintSettings* aPrintSettings)
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
    LONG rc = GreEscape (hdc, DEVESC_SETJOBPROPERTIES, bufferSize, pDJP_Buffer, 
                         &driverSize, PBYTE(nsDeviceContextSpecOS2::PrnDlg.GetPrintDriver(printer)));

    delete [] pDJP_Buffer;
    DevCloseDC(hdc);
  }
}

nsresult nsDeviceContextSpecOS2::SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, int printer)
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
NS_IMETHODIMP nsDeviceContextSpecOS2::Init(nsIPrintSettings* aPS, PRBool aIsPrintPreview)
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
      strcpy(mPrData.path,    NS_ConvertUCS2toUTF8(printfile).get());
    }
    if (printer != nsnull) 
      strcpy(mPrData.printer, NS_ConvertUCS2toUTF8(printer).get());  

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

    const nsAFlatString& printerUCS2 = NS_ConvertUTF8toUCS2(mPrData.printer);
    int numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
    if (numPrinters) {
       for(int i = 0; (i < numPrinters) && !mQueue; i++) {
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

  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(numPrinters * sizeof(PRUnichar*));
  if (!array && numPrinters > 0) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int count = 0;
  while( count < numPrinters )
  {
    PRUnichar *str = ToNewUnicode(*GlobalPrinters::GetInstance()->GetStringAt(count));

    if (!str) {
      for (int i = count - 1; i >= 0; i--) 
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

  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  for(int i = 0; i < numPrinters; i++) {
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

  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();

  for(int i = 0; i < numPrinters; i++) {
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

  int defaultPrinter = nsDeviceContextSpecOS2::PrnDlg.GetDefaultPrinter();
  for (int i = 0; i < mGlobalNumPrinters; i++) {
    nsXPIDLCString printer;
    nsDeviceContextSpecOS2::PrnDlg.GetPrinter(i, getter_Copies(printer));

    nsAutoChar16Buffer printerName;
    PRInt32 printerNameLength;
    nsresult rv = MultiByteToWideChar(0, printer, strlen(printer),
                                      printerName, printerNameLength);
    if (defaultPrinter == i) {
       mGlobalPrinterList->InsertStringAt(nsDependentString(printerName.get()), 0);
    } else {
       mGlobalPrinterList->AppendString(nsDependentString(printerName.get()));
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

  int defaultPrinter = nsDeviceContextSpecOS2::PrnDlg.GetDefaultPrinter();
  nsXPIDLCString printer;
  nsDeviceContextSpecOS2::PrnDlg.GetPrinter(defaultPrinter, getter_Copies(printer));

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


//---------------------------------------------------------------------------
// OS/2 Printing   - was in libprint.cpp
//---------------------------------------------------------------------------
static HMODULE hmodRes;

#define SHIFT_PTR(ptr,offset) ( *((LONG*)&ptr) += offset )


class PRTQUEUE
{
public:
   PRTQUEUE (const PRQINFO3* pPQI3)  { InitWithPQI3 (pPQI3); }
   PRTQUEUE (const PRTQUEUE& PQInfo);
  ~PRTQUEUE (void) { free (mpPQI3); }

   PRQINFO3& PQI3 () const { return *mpPQI3; }
   const char* DriverName () const { return mDriverName; }
   const char* DeviceName () const { return mDeviceName; }
   const char* PrinterName() const { return mPrinterName; }
   const char* QueueName  () const { return mpPQI3->pszComment; }
   
private:
   PRTQUEUE& operator = (const PRTQUEUE& z);        // prevent copying
   void InitWithPQI3 (const PRQINFO3* pInfo);

   PRQINFO3* mpPQI3;
   unsigned  mPQI3BufSize;
   char mDriverName  [DRIV_NAME_SIZE + 1];          // Driver name
   char mDeviceName  [DRIV_DEVICENAME_SIZE + 1];    // Device name
   char mPrinterName [PRINTERNAME_SIZE + 1];        // Printer name
};


PRTQUEUE::PRTQUEUE (const PRTQUEUE& PQInfo)
{
   mPQI3BufSize = PQInfo.mPQI3BufSize;
   mpPQI3 = (PRQINFO3*)malloc (mPQI3BufSize);
   memcpy (mpPQI3, PQInfo.mpPQI3, mPQI3BufSize);    // Copy entire buffer

   long Diff = (long)mpPQI3 - (long)PQInfo.mpPQI3;  // Calculate the difference between addresses
   SHIFT_PTR (mpPQI3->pszName,       Diff);         // Modify internal pointers accordingly
   SHIFT_PTR (mpPQI3->pszSepFile,    Diff);
   SHIFT_PTR (mpPQI3->pszPrProc,     Diff);
   SHIFT_PTR (mpPQI3->pszParms,      Diff);
   SHIFT_PTR (mpPQI3->pszComment,    Diff);
   SHIFT_PTR (mpPQI3->pszPrinters,   Diff);
   SHIFT_PTR (mpPQI3->pszDriverName, Diff);
   SHIFT_PTR (mpPQI3->pDriverData,   Diff);

   strcpy (mDriverName, PQInfo.mDriverName);
   strcpy (mDeviceName, PQInfo.mDeviceName);
   strcpy (mPrinterName, PQInfo.mPrinterName);
}

void PRTQUEUE::InitWithPQI3 (const PRQINFO3* pInfo)
{
   // Make local copy of PPRQINFO3 object
   ULONG SizeNeeded;
   ::SplQueryQueue (NULL, pInfo->pszName, 3, NULL, 0, &SizeNeeded);
   mpPQI3 = (PRQINFO3*)malloc (SizeNeeded);
   ::SplQueryQueue (NULL, pInfo->pszName, 3, mpPQI3, SizeNeeded, &SizeNeeded);

   mPQI3BufSize = SizeNeeded;

   PCHAR sep = strchr (pInfo->pszDriverName, '.');

   if (sep)
   {
      *sep = '\0';
      strcpy (mDriverName, pInfo->pszDriverName);
      strcpy (mDeviceName, sep + 1);
      *sep = '.';
   } else
   {
      strcpy (mDriverName, pInfo->pszDriverName);
      mDeviceName [0] = '\0';
   }


   sep = strchr (pInfo->pszPrinters, ',');

   if (sep)
   {
      *sep = '\0';
      strcpy (mPrinterName, pInfo->pszPrinters);
      *sep = '.';
   } else
   {
      strcpy (mPrinterName, pInfo->pszPrinters);
   }
}


//===========================================================================

PRINTDLG::PRINTDLG ()
{
  mQueueCount = 0;
  mDefaultQueue = 0;

  ULONG TotalQueues = 0;
  ULONG MemNeeded = 0;
  SPLERR rc;
  
  rc = ::SplEnumQueue (NULL, 3, NULL, 0, &mQueueCount, &TotalQueues, &MemNeeded, NULL);
  PRQINFO3* pPQI3Buf = (PRQINFO3*) malloc (MemNeeded);
  rc = ::SplEnumQueue (NULL, 3, pPQI3Buf, MemNeeded, &mQueueCount, &TotalQueues, &MemNeeded, NULL);

  if (mQueueCount > MAX_PRINT_QUEUES)
    mQueueCount = MAX_PRINT_QUEUES;

  for (ULONG cnt = 0 ; cnt < mQueueCount ; cnt++)
  {
    if (pPQI3Buf [cnt].fsType & PRQ3_TYPE_APPDEFAULT)
      mDefaultQueue = cnt;
    
    mPQBuf [cnt] = new PRTQUEUE (&pPQI3Buf [cnt]);
  }

  free (pPQI3Buf);
}

PRINTDLG::~PRINTDLG ()
{
  for (int cnt = 0 ; cnt < mQueueCount ; cnt++)
    delete mPQBuf [cnt];
}

void PRINTDLG::RefreshPrintQueue()
{
  ULONG newQueueCount = 0;
  ULONG TotalQueues = 0;
  ULONG MemNeeded = 0;
  mDefaultQueue = 0;
  SPLERR rc;
  
  rc = ::SplEnumQueue (NULL, 3, NULL, 0, &newQueueCount, &TotalQueues, &MemNeeded, NULL);
  PRQINFO3* pPQI3Buf = (PRQINFO3*) malloc (MemNeeded);
  rc = ::SplEnumQueue (NULL, 3, pPQI3Buf, MemNeeded, &newQueueCount, &TotalQueues, &MemNeeded, NULL);

  if (newQueueCount > MAX_PRINT_QUEUES)
    newQueueCount = MAX_PRINT_QUEUES;

  PRTQUEUE* tmpBuf[MAX_PRINT_QUEUES];

  for (ULONG cnt = 0 ; cnt < newQueueCount ; cnt++)
  {
    if (pPQI3Buf [cnt].fsType & PRQ3_TYPE_APPDEFAULT)
      mDefaultQueue = cnt;

    BOOL found = FALSE;
    for (ULONG j = 0 ; j < mQueueCount && !found; j++)
    {
       //Compare printer from requeried list with what's already in Mozilla's printer list(mPQBuf)
       //If printer is already there, use current properties; otherwise create a new printer in list
       if (mPQBuf[j] != 0) {
         if ((strcmp(pPQI3Buf[cnt].pszPrinters, mPQBuf[j]->PrinterName()) == 0) && 
             (strcmp(pPQI3Buf[cnt].pszDriverName, mPQBuf[j]->PQI3().pszDriverName) == 0)) {
           found = TRUE;
           tmpBuf[cnt] = mPQBuf[j];
           mPQBuf[j] = 0;
         }
       }
    }
    if (!found) 
       tmpBuf[cnt] = new PRTQUEUE (&pPQI3Buf[cnt]); 
  }

  for (int i=0; i < newQueueCount; i++) {
    if (mPQBuf[i] != 0)
      delete(mPQBuf[i]);
    mPQBuf[i] = tmpBuf[i];
  }

  if (mQueueCount > newQueueCount)
    for (int i = newQueueCount; i < mQueueCount; i++)
       if (mPQBuf[i] != 0)
         delete(mPQBuf[i]);

  mQueueCount = newQueueCount;
  free (pPQI3Buf);
}

int PRINTDLG::GetIndex (int numPrinter)
{
   int index;
   
   if (numPrinter == 0)
      index = mDefaultQueue;
   else if (numPrinter > mDefaultQueue)
      index = numPrinter;
   else
      index = numPrinter - 1;

   return index;
}

int PRINTDLG::GetNumPrinters ()
{
   return mQueueCount;
}

int PRINTDLG::GetDefaultPrinter ()
{
   return mDefaultQueue;
}

void PRINTDLG::GetPrinter (int numPrinter, char** printerName)
{
   if (numPrinter > mQueueCount)
      return;
 
   nsCAutoString pName(mPQBuf [numPrinter]->QueueName());
 
   pName.ReplaceChar('\r', ' ');
   pName.StripChars("\n");
   *printerName = ToNewCString(pName);
}

PRTQUEUE* PRINTDLG::SetPrinterQueue (int numPrinter)
{
   PRTQUEUE *pPQ = NULL;

   if (numPrinter > mQueueCount)
      return NULL;

   pPQ = mPQBuf [GetIndex(numPrinter)];

   return new PRTQUEUE (*pPQ);
}

LONG PRINTDLG::GetPrintDriverSize (int printer)
{
   return mPQBuf[GetIndex(printer)]->PQI3().pDriverData->cb;
}

PDRIVDATA PRINTDLG::GetPrintDriver (int printer)
{
   if (printer > mQueueCount)
      return NULL;

   return mPQBuf[GetIndex(printer)]->PQI3().pDriverData;
}

HDC PRINTDLG::GetDCHandle (int numPrinter)
{
    HDC hdc = 0;
    int index = GetIndex(numPrinter);
    DEVOPENSTRUC dop;

    dop.pszLogAddress      = 0; 
    dop.pszDriverName      = (char *) mPQBuf[index]->DriverName();
    dop.pdriv              = mPQBuf[index]->PQI3 ().pDriverData;
    dop.pszDataType        = 0; 
    dop.pszComment         = 0;
    dop.pszQueueProcName   = 0;     
    dop.pszQueueProcParams = 0;   
    dop.pszSpoolerParams   = 0;     
    dop.pszNetworkParams   = 0;     

    hdc = ::DevOpenDC( 0, OD_INFO, "*", 9, (PDEVOPENDATA) &dop, NULLHANDLE);
    return hdc;
}

char* PRINTDLG::GetDriverType (int printer)
{
  return (char *)mPQBuf[GetIndex(printer)]->DriverName ();
}

BOOL PRINTDLG::ShowProperties (int index)
{
    BOOL          rc = FALSE;
    ULONG         devrc = FALSE;
    PDRIVDATA     pOldDrivData;
    PDRIVDATA     pNewDrivData = NULL;
    LONG          buflen;
    int           Ind = GetIndex(index);

/* check size of buffer required for job properties */
    buflen = DevPostDeviceModes( 0 /*hab*/,
                                 NULL,
                                 mPQBuf[Ind]->DriverName (),
                                 mPQBuf[Ind]->DeviceName (),
                                 mPQBuf[Ind]->PrinterName (),
                                 DPDM_POSTJOBPROP);

/* return error to caller */
    if (buflen <= 0)
        return(buflen);

/* allocate some memory for larger job properties and */
/* return error to caller */

    if (buflen != mPQBuf[Ind]->PQI3().pDriverData->cb)
    {
        if (DosAllocMem((PPVOID)&pNewDrivData,buflen,fALLOC))
            return(FALSE); // DPDM_ERROR
    
/* copy over old data so driver can use old job */
/* properties as base for job properties dialog */
        pOldDrivData = mPQBuf[Ind]->PQI3().pDriverData;
        mPQBuf[Ind]->PQI3().pDriverData = pNewDrivData;
        memcpy( (PSZ)pNewDrivData, (PSZ)pOldDrivData, pOldDrivData->cb );
    }

/* display job properties dialog and get updated */
/* job properties from driver */

    devrc = DevPostDeviceModes( 0 /*hab*/,
                                mPQBuf[Ind]->PQI3().pDriverData,
                                mPQBuf[Ind]->DriverName (),
                                mPQBuf[Ind]->DeviceName (),
                                mPQBuf[Ind]->PrinterName (),
                                DPDM_POSTJOBPROP);
    rc = (devrc != DPDM_ERROR);
    return rc;
}

/****************************************************************************/
/*  Job management                                                          */
/****************************************************************************/

HDC PrnOpenDC( PRTQUEUE *pInfo, PSZ pszApplicationName, int copies, int destination, char *file )
{
   HDC hdc = 0;
   PSZ pszLogAddress;
   PSZ pszDataType;
   LONG dcType;
   DEVOPENSTRUC dop;

   if (!pInfo || !pszApplicationName)
      return hdc;

   if ( destination ) {
      pszLogAddress = pInfo->PQI3 ().pszName;
      pszDataType = "PM_Q_STD";
      if ( destination == 2 )
         dcType = OD_METAFILE;
      else
         dcType = OD_QUEUED;
   } else {
      if (file && *file)
         pszLogAddress = (PSZ) file;
      else    
         pszLogAddress = "FILE";
      pszDataType = "PM_Q_RAW";
      dcType = OD_DIRECT;
   } 

    dop.pszLogAddress      = pszLogAddress; 
    dop.pszDriverName      = (char*)pInfo->DriverName ();
    dop.pdriv              = pInfo->PQI3 ().pDriverData;
    dop.pszDataType        = pszDataType; 
    dop.pszComment         = pszApplicationName;
    dop.pszQueueProcName   = pInfo->PQI3 ().pszPrProc;     
    dop.pszQueueProcParams = 0;
    dop.pszSpoolerParams   = 0;     
    dop.pszNetworkParams   = 0;     

    hdc = ::DevOpenDC( 0, dcType, "*", 9, (PDEVOPENDATA) &dop, NULLHANDLE);

if (hdc == 0)
{
  ULONG ErrorCode = ERRORIDERROR (::WinGetLastError (0));
#ifdef DEBUG
  printf ("!ERROR! - Can't open DC for printer %04X\a\n", ErrorCode);
#endif
}   

   return hdc;
}

/* find the selected form */
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo)
{
   BOOL rc = FALSE;

   if( hdc && pHCInfo)
   {
      PHCINFO pBuffer;
      long    lAvail, i;

      /* query how many forms are available */
      lAvail = ::DevQueryHardcopyCaps( hdc, 0, 0, NULL);

      pBuffer = (PHCINFO) malloc( lAvail * sizeof(HCINFO));

      ::DevQueryHardcopyCaps( hdc, 0, lAvail, pBuffer);

      for( i = 0; i < lAvail; i++)
         if( pBuffer[ i].flAttributes & HCAPS_CURRENT)
         {
            memcpy( pHCInfo, pBuffer + i, sizeof(HCINFO));
            rc = TRUE;
            break;
         }

      free( pBuffer);
   }

   return rc;
}


/****************************************************************************/
/*  Library-level data and functions    -Printing                           */
/****************************************************************************/

BOOL PrnInitialize( HMODULE hmodResources)
{
   hmodRes = hmodResources;
   return TRUE;
}

BOOL PrnTerminate()
{
   /* nop for now, may do something eventually */
   return TRUE;
}

BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue)
{
   BOOL rc = FALSE;

   if (pPrintQueue)
   {
      delete pPrintQueue;
      rc = TRUE;
   }

   return rc;
}

