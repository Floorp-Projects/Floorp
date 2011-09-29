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

#include "nsDeviceContextSpecWin.h"
#include "prmem.h"

#include <winspool.h>

#include <tchar.h>

#include "nsAutoPtr.h"
#include "nsIWidget.h"

#include "nsTArray.h"
#include "nsIPrintSettingsWin.h"

#include "nsString.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"

#include "gfxPDFSurface.h"
#include "gfxWindowsSurface.h"

#include "nsIFileStreams.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "mozilla/Services.h"

// For NS_CopyNativeToUnicode
#include "nsNativeCharsetUtils.h"

// File Picker
#include "nsILocalFile.h"
#include "nsIFile.h"
#include "nsIFilePicker.h"
#include "nsIStringBundle.h"
#define NS_ERROR_GFX_PRINTER_BUNDLE_URL "chrome://global/locale/printing.properties"

#include "prlog.h"
#ifdef PR_LOGGING 
PRLogModuleInfo * kWidgetPrintingLogMod = PR_NewLogModule("printing-widget");
#define PR_PL(_p1)  PR_LOG(kWidgetPrintingLogMod, PR_LOG_DEBUG, _p1)
#else
#define PR_PL(_p1)
#endif

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecWin
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecWin cleans it up
// If it gets created (via the Page Setup Dialog) but the user never prints anything
// then it will never be delete, so this class takes care of that.
class GlobalPrinters {
public:
  static GlobalPrinters* GetInstance() { return &mGlobalPrinters; }
  ~GlobalPrinters() { FreeGlobalPrinters(); }

  void FreeGlobalPrinters();

  bool         PrintersAreAllocated() { return mPrinters != nsnull; }
  LPWSTR       GetItemFromList(PRInt32 aInx) { return mPrinters?mPrinters->ElementAt(aInx):nsnull; }
  nsresult     EnumeratePrinterList();
  void         GetDefaultPrinterName(nsString& aDefaultPrinterName);
  PRUint32     GetNumPrinters() { return mPrinters?mPrinters->Length():0; }

protected:
  GlobalPrinters() {}
  nsresult EnumerateNativePrinters();
  void     ReallocatePrinters();

  static GlobalPrinters    mGlobalPrinters;
  static nsTArray<LPWSTR>* mPrinters;
};
//---------------
// static members
GlobalPrinters    GlobalPrinters::mGlobalPrinters;
nsTArray<LPWSTR>* GlobalPrinters::mPrinters = nsnull;


//******************************************************
// Define native paper sizes
//******************************************************
typedef struct {
  short  mPaperSize; // native enum
  double mWidth;
  double mHeight;
  bool mIsInches;
} NativePaperSizes;

// There are around 40 default print sizes defined by Windows
const NativePaperSizes kPaperSizes[] = {
  {DMPAPER_LETTER,    8.5,   11.0,  PR_TRUE},
  {DMPAPER_LEGAL,     8.5,   14.0,  PR_TRUE},
  {DMPAPER_A4,        210.0, 297.0, PR_FALSE},
  {DMPAPER_B4,        250.0, 354.0, PR_FALSE}, 
  {DMPAPER_B5,        182.0, 257.0, PR_FALSE},
  {DMPAPER_TABLOID,   11.0,  17.0,  PR_TRUE},
  {DMPAPER_LEDGER,    17.0,  11.0,  PR_TRUE},
  {DMPAPER_STATEMENT, 5.5,   8.5,   PR_TRUE},
  {DMPAPER_EXECUTIVE, 7.25,  10.5,  PR_TRUE},
  {DMPAPER_A3,        297.0, 420.0, PR_FALSE},
  {DMPAPER_A5,        148.0, 210.0, PR_FALSE},
  {DMPAPER_CSHEET,    17.0,  22.0,  PR_TRUE},  
  {DMPAPER_DSHEET,    22.0,  34.0,  PR_TRUE},  
  {DMPAPER_ESHEET,    34.0,  44.0,  PR_TRUE},  
  {DMPAPER_LETTERSMALL, 8.5, 11.0,  PR_TRUE},  
  {DMPAPER_A4SMALL,   210.0, 297.0, PR_FALSE}, 
  {DMPAPER_FOLIO,     8.5,   13.0,  PR_TRUE},
  {DMPAPER_QUARTO,    215.0, 275.0, PR_FALSE},
  {DMPAPER_10X14,     10.0,  14.0,  PR_TRUE},
  {DMPAPER_11X17,     11.0,  17.0,  PR_TRUE},
  {DMPAPER_NOTE,      8.5,   11.0,  PR_TRUE},  
  {DMPAPER_ENV_9,     3.875, 8.875, PR_TRUE},  
  {DMPAPER_ENV_10,    40.125, 9.5,  PR_TRUE},  
  {DMPAPER_ENV_11,    4.5,   10.375, PR_TRUE},  
  {DMPAPER_ENV_12,    4.75,  11.0,  PR_TRUE},  
  {DMPAPER_ENV_14,    5.0,   11.5,  PR_TRUE},  
  {DMPAPER_ENV_DL,    110.0, 220.0, PR_FALSE}, 
  {DMPAPER_ENV_C5,    162.0, 229.0, PR_FALSE}, 
  {DMPAPER_ENV_C3,    324.0, 458.0, PR_FALSE}, 
  {DMPAPER_ENV_C4,    229.0, 324.0, PR_FALSE}, 
  {DMPAPER_ENV_C6,    114.0, 162.0, PR_FALSE}, 
  {DMPAPER_ENV_C65,   114.0, 229.0, PR_FALSE}, 
  {DMPAPER_ENV_B4,    250.0, 353.0, PR_FALSE}, 
  {DMPAPER_ENV_B5,    176.0, 250.0, PR_FALSE}, 
  {DMPAPER_ENV_B6,    176.0, 125.0, PR_FALSE}, 
  {DMPAPER_ENV_ITALY, 110.0, 230.0, PR_FALSE}, 
  {DMPAPER_ENV_MONARCH,  3.875,  7.5, PR_TRUE},  
  {DMPAPER_ENV_PERSONAL, 3.625,  6.5, PR_TRUE},  
  {DMPAPER_FANFOLD_US,   14.875, 11.0, PR_TRUE},  
  {DMPAPER_FANFOLD_STD_GERMAN, 8.5, 12.0, PR_TRUE},  
  {DMPAPER_FANFOLD_LGL_GERMAN, 8.5, 13.0, PR_TRUE},  
};
const PRInt32 kNumPaperSizes = 41;

//----------------------------------------------------------------------------------
nsDeviceContextSpecWin::nsDeviceContextSpecWin()
{
  mDriverName    = nsnull;
  mDeviceName    = nsnull;
  mDevMode       = NULL;

}


//----------------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecWin, nsIDeviceContextSpec)

nsDeviceContextSpecWin::~nsDeviceContextSpecWin()
{
  SetDeviceName(nsnull);
  SetDriverName(nsnull);
  SetDevMode(NULL);

  nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(mPrintSettings));
  if (psWin) {
    psWin->SetDeviceName(nsnull);
    psWin->SetDriverName(nsnull);
    psWin->SetDevMode(NULL);
  }

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}


//------------------------------------------------------------------
// helper
static PRUnichar * GetDefaultPrinterNameFromGlobalPrinters()
{
  nsAutoString printerName;
  GlobalPrinters::GetInstance()->GetDefaultPrinterName(printerName);
  return ToNewUnicode(printerName);
}

//----------------------------------------------------------------
static nsresult 
EnumerateNativePrinters(DWORD aWhichPrinters, LPWSTR aPrinterName, bool& aIsFound, bool& aIsFile)
{
  DWORD             dwSizeNeeded = 0;
  DWORD             dwNumItems   = 0;
  LPPRINTER_INFO_2W  lpInfo        = NULL;

  // Get buffer size
  if (::EnumPrintersW(aWhichPrinters, NULL, 2, NULL, 0, &dwSizeNeeded,
                      &dwNumItems)) {
    return NS_ERROR_FAILURE;
  }

  // allocate memory
  lpInfo = (LPPRINTER_INFO_2W) malloc(dwSizeNeeded);
  if (!lpInfo) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (::EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, (LPBYTE)lpInfo,
                      dwSizeNeeded, &dwSizeNeeded, &dwNumItems) == 0) {
    free(lpInfo);
    return NS_OK;
  }

  for (DWORD i = 0; i < dwNumItems; i++ ) {
    if (wcscmp(lpInfo[i].pPrinterName, aPrinterName) == 0) {
      aIsFound = PR_TRUE;
      aIsFile  = wcscmp(lpInfo[i].pPortName, L"FILE:") == 0;
      break;
    }
  }

  free(lpInfo);
  return NS_OK;
}

//----------------------------------------------------------------
static void 
CheckForPrintToFileWithName(LPWSTR aPrinterName, bool& aIsFile)
{
  bool isFound = false;
  aIsFile = PR_FALSE;
  nsresult rv = EnumerateNativePrinters(PRINTER_ENUM_LOCAL, aPrinterName, isFound, aIsFile);
  if (isFound) return;

  rv = EnumerateNativePrinters(PRINTER_ENUM_NETWORK, aPrinterName, isFound, aIsFile);
  if (isFound) return;

  rv = EnumerateNativePrinters(PRINTER_ENUM_SHARED, aPrinterName, isFound, aIsFile);
  if (isFound) return;

  rv = EnumerateNativePrinters(PRINTER_ENUM_REMOTE, aPrinterName, isFound, aIsFile);
  if (isFound) return;
}

static nsresult 
GetFileNameForPrintSettings(nsIPrintSettings* aPS)
{
  // for testing
#ifdef DEBUG_rods
  return NS_OK;
#endif

  nsresult rv;

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(NS_ERROR_GFX_PRINTER_BUNDLE_URL, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString title;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("PrintToFile").get(), getter_Copies(title));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWindowWatcher> wwatch =
    (do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> window;
  wwatch->GetActiveWindow(getter_AddRefs(window));

  rv = filePicker->Init(window, title, nsIFilePicker::modeSave);
  NS_ENSURE_SUCCESS(rv, rv);
 
  rv = filePicker->AppendFilters(nsIFilePicker::filterAll);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar* fileName;
  aPS->GetToFileName(&fileName);

  if (fileName) {
    if (*fileName) {
      nsAutoString leafName;
      nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
      if (file) {
        rv = file->InitWithPath(nsDependentString(fileName));
        if (NS_SUCCEEDED(rv)) {
          file->GetLeafName(leafName);
          filePicker->SetDisplayDirectory(file);
        }
      }
      if (!leafName.IsEmpty()) {
        rv = filePicker->SetDefaultString(leafName);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsMemory::Free(fileName);
  }

  PRInt16 dialogResult;
  filePicker->Show(&dialogResult);

  if (dialogResult == nsIFilePicker::returnCancel) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsILocalFile> localFile;
  rv = filePicker->GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (dialogResult == nsIFilePicker::returnReplace) {
    // be extra safe and only delete when the file is really a file
    bool isFile;
    rv = localFile->IsFile(&isFile);
    if (NS_SUCCEEDED(rv) && isFile) {
      rv = localFile->Remove(PR_FALSE /* recursive delete */);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsAutoString unicodePath;
  rv = localFile->GetPath(unicodePath);
  NS_ENSURE_SUCCESS(rv,rv);

  if (unicodePath.IsEmpty()) {
    rv = NS_ERROR_ABORT;
  }

  if (NS_SUCCEEDED(rv)) aPS->SetToFileName(unicodePath.get());

  return rv;
}

//----------------------------------------------------------------------------------
static nsresult
CheckForPrintToFile(nsIPrintSettings* aPS, LPWSTR aPrinterName, PRUnichar* aUPrinterName)
{
  nsresult rv = NS_OK;

  if (!aPrinterName && !aUPrinterName) return rv;

  bool toFile;
  CheckForPrintToFileWithName(aPrinterName?aPrinterName:aUPrinterName, toFile);
  // Since the driver wasn't a "Print To File" Driver, check to see
  // if the name of the file has been set to the special "FILE:"
  if (!toFile) {
    nsXPIDLString toFileName;
    aPS->GetToFileName(getter_Copies(toFileName));
    if (toFileName) {
      if (*toFileName) {
        if (toFileName.EqualsLiteral("FILE:")) {
          // this skips the setting of the "print to file" info below
          // which we don't want to do.
          return NS_OK; 
        }
      }
    }
  }
  aPS->SetPrintToFile(toFile);
  if (toFile) {
    rv = GetFileNameForPrintSettings(aPS);
  }
  return rv;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP nsDeviceContextSpecWin::Init(nsIWidget* aWidget, 
                                           nsIPrintSettings* aPrintSettings,
                                           bool aIsPrintPreview)
{
  mPrintSettings = aPrintSettings;

  nsresult rv = aIsPrintPreview ? NS_ERROR_GFX_PRINTER_PRINTPREVIEW : 
                                  NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  if (aPrintSettings) {
    nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
    if (psWin) {
      PRUnichar* deviceName;
      PRUnichar* driverName;
      psWin->GetDeviceName(&deviceName); // creates new memory (makes a copy)
      psWin->GetDriverName(&driverName); // creates new memory (makes a copy)

      LPDEVMODEW devMode;
      psWin->GetDevMode(&devMode);       // creates new memory (makes a copy)

      if (deviceName && driverName && devMode) {
        // Scaling is special, it is one of the few
        // devMode items that we control in layout
        if (devMode->dmFields & DM_SCALE) {
          double scale = double(devMode->dmScale) / 100.0f;
          if (scale != 1.0) {
            aPrintSettings->SetScaling(scale);
            devMode->dmScale = 100;
          }
        }

        SetDeviceName(deviceName);
        SetDriverName(driverName);
        SetDevMode(devMode);

        if (!aIsPrintPreview) {
          rv = CheckForPrintToFile(mPrintSettings, deviceName, nsnull);
          if (NS_FAILED(rv)) {
            nsCRT::free(deviceName);
            nsCRT::free(driverName);
            return NS_ERROR_FAILURE;
          }
        }

        // clean up
        nsCRT::free(deviceName);
        nsCRT::free(driverName);

        return NS_OK;
      } else {
        PR_PL(("***** nsDeviceContextSpecWin::Init - deviceName/driverName/devMode was NULL!\n"));
        if (deviceName) nsCRT::free(deviceName);
        if (driverName) nsCRT::free(driverName);
        if (devMode) ::HeapFree(::GetProcessHeap(), 0, devMode);
      }
    }
  } else {
    PR_PL(("***** nsDeviceContextSpecWin::Init - aPrintSettingswas NULL!\n"));
  }

  // Get the Print Name to be used
  PRUnichar * printerName = nsnull;
  if (mPrintSettings) {
    mPrintSettings->GetPrinterName(&printerName);
  }

  // If there is no name then use the default printer
  if (!printerName || (printerName && !*printerName)) {
    printerName = GetDefaultPrinterNameFromGlobalPrinters();
  }

  NS_ASSERTION(printerName, "We have to have a printer name");
  if (!printerName || !*printerName) return rv;

  if (!aIsPrintPreview) {
    CheckForPrintToFile(mPrintSettings, nsnull, printerName);
  }
 
  return GetDataFromPrinter(printerName, mPrintSettings);
}

//----------------------------------------------------------
// Helper Function - Free and reallocate the string
static void CleanAndCopyString(PRUnichar*& aStr, const PRUnichar* aNewStr)
{
  if (aStr != nsnull) {
    if (aNewStr != nsnull && wcslen(aStr) > wcslen(aNewStr)) { // reuse it if we can
      wcscpy(aStr, aNewStr);
      return;
    } else {
      PR_Free(aStr);
      aStr = nsnull;
    }
  }

  if (nsnull != aNewStr) {
    aStr = (PRUnichar *)PR_Malloc(sizeof(PRUnichar)*(wcslen(aNewStr) + 1));
    wcscpy(aStr, aNewStr);
  }
}

NS_IMETHODIMP nsDeviceContextSpecWin::GetSurfaceForPrinter(gfxASurface **surface)
{
  NS_ASSERTION(mDevMode, "DevMode can't be NULL here");

  nsRefPtr<gfxASurface> newSurface;

  PRInt16 outputFormat = 0;
  if (mPrintSettings) {
    mPrintSettings->GetOutputFormat(&outputFormat);
  }

  if (outputFormat == nsIPrintSettings::kOutputFormatPDF) {
    nsXPIDLString filename;
    mPrintSettings->GetToFileName(getter_Copies(filename));

    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);
    // convert twips to points
    width  /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    nsresult rv = file->InitWithPath(filename);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    rv = stream->Init(file, -1, -1, 0);
    if (NS_FAILED(rv))
      return rv;

    newSurface = new gfxPDFSurface(stream, gfxSize(width, height));
  } else {
    if (mDevMode) {
      NS_WARN_IF_FALSE(mDriverName, "No driver!");
      HDC dc = ::CreateDCW(mDriverName, mDeviceName, NULL, mDevMode);

      // have this surface take over ownership of this DC
      newSurface = new gfxWindowsSurface(dc, gfxWindowsSurface::FLAG_TAKE_DC | gfxWindowsSurface::FLAG_FOR_PRINTING);
    }
  }

  if (newSurface) {
    *surface = newSurface;
    NS_ADDREF(*surface);
    return NS_OK;
  }

  *surface = nsnull;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDeviceName(const PRUnichar* aDeviceName)
{
  CleanAndCopyString(mDeviceName, aDeviceName);
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDriverName(const PRUnichar* aDriverName)
{
  CleanAndCopyString(mDriverName, aDriverName);
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDevMode(LPDEVMODEW aDevMode)
{
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
  }

  mDevMode = aDevMode;
}

//------------------------------------------------------------------
void 
nsDeviceContextSpecWin::GetDevMode(LPDEVMODEW &aDevMode)
{
  aDevMode = mDevMode;
}

//----------------------------------------------------------------------------------
// Map an incoming size to a Windows Native enum in the DevMode
static void 
MapPaperSizeToNativeEnum(LPDEVMODEW aDevMode,
                         PRInt16   aType, 
                         double    aW, 
                         double    aH)
{

#ifdef DEBUG_rods
  BOOL doingOrientation = aDevMode->dmFields & DM_ORIENTATION;
  BOOL doingPaperSize   = aDevMode->dmFields & DM_PAPERSIZE;
  BOOL doingPaperLength = aDevMode->dmFields & DM_PAPERLENGTH;
  BOOL doingPaperWidth  = aDevMode->dmFields & DM_PAPERWIDTH;
#endif

  for (PRInt32 i=0;i<kNumPaperSizes;i++) {
    if (kPaperSizes[i].mWidth == aW && kPaperSizes[i].mHeight == aH) {
      aDevMode->dmPaperSize = kPaperSizes[i].mPaperSize;
      aDevMode->dmFields &= ~DM_PAPERLENGTH;
      aDevMode->dmFields &= ~DM_PAPERWIDTH;
      aDevMode->dmFields |= DM_PAPERSIZE;
      return;
    }
  }

  short width  = 0;
  short height = 0;
  if (aType == nsIPrintSettings::kPaperSizeInches) {
    width  = short(NS_TWIPS_TO_MILLIMETERS(NS_INCHES_TO_TWIPS(float(aW))) / 10);
    height = short(NS_TWIPS_TO_MILLIMETERS(NS_INCHES_TO_TWIPS(float(aH))) / 10);

  } else if (aType == nsIPrintSettings::kPaperSizeMillimeters) {
    width  = short(aW / 10.0);
    height = short(aH / 10.0);
  } else {
    return; // don't set anything
  }

  // width and height is in 
  aDevMode->dmPaperSize   = 0;
  aDevMode->dmPaperWidth  = width;
  aDevMode->dmPaperLength = height;

  aDevMode->dmFields |= DM_PAPERSIZE;
  aDevMode->dmFields |= DM_PAPERLENGTH;
  aDevMode->dmFields |= DM_PAPERWIDTH;
}

//----------------------------------------------------------------------------------
// Setup Paper Size & Orientation options into the DevMode
// 
static void 
SetupDevModeFromSettings(LPDEVMODEW aDevMode, nsIPrintSettings* aPrintSettings)
{
  // Setup paper size
  if (aPrintSettings) {
    PRInt16 type;
    aPrintSettings->GetPaperSizeType(&type);
    if (type == nsIPrintSettings::kPaperSizeNativeData) {
      PRInt16 paperEnum;
      aPrintSettings->GetPaperData(&paperEnum);
      aDevMode->dmPaperSize = paperEnum;
      aDevMode->dmFields &= ~DM_PAPERLENGTH;
      aDevMode->dmFields &= ~DM_PAPERWIDTH;
      aDevMode->dmFields |= DM_PAPERSIZE;
    } else {
      PRInt16 unit;
      double width, height;
      aPrintSettings->GetPaperSizeUnit(&unit);
      aPrintSettings->GetPaperWidth(&width);
      aPrintSettings->GetPaperHeight(&height);
      MapPaperSizeToNativeEnum(aDevMode, unit, width, height);
    }

    // Setup Orientation
    PRInt32 orientation;
    aPrintSettings->GetOrientation(&orientation);
    aDevMode->dmOrientation = orientation == nsIPrintSettings::kPortraitOrientation?DMORIENT_PORTRAIT:DMORIENT_LANDSCAPE;
    aDevMode->dmFields |= DM_ORIENTATION;

    // Setup Number of Copies
    PRInt32 copies;
    aPrintSettings->GetNumCopies(&copies);
    aDevMode->dmCopies = copies;
    aDevMode->dmFields |= DM_COPIES;
  }

}

#define DISPLAY_LAST_ERROR 

//----------------------------------------------------------------------------------
// Setup the object's data member with the selected printer's data
nsresult
nsDeviceContextSpecWin::GetDataFromPrinter(const PRUnichar * aName, nsIPrintSettings* aPS)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!GlobalPrinters::GetInstance()->PrintersAreAllocated()) {
    rv = GlobalPrinters::GetInstance()->EnumeratePrinterList();
    if (NS_FAILED(rv)) {
      PR_PL(("***** nsDeviceContextSpecWin::GetDataFromPrinter - Couldn't enumerate printers!\n"));
      DISPLAY_LAST_ERROR
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  HANDLE hPrinter = NULL;
  
  BOOL status = ::OpenPrinterW((LPWSTR)(aName),
                              &hPrinter, NULL);
  if (status) {

    LPDEVMODEW   pDevMode;
    DWORD       dwNeeded, dwRet;

    // Allocate a buffer of the correct size.
    dwNeeded = ::DocumentPropertiesW(NULL, hPrinter,
                                    const_cast<wchar_t*>(aName),
                                    NULL, NULL, 0);

    pDevMode = (LPDEVMODEW)::HeapAlloc (::GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (!pDevMode) return NS_ERROR_FAILURE;

    // Get the default DevMode for the printer and modify it for our needs.
    dwRet = DocumentPropertiesW(NULL, hPrinter, 
                               const_cast<wchar_t*>(aName),
                               pDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet == IDOK && aPS) {
      SetupDevModeFromSettings(pDevMode, aPS);
      // Sets back the changes we made to the DevMode into the Printer Driver
      dwRet = ::DocumentPropertiesW(NULL, hPrinter,
                                   const_cast<wchar_t*>(aName),
                                   pDevMode, pDevMode,
                                   DM_IN_BUFFER | DM_OUT_BUFFER);
    }

    if (dwRet != IDOK) {
      ::HeapFree(::GetProcessHeap(), 0, pDevMode);
      ::ClosePrinter(hPrinter);
      PR_PL(("***** nsDeviceContextSpecWin::GetDataFromPrinter - DocumentProperties call failed code: %d/0x%x\n", dwRet, dwRet));
      DISPLAY_LAST_ERROR
      return NS_ERROR_FAILURE;
    }

    SetDevMode(pDevMode); // cache the pointer and takes responsibility for the memory

    SetDeviceName(aName);

    SetDriverName(L"WINSPOOL");

    ::ClosePrinter(hPrinter);
    rv = NS_OK;
  } else {
    rv = NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
    PR_PL(("***** nsDeviceContextSpecWin::GetDataFromPrinter - Couldn't open printer: [%s]\n", NS_ConvertUTF16toUTF8(aName).get()));
    DISPLAY_LAST_ERROR
  }
  return rv;
}

//----------------------------------------------------------------------------------
// Setup Paper Size options into the DevMode
// 
// When using a data member it may be a HGLOCAL or LPDEVMODE
// if it is a HGLOBAL then we need to "lock" it to get the LPDEVMODE
// and unlock it when we are done.
void 
nsDeviceContextSpecWin::SetupPaperInfoFromSettings()
{
  LPDEVMODEW devMode;

  GetDevMode(devMode);
  NS_ASSERTION(devMode, "DevMode can't be NULL here");
  if (devMode) {
    SetupDevModeFromSettings(devMode, mPrintSettings);
  }
}

//----------------------------------------------------------------------------------
// Helper Function - Free and reallocate the string
nsresult 
nsDeviceContextSpecWin::SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, 
                                                    LPDEVMODEW         aDevMode)
{
  if (aPrintSettings == nsnull) {
    return NS_ERROR_FAILURE;
  }
  aPrintSettings->SetIsInitializedFromPrinter(PR_TRUE);

  BOOL doingNumCopies   = aDevMode->dmFields & DM_COPIES;
  BOOL doingOrientation = aDevMode->dmFields & DM_ORIENTATION;
  BOOL doingPaperSize   = aDevMode->dmFields & DM_PAPERSIZE;
  BOOL doingPaperLength = aDevMode->dmFields & DM_PAPERLENGTH;
  BOOL doingPaperWidth  = aDevMode->dmFields & DM_PAPERWIDTH;

  if (doingOrientation) {
    PRInt32 orientation  = aDevMode->dmOrientation == DMORIENT_PORTRAIT?
      PRInt32(nsIPrintSettings::kPortraitOrientation):nsIPrintSettings::kLandscapeOrientation;
    aPrintSettings->SetOrientation(orientation);
  }

  // Setup Number of Copies
  if (doingNumCopies) {
    aPrintSettings->SetNumCopies(PRInt32(aDevMode->dmCopies));
  }

  if (aDevMode->dmFields & DM_SCALE) {
    double scale = double(aDevMode->dmScale) / 100.0f;
    if (scale != 1.0) {
      aPrintSettings->SetScaling(scale);
      aDevMode->dmScale = 100;
      // To turn this on you must change where the mPrt->mShrinkToFit is being set in the DocumentViewer
      //aPrintSettings->SetShrinkToFit(PR_FALSE);
    }
  }

  if (doingPaperSize) {
    aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeNativeData);
    aPrintSettings->SetPaperData(aDevMode->dmPaperSize);
    for (PRInt32 i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches
          ?PRInt16(nsIPrintSettings::kPaperSizeInches):nsIPrintSettings::kPaperSizeMillimeters);
        break;
      }
    }

  } else if (doingPaperLength && doingPaperWidth) {
    bool found = false;
    for (PRInt32 i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperWidth(kPaperSizes[i].mWidth);
        aPrintSettings->SetPaperHeight(kPaperSizes[i].mHeight);
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches
          ?PRInt16(nsIPrintSettings::kPaperSizeInches):nsIPrintSettings::kPaperSizeMillimeters);
        found = PR_TRUE;
        break;
      }
    }
    if (!found) {
      return NS_ERROR_FAILURE;
    }
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//***********************************************************
//  Printer Enumerator
//***********************************************************
nsPrinterEnumeratorWin::nsPrinterEnumeratorWin()
{
}

nsPrinterEnumeratorWin::~nsPrinterEnumeratorWin()
{
  // Do not free printers here
  // GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorWin, nsIPrinterEnumerator)

//----------------------------------------------------------------------------------
// Return the Default Printer name
/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP 
nsPrinterEnumeratorWin::GetDefaultPrinterName(PRUnichar * *aDefaultPrinterName)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

  *aDefaultPrinterName = GetDefaultPrinterNameFromGlobalPrinters(); // helper

  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP 
nsPrinterEnumeratorWin::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrinterName);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  if (!*aPrinterName) {
    return NS_OK;
  }

  nsRefPtr<nsDeviceContextSpecWin> devSpecWin = new nsDeviceContextSpecWin();
  if (!devSpecWin) return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(GlobalPrinters::GetInstance()->EnumeratePrinterList())) {
    return NS_ERROR_FAILURE;
  }

  devSpecWin->GetDataFromPrinter(aPrinterName);

  LPDEVMODEW devmode;
  devSpecWin->GetDevMode(devmode);
  NS_ASSERTION(devmode, "DevMode can't be NULL here");
  if (devmode) {
    aPrintSettings->SetPrinterName(aPrinterName);
    nsDeviceContextSpecWin::SetPrintSettingsFromDevMode(aPrintSettings, devmode);
  }

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  return NS_OK;
}


//----------------------------------------------------------------------------------
// Enumerate all the Printers from the global array and pass their
// names back (usually to script)
NS_IMETHODIMP 
nsPrinterEnumeratorWin::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nsnull;

  nsresult rv = GlobalPrinters::GetInstance()->EnumeratePrinterList();
  if (NS_FAILED(rv)) {
    PR_PL(("***** nsDeviceContextSpecWin::GetPrinterNameList - Couldn't enumerate printers!\n"));
    return rv;
  }

  PRUint32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 printerInx = 0;
  while( printerInx < numPrinters ) {
    LPWSTR name = GlobalPrinters::GetInstance()->GetItemFromList(printerInx++);
    printers->AppendElement(nsDependentString(name));
  }

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

//----------------------------------------------------------------------------------
// Display the AdvancedDocumentProperties for the selected Printer
NS_IMETHODIMP nsPrinterEnumeratorWin::DisplayPropertiesDlg(const PRUnichar *aPrinterName, nsIPrintSettings* aPrintSettings)
{
  // Implementation removed because it is unused
  return NS_OK;
}

//----------------------------------------------------------------------------------
//-- Global Printers
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// THe array hold the name and port for each printer
void 
GlobalPrinters::ReallocatePrinters()
{
  if (PrintersAreAllocated()) {
    FreeGlobalPrinters();
  }
  mPrinters = new nsTArray<LPWSTR>();
  NS_ASSERTION(mPrinters, "Printers Array is NULL!");
}

//----------------------------------------------------------------------------------
void 
GlobalPrinters::FreeGlobalPrinters()
{
  if (mPrinters != nsnull) {
    for (PRUint32 i=0;i<mPrinters->Length();i++) {
      free(mPrinters->ElementAt(i));
    }
    delete mPrinters;
    mPrinters = nsnull;
  }
}

//----------------------------------------------------------------------------------
nsresult 
GlobalPrinters::EnumerateNativePrinters()
{
  nsresult rv = NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  PR_PL(("-----------------------\n"));
  PR_PL(("EnumerateNativePrinters\n"));

  WCHAR szDefaultPrinterName[1024];    
  DWORD status = GetProfileStringW(L"devices", 0, L",",
                                   szDefaultPrinterName,
                                   NS_ARRAY_LENGTH(szDefaultPrinterName));
  if (status > 0) {
    DWORD count = 0;
    LPWSTR sPtr   = szDefaultPrinterName;
    LPWSTR ePtr   = szDefaultPrinterName + status;
    LPWSTR prvPtr = sPtr;
    while (sPtr < ePtr) {
      if (*sPtr == 0) {
        LPWSTR name = wcsdup(prvPtr);
        mPrinters->AppendElement(name);
        PR_PL(("Printer Name:    %s\n", prvPtr));
        prvPtr = sPtr+1;
        count++;
      }
      sPtr++;
    }
    rv = NS_OK;
  }
  PR_PL(("-----------------------\n"));
  return rv;
}

//------------------------------------------------------------------
// Uses the GetProfileString to get the default printer from the registry
void 
GlobalPrinters::GetDefaultPrinterName(nsString& aDefaultPrinterName)
{
  aDefaultPrinterName.Truncate();
  WCHAR szDefaultPrinterName[1024];    
  DWORD status = GetProfileStringW(L"windows", L"device", 0,
                                   szDefaultPrinterName,
                                   NS_ARRAY_LENGTH(szDefaultPrinterName));
  if (status > 0) {
    WCHAR comma = ',';
    LPWSTR sPtr = szDefaultPrinterName;
    while (*sPtr != comma && *sPtr != 0) 
      sPtr++;
    if (*sPtr == comma) {
      *sPtr = 0;
    }
    aDefaultPrinterName = szDefaultPrinterName;
  } else {
    aDefaultPrinterName = EmptyString();
  }

  PR_PL(("DEFAULT PRINTER [%s]\n", aDefaultPrinterName.get()));
}

//----------------------------------------------------------------------------------
// This goes and gets the list of available printers and puts
// the default printer at the beginning of the list
nsresult 
GlobalPrinters::EnumeratePrinterList()
{
  // reallocate and get a new list each time it is asked for
  // this deletes the list and re-allocates them
  ReallocatePrinters();

  // any of these could only fail with an OUT_MEMORY_ERROR
  // PRINTER_ENUM_LOCAL should get the network printers on Win95
  nsresult rv = EnumerateNativePrinters();
  if (NS_FAILED(rv)) return rv;

  // get the name of the default printer
  nsAutoString defPrinterName;
  GetDefaultPrinterName(defPrinterName);

  // put the default printer at the beginning of list
  if (!defPrinterName.IsEmpty()) {
    for (PRUint32 i=0;i<mPrinters->Length();i++) {
      LPWSTR name = mPrinters->ElementAt(i);
      if (defPrinterName.Equals(name)) {
        if (i > 0) {
          LPWSTR ptr = mPrinters->ElementAt(0);
          mPrinters->ElementAt(0) = name;
          mPrinters->ElementAt(i) = ptr;
        }
        break;
      }
    }
  }

  // make sure we at least tried to get the printers
  if (!PrintersAreAllocated()) {
    PR_PL(("***** nsDeviceContextSpecWin::EnumeratePrinterList - Printers aren`t allocated\n"));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

