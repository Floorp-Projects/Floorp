/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nsDeviceContextSpecOS2.h"

#include "nsReadableUtils.h"
#include "nsTArray.h"

#include "prenv.h" /* for PR_GetEnv */
#include "prtime.h"

#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"
#include "nsStringFwd.h"
#include "nsStringEnumerator.h"

#include "nsOS2Uni.h"

#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFileStreams.h"
#include "gfxPDFSurface.h"
#include "gfxOS2Surface.h"
#include "nsIPrintSettingsService.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

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

  bool      PrintersAreAllocated()       { return mGlobalPrinterList != nsnull; }
  PRUint32  GetNumPrinters()             { return mGlobalNumPrinters; }
  nsString* GetStringAt(PRInt32 aInx)    { return &mGlobalPrinterList->ElementAt(aInx); }
  void      GetDefaultPrinterName(PRUnichar*& aDefaultPrinterName);

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsTArray<nsString>* mGlobalPrinterList;
  static ULONG          mGlobalNumPrinters;

};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsTArray<nsString>* GlobalPrinters::mGlobalPrinterList = nsnull;
ULONG          GlobalPrinters::mGlobalNumPrinters = 0;
//---------------

nsDeviceContextSpecOS2::nsDeviceContextSpecOS2()
  : mQueue(nsnull), mPrintDC(nsnull), mPrintingStarted(false)
{
}

nsDeviceContextSpecOS2::~nsDeviceContextSpecOS2()
{
  if (mQueue)
    PrnClosePrinter(mQueue);
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecOS2, nsIDeviceContextSpec)

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

NS_IMETHODIMP nsDeviceContextSpecOS2::Init(nsIWidget *aWidget,
                                           nsIPrintSettings* aPS,
                                           bool aIsPrintPreview)
{
  nsresult rv = NS_ERROR_FAILURE;

  mPrintSettings = aPS;
  NS_ASSERTION(aPS, "Must have a PrintSettings!");

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }
 
  if (aPS) {
    bool       tofile         = false;
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

NS_IMETHODIMP nsDeviceContextSpecOS2 :: GetUserCancelled( bool &aCancel )     
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

NS_IMETHODIMP nsDeviceContextSpecOS2::GetSurfaceForPrinter(gfxASurface **surface)
{
  NS_ASSERTION(mQueue, "Queue can't be NULL here");

  nsRefPtr<gfxASurface> newSurface;

  PRInt16 outputFormat;
  mPrintSettings->GetOutputFormat(&outputFormat);
  int printerDest;
  GetDestination(printerDest);
  if (printerDest != printPreview) {
    // for now always set the output format to PDF, see bug 415522
    outputFormat = nsIPrintSettings::kOutputFormatPDF;
    mPrintSettings->SetOutputFormat(outputFormat); // save PDF format in settings
  }

  if (outputFormat == nsIPrintSettings::kOutputFormatPDF) {
    nsXPIDLString filename;
    mPrintSettings->GetToFileName(getter_Copies(filename));
    nsresult rv;
    if (filename.IsEmpty()) {
      // print to a file that is visible, like one on the Desktop
      nsCOMPtr<nsIFile> pdfLocation;
      rv = NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(pdfLocation));
      NS_ENSURE_SUCCESS(rv, rv);

      // construct a print output name using the current time, to make it
      // unique and not overwrite existing files
      char printName[CCHMAXPATH];
      PRExplodedTime time;
      PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &time);
      snprintf(printName, CCHMAXPATH-1, "%s_%04d%02d%02d_%02d%02d%02d.pdf",
               MOZ_APP_DISPLAYNAME, time.tm_year, time.tm_month+1, time.tm_mday,
               time.tm_hour, time.tm_min, time.tm_sec);
      printName[CCHMAXPATH-1] = '\0';

      nsCAutoString printString(printName);
      rv = pdfLocation->AppendNative(printString);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = pdfLocation->GetPath(filename);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#ifdef debug_thebes_print
    printf("nsDeviceContextSpecOS2::GetSurfaceForPrinter(): print to filename=%s\n",
           NS_LossyConvertUTF16toASCII(filename).get());
#endif

    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);
    // convert twips to points
    width  /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    rv = file->InitWithPath(filename);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    rv = stream->Init(file, -1, -1, 0);
    if (NS_FAILED(rv))
      return rv;

    newSurface = new(std::nothrow) gfxPDFSurface(stream, gfxSize(width, height));
  } else {
    int numCopies = 0;
    GetCopies(numCopies);
    char *filename = nsnull;
    if (printerDest == printToFile) {
      GetPath(&filename);
    }
    mPrintingStarted = true;
    mPrintDC = PrnOpenDC(mQueue, "Mozilla", numCopies, printerDest, filename);

    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);
#ifdef debug_thebes_print
    printf("nsDeviceContextSpecOS2::GetSurfaceForPrinter(): %fx%ftwips, copies=%d\n",
           width, height, numCopies);
#endif

    // we need pixels, so scale from twips to the printer resolution
    // and take into account that CAPS_*_RESOLUTION are in px/m, default
    // to approx. 100dpi
    double hDPI = 3937., vDPI = 3937.;
    LONG value;
    if (DevQueryCaps(mPrintDC, CAPS_HORIZONTAL_RESOLUTION, 1, &value))
      hDPI = value * 0.0254;
    if (DevQueryCaps(mPrintDC, CAPS_VERTICAL_RESOLUTION, 1, &value))
      vDPI = value * 0.0254;
    width = width * hDPI / 1440;
    height = height * vDPI / 1440;
#ifdef debug_thebes_print
    printf("nsDeviceContextSpecOS2::GetSurfaceForPrinter(): %fx%fpx (res=%fx%f)\n"
           "  expected size: %7.2f MiB\n",
           width, height, hDPI, vDPI, width*height*4./1024./1024.);
#endif

    // perhaps restrict to usable area
    // (this or scaling down won't help, we will just get more pages and still crash!)
    if (DevQueryCaps(mPrintDC, CAPS_WIDTH, 1, &value) && width > (double)value)
      width = (double)value;
    if (DevQueryCaps(mPrintDC, CAPS_HEIGHT, 1, &value) && height > (double)value)
      height = (double)value;

#ifdef debug_thebes_print
    printf("nsDeviceContextSpecOS2::GetSurfaceForPrinter(): capped? %fx%fpx (res=%fx%f)\n"
           "  expected size: %7.2f MiB per page\n",
           width, height, hDPI, vDPI, width*height*4./1024./1024.);
#endif

    // Now pass the created DC into the thebes surface for printing.
    // It gets destroyed there.
    newSurface = new(std::nothrow)
      gfxOS2Surface(mPrintDC, gfxIntSize(int(ceil(width)), int(ceil(height))));
  }
  if (!newSurface) {
    *surface = nsnull;
    return NS_ERROR_FAILURE;
  }
  *surface = newSurface;
  NS_ADDREF(*surface);
  return NS_OK;
}

// Helper function to convert the string to the native codepage,
// similar to UnicodeToCodepage() in nsDragService.cpp.
char *GetACPString(const PRUnichar* aStr)
{
   nsString str(aStr);
   if (str.Length() == 0) {
      return nsnull;
   }

   nsAutoCharBuffer buffer;
   PRInt32 bufLength;
   WideCharToMultiByte(0, PromiseFlatString(str).get(), str.Length(),
                       buffer, bufLength);
   return ToNewCString(nsDependentCString(buffer.Elements()));
}

NS_IMETHODIMP nsDeviceContextSpecOS2::BeginDocument(PRUnichar* aTitle,
                                                    PRUnichar* aPrintToFileName,
                                                    PRInt32 aStartPage,
                                                    PRInt32 aEndPage)
{
#ifdef debug_thebes_print
  printf("nsDeviceContextSpecOS2[%#x]::BeginPrinting(%s, %s)\n", (unsigned)this,
         NS_LossyConvertUTF16toASCII(nsString(aTitle)).get(),
         NS_LossyConvertUTF16toASCII(nsString(aPrintToFileName)).get());
#endif
  // don't try to send device escapes for non-native output (like PDF)
  PRInt16 outputFormat;
  mPrintSettings->GetOutputFormat(&outputFormat);
  if (outputFormat != nsIPrintSettings::kOutputFormatNative) {
    return NS_OK;
  }

  char *title = GetACPString(aTitle);
  PCSZ pszGenericDocName = "Mozilla Document";
  PCSZ pszDocName = title ? title : pszGenericDocName;
  LONG lResult = DevEscape(mPrintDC, DEVESC_STARTDOC,
                           strlen(pszDocName) + 1, const_cast<BYTE*>(pszDocName),
                           (PLONG)NULL, (PBYTE)NULL);
  mPrintingStarted = true;
  if (title) {
    nsMemory::Free(title);
  }

  return lResult == DEV_OK ? NS_OK : NS_ERROR_GFX_PRINTER_STARTDOC;
}

NS_IMETHODIMP nsDeviceContextSpecOS2::EndDocument()
{
  // don't try to send device escapes for non-native output (like PDF)
  // but clear the filename to make sure that we don't overwrite it next time
  PRInt16 outputFormat;
  mPrintSettings->GetOutputFormat(&outputFormat);
  if (outputFormat != nsIPrintSettings::kOutputFormatNative) {
    mPrintSettings->SetToFileName(NULL);
    nsCOMPtr<nsIPrintSettingsService> pss = do_GetService("@mozilla.org/gfx/printsettings-service;1");
    if (pss)
      pss->SavePrintSettingsToPrefs(mPrintSettings, true, nsIPrintSettings::kInitSaveToFileName);
    return NS_OK;
  }

  LONG lOutCount = 2;
  USHORT usJobID = 0;
  LONG lResult = DevEscape(mPrintDC, DEVESC_ENDDOC, 0L, (PBYTE)NULL,
                           &lOutCount, (PBYTE)&usJobID);
  return lResult == DEV_OK ? NS_OK : NS_ERROR_GFX_PRINTER_ENDDOC;
}

NS_IMETHODIMP nsDeviceContextSpecOS2::BeginPage()
{
  PRInt16 outputFormat;
  mPrintSettings->GetOutputFormat(&outputFormat);
  if (outputFormat != nsIPrintSettings::kOutputFormatNative) {
    return NS_OK;
  }

  if (mPrintingStarted) {
    // we don't want an extra page break at the start of the document
    mPrintingStarted = false;
    return NS_OK;
  }
  LONG lResult = DevEscape(mPrintDC, DEVESC_NEWFRAME, 0L, (PBYTE)NULL,
                           (PLONG)NULL, (PBYTE)NULL);
  return lResult == DEV_OK ? NS_OK : NS_ERROR_GFX_PRINTER_STARTPAGE;
}

NS_IMETHODIMP nsDeviceContextSpecOS2::EndPage()
{
  return NS_OK;
}

//  Printer Enumerator
nsPrinterEnumeratorOS2::nsPrinterEnumeratorOS2()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorOS2, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorOS2::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nsnull;

  nsDeviceContextSpecOS2::PrnDlg.RefreshPrintQueue();
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  ULONG numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ULONG count = 0;
  while( count < numPrinters )
  {
    printers->AppendElement(*GlobalPrinters::GetInstance()->GetStringAt(count++));
  }
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
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
  aPrintSettings->SetIsInitializedFromPrinter(true);
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

  mGlobalPrinterList = new nsTArray<nsString>();
  if (!mGlobalPrinterList) 
     return NS_ERROR_OUT_OF_MEMORY;

  // don't return on failure, optional feature
  BOOL prefFailed = (Preferences::GetRootBranch() == nsnull);

  for (ULONG i = 0; i < mGlobalNumPrinters; i++) {
    nsXPIDLCString printer;
    nsDeviceContextSpecOS2::PrnDlg.GetPrinter(i, getter_Copies(printer));

    nsAutoChar16Buffer printerName;
    PRInt32 printerNameLength;
    nsresult rv = MultiByteToWideChar(0, printer, strlen(printer),
                                      printerName, printerNameLength);
    mGlobalPrinterList->AppendElement(nsDependentString(printerName.Elements()));

    // store printer description in prefs for the print dialog
    if (!prefFailed) {
       nsCAutoString printerDescription;
       printerDescription = nsCAutoString(nsDeviceContextSpecOS2::PrnDlg.GetPrintDriver(i)->szDeviceName);
       printerDescription += " (";
       printerDescription += nsCAutoString(nsDeviceContextSpecOS2::PrnDlg.GetDriverType(i));
       printerDescription += ")";
       nsCAutoString prefName("print.printer_");
       prefName += printer;
       prefName += ".printer_description";
       Preferences::SetCString(prefName.get(), printerDescription);
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
  aDefaultPrinterName = ToNewUnicode(nsDependentString(printerName.Elements()));

  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}

void GlobalPrinters::FreeGlobalPrinters()
{
  delete mGlobalPrinterList;
  mGlobalPrinterList = nsnull;
  mGlobalNumPrinters = 0;
}

