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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#include "nsDeviceContextSpecOS2.h"

#include "nsReadableUtils.h"
#include "nsISupportsArray.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"

nsStringArray* nsDeviceContextSpecOS2::globalPrinterList = nsnull;
int nsDeviceContextSpecOS2::globalNumPrinters = 0;

PRINTDLG nsDeviceContextSpecOS2::PrnDlg;

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecOS2
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecOS2 :: nsDeviceContextSpecOS2()
{
  NS_INIT_REFCNT();
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

int nsDeviceContextSpecOS2::InitializeGlobalPrinters ()
{
   globalNumPrinters = PrnDlg.GetNumPrinters();
   if (!globalNumPrinters) 
      return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE; 

   globalPrinterList = new nsStringArray();
   if (!globalPrinterList) 
      return NS_ERROR_OUT_OF_MEMORY;

   int defaultPrinter = PrnDlg.GetDefaultPrinter();

   for (int i = 0; i < globalNumPrinters; i++) {
     char *printer = PrnDlg.GetPrinter(i);
     if ( defaultPrinter == i ) 
        globalPrinterList->InsertStringAt(nsString(NS_ConvertASCIItoUCS2(printer)), 0);
     else 
        globalPrinterList->AppendString(nsString(NS_ConvertASCIItoUCS2(printer)));
   } 
   return NS_OK;
}


void nsDeviceContextSpecOS2::FreeGlobalPrinters ()
{
   delete globalPrinterList;
   globalPrinterList = nsnull;
   globalNumPrinters = 0;
}
   
   

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecOS2
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 *
 * gisburn: Please note that this function exists as 1:1 copy in other
 * toolkits including:
 * - GTK+-toolkit:
 *   file:     mozilla/gfx/src/gtk/nsDeviceContextSpecG.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecGTK::Init(PRBool aQuiet)
 * - Xlib-toolkit: 
 *   file:     mozilla/gfx/src/xlib/nsDeviceContextSpecXlib.cpp 
 *   function: NS_IMETHODIMP nsDeviceContextSpecXlib::Init(PRBool aQuiet)
 * - Qt-toolkit:
 *   file:     mozilla/gfx/src/qt/nsDeviceContextSpecQT.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aQuiet)
 * 
 * ** Please update the other toolkits when changing this function.
 */
NS_IMETHODIMP nsDeviceContextSpecOS2::Init(nsIPrintSettings* aPS, PRBool aQuiet)
{
  nsresult rv = NS_ERROR_FAILURE;

  mPrintSettings = aPS;
  NS_ASSERTION(aPS, "Must have a PrintSettings!");

  // if there is a current selection then enable the "Selection" radio button
  if (aPS != nsnull) {
    PRBool isOn;
    aPS->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
    }
  }

  char      *path;
  PRBool     canPrint       = PR_FALSE;
  PRBool     tofile         = PR_FALSE;
  PRInt16    printRange     = nsIPrintSettings::kRangeAllPages;
  PRInt32    fromPage       = 1;
  PRInt32    toPage         = 1;
  PRInt32    copies         = 1;
  PRUnichar *printer        = nsnull;
  PRUnichar *printfile      = nsnull;

  if( !globalPrinterList )
    if (InitializeGlobalPrinters())
       return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  if( globalNumPrinters && !globalPrinterList->Count() ) 
     return NS_ERROR_OUT_OF_MEMORY;
 
  if ( !aQuiet ) {
    rv = NS_ERROR_FAILURE;
    // create a nsISupportsArray of the parameters 
    // being passed to the window
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrintSettings> ps = aPS;
    nsCOMPtr<nsISupports> psSupports(do_QueryInterface(ps));
    NS_ASSERTION(psSupports, "PrintSettings must be a supports");
    array->AppendElement(psSupports);

    nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1"));
    if (ioParamBlock) {
      ioParamBlock->SetInt(0, 0);
      nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(ioParamBlock));
      NS_ASSERTION(blkSupps, "IOBlk must be a supports");

      array->AppendElement(blkSupps);
      nsCOMPtr<nsISupports> arguments(do_QueryInterface(array));
      NS_ASSERTION(array, "array must be a supports");

      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
      if (wwatch) {
        nsCOMPtr<nsIDOMWindow> active;
        wwatch->GetActiveWindow(getter_AddRefs(active));    
        nsCOMPtr<nsIDOMWindowInternal> parent = do_QueryInterface(active);

        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = wwatch->OpenWindow(parent, "chrome://global/content/printdialog.xul",
              "_blank", "chrome,modal,centerscreen", array,
              getter_AddRefs(newWindow));
      }
    }

    if (NS_SUCCEEDED(rv)) {
      PRInt32 buttonPressed = 0;
      ioParamBlock->GetInt(0, &buttonPressed);
      if (buttonPressed == 1) 
        canPrint = PR_TRUE;
      else 
        {
           FreeGlobalPrinters();
           return NS_ERROR_ABORT;
        }
    }
  } else {
    canPrint = PR_TRUE;
  }

  if (canPrint) {
    if (mPrintSettings) {
      mPrintSettings->GetPrinterName(&printer);
      mPrintSettings->GetPrintRange(&printRange);
      mPrintSettings->GetToFileName(&printfile);
      mPrintSettings->GetPrintToFile(&tofile);
      mPrintSettings->GetStartPageRange(&fromPage);
      mPrintSettings->GetEndPageRange(&toPage);
      mPrintSettings->GetNumCopies(&copies);

      if ((copies == 0)  ||  (copies > 999)) {
         FreeGlobalPrinters();
         return NS_ERROR_FAILURE;
      }

      if (printfile != nsnull) {
        // ToDo: Use LocalEncoding instead of UTF-8 (see bug 73446)
        strcpy(mPrData.path,    NS_ConvertUCS2toUTF8(printfile).get());
      }
      if (printer != nsnull) 
        strcpy(mPrData.printer, NS_ConvertUCS2toUTF8(printer).get());  
    }

    mPrData.toPrinter = !tofile;
    mPrData.copies = copies;

    if (globalNumPrinters) {
       for(int i = 0; (i < globalNumPrinters) && !mQueue; i++) {
          if (!(globalPrinterList->StringAt(i)->CompareWithConversion(mPrData.printer, TRUE, -1)))
             mQueue = PrnDlg.SetPrinterQueue(i);
       }
    }

    if (printfile != nsnull) 
      nsMemory::Free(printfile);
    
    if (printer != nsnull) 
      nsMemory::Free(printer);
    
    FreeGlobalPrinters();
    return NS_OK;
  }

  FreeGlobalPrinters();
  return rv;
}


NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetToPrinter( PRBool &aToPrinter )     
{
  aToPrinter = mPrData.toPrinter;
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
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorOS2, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorOS2::EnumeratePrintersExtended(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);
  *aCount = 0;
  *aResult = nsnull;
  return NS_OK;
}

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
  

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(nsDeviceContextSpecOS2::globalNumPrinters * sizeof(PRUnichar*));
  if (!array && nsDeviceContextSpecOS2::globalNumPrinters) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  int count = 0;
  while( count < nsDeviceContextSpecOS2::globalNumPrinters )
  {
    PRUnichar *str = ToNewUnicode(*nsDeviceContextSpecOS2::globalPrinterList->StringAt(count));

    if (!str) {
      for (int i = count - 1; i >= 0; i--) 
        nsMemory::Free(array[i]);
      
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[count++] = str;
    
  }
  *aCount = count;
  *aResult = array;

  return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorOS2::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
   for(int i = 0; i < nsDeviceContextSpecOS2::globalNumPrinters; i++) {
     if (!(nsDeviceContextSpecOS2::globalPrinterList->StringAt(i)->CompareWithConversion(aPrinter, TRUE, -1))) {
        if ( nsDeviceContextSpecOS2::PrnDlg.ShowProperties(i) ) 
           return NS_OK;
        else
           return NS_ERROR_FAILURE;
     }
   }
   return NS_ERROR_FAILURE;
}

//---------------------------------------------------------------------------
// OS/2 Printing   - was in libprint.cpp
//---------------------------------------------------------------------------
static HMODULE hmodRes;
static BOOL prnEscape (HDC hdc, long lEscape);

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

char* PRINTDLG::GetPrinter (int numPrinter)
{
   const char* pq = NULL;
  
   if (numPrinter > mQueueCount)
      return NULL;

   pq = mPQBuf [numPrinter]->PQI3().pszDriverName;

   return (char *)pq;
}

PRTQUEUE* PRINTDLG::SetPrinterQueue (int numPrinter)
{
   PRTQUEUE *pPQ = NULL;

   if (numPrinter > mQueueCount)
      return NULL;

   pPQ = mPQBuf [GetIndex(numPrinter)];

   return new PRTQUEUE (*pPQ);
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
            return(DPDM_ERROR);
    
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

HDC PrnOpenDC( PRTQUEUE *pInfo, PSZ pszApplicationName, int copies, int toPrinter, char *file )
{
   HDC hdc = 0;
   PSZ pszLogAddress;
   PSZ pszDataType;
   LONG dcType;
   DEVOPENSTRUC dop;

   if (!pInfo || !pszApplicationName)
      return hdc;

   char pszQueueProcParams[CCHMAXPATH] = "COP=";
   char numCopies[12];
   itoa (copies, numCopies, 10);
   strcat (pszQueueProcParams, numCopies);

   if ( toPrinter ) {
      pszLogAddress = pInfo->PQI3 ().pszName;
      pszDataType = "PM_Q_STD";
      dcType = OD_QUEUED;
   } else {
      if (file && strlen(file) != 0) 
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
    dop.pszQueueProcParams = pszQueueProcParams;   
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

BOOL prnEscape( HDC hdc, long lEscape)
{
   BOOL rc = FALSE;

   if( hdc)
   {
      long lDummy = 0;
      long lResult = ::DevEscape( hdc, lEscape, 0, NULL, &lDummy, NULL);
      rc = (lResult == DEV_OK);
   }

   return rc;
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

