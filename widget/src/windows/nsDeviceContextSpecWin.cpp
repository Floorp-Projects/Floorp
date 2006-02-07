/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_REQUIRE_CURRENT_SDK
#undef WINVER
#define WINVER 0x0500
#endif

#include "nsDeviceContextSpecWin.h"
#include "prmem.h"
#include "plstr.h"
#include <windows.h>
#include <TCHAR.H>

#include <unknwn.h>
#include <commdlg.h>

#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsGfxCIID.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);
#include "nsIPromptService.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include <windows.h>
#include <winspool.h> 

// For Localization
#include "nsIStringBundle.h"
#include "nsDeviceContext.h"
#include "nsDeviceContextWin.h"

// This is for extending the dialog
#include <dlgs.h>

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

// For PrintDlgEx
// needed because there are unicode/ansi versions of this routine
// and we need to make sure we get the correct one.
#ifdef UNICODE
#define GetPrintDlgExQuoted "PrintDlgExW"
#else
#define GetPrintDlgExQuoted "PrintDlgExA"
#endif

// Default labels for the radio buttons
const char* kAsLaidOutOnScreenStr = "As &laid out on the screen";
const char* kTheSelectedFrameStr  = "The selected &frame";
const char* kEachFrameSeparately  = "&Each frame separately";


//-----------------------------------------------
// Global Data
//-----------------------------------------------
// Identifies which new radio btn was cliked on
static UINT gFrameSelectedRadioBtn = 0;

// Indicates whether the native print dialog was successfully extended
static PRPackedBool gDialogWasExtended     = PR_FALSE;

#define PRINTDLG_PROPERTIES "chrome://communicator-platform/locale/gfx/printdialog.properties"

static HWND gParentWnd = NULL;

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

  PRBool       PrintersAreAllocated() { return mPrinters != nsnull; }
  LPTSTR       GetItemFromList(PRInt32 aInx) { return mPrinters?(LPTSTR)mPrinters->ElementAt(aInx):nsnull; }
  nsresult     EnumeratePrinterList();
  void         GetDefaultPrinterName(LPTSTR& aDefaultPrinterName);
  PRInt32      GetNumPrinters() { return mPrinters?mPrinters->Count():0; }

protected:
  GlobalPrinters() {}
  nsresult EnumerateNativePrinters();
  void     ReallocatePrinters();

  static GlobalPrinters mGlobalPrinters;
  static nsVoidArray*   mPrinters;
};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsVoidArray*   GlobalPrinters::mPrinters = nsnull;


//******************************************************
// Define native paper sizes
//******************************************************
typedef struct {
  short  mPaperSize; // native enum
  double mWidth;
  double mHeight;
  PRBool mIsInches;
} NativePaperSizes;

// There are around 40 default print sizes defined by Windows
const NativePaperSizes kPaperSizes[] = {
  {DMPAPER_LETTER,    8.5,   11.0,  PR_TRUE},
  {DMPAPER_LEGAL,     8.5,   14.0,  PR_TRUE},
  {DMPAPER_A4,        210.0, 297.0, PR_FALSE},
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
  {DMPAPER_B4,        250.0, 354.0, PR_FALSE}, 
  {DMPAPER_B5,        182.0, 257.0, PR_FALSE},
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
nsDeviceContextSpecWin::nsDeviceContextSpecWin() :
  mUseExtendedPrintDlg(NULL)
{
  NS_INIT_REFCNT();

  mDriverName    = nsnull;
  mDeviceName    = nsnull;
  mDevMode       = NULL;
  mGlobalDevMode = NULL;
  mIsDEVMODEGlobalHandle = PR_FALSE;

#ifdef MOZ_REQUIRE_CURRENT_SDK
  HMODULE lib = GetModuleHandle("comdlg32.dll");
  if ( lib ) {
    mUseExtendedPrintDlg = GetProcAddress(lib, GetPrintDlgExQuoted);
  }
#endif
}


//----------------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecWin, nsIDeviceContextSpec)

nsDeviceContextSpecWin::~nsDeviceContextSpecWin()
{
  SetDeviceName(nsnull);
  SetDriverName(nsnull);
  SetDevMode(NULL);
  SetGlobalDevMode(NULL);

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}


//----------------------------------------------------------------------------------
// Map an incoming size to a Windows Native enum in the DevMode
static void 
MapPaperSizeToNativeEnum(LPDEVMODE aDevMode,
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

  PRBool foundEnum = PR_FALSE;
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
SetupDevModeFromSettings(LPDEVMODE aDevMode, nsIPrintSettings* aPrintSettings)
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

//----------------------------------------------------------------------------------
// Helper Function - Free and reallocate the string
nsresult 
nsDeviceContextSpecWin::SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, 
                                                    LPDEVMODE         aDevMode)
{
  if (aPrintSettings == nsnull) {
    return NS_ERROR_FAILURE;
  }
  BOOL doingNumCopies   = aDevMode->dmFields & DM_COPIES;
  BOOL doingOrientation = aDevMode->dmFields & DM_ORIENTATION;
  BOOL doingPaperSize   = aDevMode->dmFields & DM_PAPERSIZE;
  BOOL doingPaperLength = aDevMode->dmFields & DM_PAPERLENGTH;
  BOOL doingPaperWidth  = aDevMode->dmFields & DM_PAPERWIDTH;

  if (doingOrientation) {
    PRInt32 orientation  = aDevMode->dmOrientation == DMORIENT_PORTRAIT?
      nsIPrintSettings::kPortraitOrientation:nsIPrintSettings::kLandscapeOrientation;
    aPrintSettings->SetOrientation(orientation);
  }

  // Setup Number of Copies
  if (doingNumCopies) {
    aPrintSettings->SetNumCopies(PRInt32(aDevMode->dmCopies));
  }

  if (doingPaperSize) {
    aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeNativeData);
    aPrintSettings->SetPaperData(aDevMode->dmPaperSize);

  } else if (doingPaperLength && doingPaperWidth) {
    PRBool found = PR_FALSE;
    for (PRInt32 i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperWidth(kPaperSizes[i].mWidth);
        aPrintSettings->SetPaperHeight(kPaperSizes[i].mHeight);
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeInches);
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

//----------------------------------------------------------------------------------
NS_IMETHODIMP nsDeviceContextSpecWin::Init(nsIWidget* aWidget, 
                                           nsIPrintSettings* aPrintSettings,
                                           PRBool aQuiet)
{
  mPrintSettings = aPrintSettings;

  gParentWnd = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);

  PRBool doNativeDialog = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    pPrefs->GetBoolPref("print.use_native_print_dialog", &doNativeDialog);
  }

  if (doNativeDialog || aQuiet) {
#ifdef MOZ_REQUIRE_CURRENT_SDK
    if (mUseExtendedPrintDlg) {
      rv = ShowNativePrintDialogEx(aWidget, aQuiet);
    } else {
      rv = ShowNativePrintDialog(aWidget, aQuiet);
    }
#else
      rv = ShowNativePrintDialog(aWidget, aQuiet);
#endif
  } else {
    rv = ShowXPPrintDialog(aQuiet);
  }

  return rv;
}

//----------------------------------------------------------
// Helper Function - Free and reallocate the string
static void CleanAndCopyString(char*& aStr, char* aNewStr)
{
  if (aStr != nsnull) {
    if (aNewStr != nsnull && strlen(aStr) > strlen(aNewStr)) { // reuse it if we can
      PL_strcpy(aStr, aNewStr);
      return;
    } else {
      PR_Free(aStr);
      aStr = nsnull;
    }
  }

  if (nsnull != aNewStr) {
    aStr = (char *)PR_Malloc(PL_strlen(aNewStr) + 1);
    PL_strcpy(aStr, aNewStr);
  }
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDeviceName(char* aDeviceName)
{
  CleanAndCopyString(mDeviceName, aDeviceName);
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDriverName(char* aDriverName)
{
  CleanAndCopyString(mDriverName, aDriverName);
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetGlobalDevMode(HGLOBAL aHGlobal)
{
  if (mGlobalDevMode) {
    ::GlobalFree(mGlobalDevMode);
    mGlobalDevMode = NULL;
  }
  mGlobalDevMode = aHGlobal;
  mIsDEVMODEGlobalHandle = PR_TRUE;
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDevMode(LPDEVMODE aDevMode)
{
  if (mDevMode) free(mDevMode);

  mDevMode = aDevMode;
  mIsDEVMODEGlobalHandle = PR_FALSE;
}

//----------------------------------------------------------------------------------
// Return localized bundle for resource strings
static nsresult
GetLocalizedBundle(const char * aPropFileName, nsIStringBundle** aStrBundle)
{
  NS_ENSURE_ARG_POINTER(aPropFileName);
  NS_ENSURE_ARG_POINTER(aStrBundle);

  nsresult rv;
  nsCOMPtr<nsIStringBundle> bundle;
  

  // Create bundle
  nsCOMPtr<nsIStringBundleService> stringService = 
    do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService) {
    rv = stringService->CreateBundle(aPropFileName, aStrBundle);
  }
  
  return rv;
}

//--------------------------------------------------------
// Return localized string 
static nsresult
GetLocalizedString(nsIStringBundle* aStrBundle, const char* aKey, nsString& oVal)
{
  NS_ENSURE_ARG_POINTER(aStrBundle);
  NS_ENSURE_ARG_POINTER(aKey);

  // Determine default label from string bundle
  nsXPIDLString valUni;
  nsAutoString key; 
  key.AssignWithConversion(aKey);
  nsresult rv = aStrBundle->GetStringFromName(key.get(), getter_Copies(valUni));
  if (NS_SUCCEEDED(rv) && valUni) {
    oVal.Assign(valUni);
  } else {
    oVal.Truncate();
  }
  return rv;
}

//--------------------------------------------------------
// Set a multi-byte string in the control
static void SetTextOnWnd(HWND aControl, const nsString& aStr)
{
  char* pStr = nsDeviceContextWin::GetACPString(aStr);
  if (pStr) {
    ::SetWindowText(aControl, pStr);
    delete [] pStr;
  }
}

//--------------------------------------------------------
// Will get the control and localized string by "key"
static void SetText(HWND             aParent, 
                    UINT             aId, 
                    nsIStringBundle* aStrBundle,
                    const char*      aKey) 
{
  HWND wnd = GetDlgItem (aParent, aId);
  if (!wnd) {
    return;
  }
  nsAutoString str;
  nsresult rv = GetLocalizedString(aStrBundle, aKey, str);
  if (NS_SUCCEEDED(rv)) {
    SetTextOnWnd(wnd, str);
  }
}

//--------------------------------------------------------
static void SetRadio(HWND         aParent, 
                     UINT         aId, 
                     PRBool       aIsSet,
                     PRBool       isEnabled = PR_TRUE) 
{
  HWND wnd = ::GetDlgItem (aParent, aId);
  if (!wnd) {
    return;
  }
  if (!isEnabled) {
    ::EnableWindow(wnd, FALSE);
    return;
  }
  ::EnableWindow(wnd, TRUE);
  ::SendMessage(wnd, BM_SETCHECK, (WPARAM)aIsSet, (LPARAM)0);
}

//--------------------------------------------------------
static void SetRadioOfGroup(HWND aDlg, int aRadId)
{
  int radioIds[] = {rad4, rad5, rad6};
  int numRads = 3;

  for (int i=0;i<numRads;i++) {
    HWND radWnd = ::GetDlgItem(aDlg, radioIds[i]);
    if (radWnd != NULL) {
      ::SendMessage(radWnd, BM_SETCHECK, (WPARAM)(radioIds[i] == aRadId), (LPARAM)0);
    }
  }
}

//--------------------------------------------------------
typedef struct {
  char * mKeyStr;
  long   mKeyId;
} PropKeyInfo;

// These are the control ids used in the dialog and 
// defined by MS-Windows in commdlg.h
static PropKeyInfo gAllPropKeys[] = {
    {"PrintFrames", grp3},
    {"Aslaid", rad4},
    {"selectedframe", rad5},
    {"Eachframe", rad6},
    {NULL, NULL}};

    // This is a list of other controls Ids in the native dialog
    // I am leaving these here for documentation purposes
    //
    //{"Printer", grp4},
    //{"Name", stc6},
    //{"Properties", psh2},
    //{"Status", stc8},
    //{"Type", stc7},
    //{"Where", stc10},
    //{"Comment", stc9},
    //{"Printtofile", chx1},
    //{"Printrange", grp1},
    //{"All", rad1},
    //{"Pages", rad3},
    //{"Selection", rad2},
    //{"from", stc2},
    //{"to", stc3},
    //{"Copies", grp2},
    //{"Numberofcopies", stc5},
    //{"Collate", chx2},
    //{"OK", IDOK},
    //{"Cancel", IDCANCEL},
    //{"stc11", stc11},
    //{"stc12", stc12},
    //{"stc14", stc14},
    //{"stc13", stc13},

//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
// Get the absolute coords of the child windows relative
// to its parent window
static void GetLocalRect(HWND aWnd, RECT& aRect, HWND aParent)
{
  RECT wr;
  ::GetWindowRect(aParent, &wr);

  RECT cr;
  ::GetClientRect(aParent, &cr);

  ::GetWindowRect(aWnd, &aRect);

  int borderH = (wr.bottom-wr.top+1) - (cr.bottom-cr.top+1);
  int borderW = ((wr.right-wr.left+1) - (cr.right-cr.left+1))/2;
  aRect.top    -= wr.top+borderH-borderW;
  aRect.left   -= wr.left+borderW;
  aRect.right  -= wr.left+borderW;
  aRect.bottom -= wr.top+borderH-borderW;
}

//--------------------------------------------------------
// Show or Hide the control
void Show(HWND aWnd, PRBool bState)
{
  if (aWnd) {
    ::ShowWindow(aWnd, bState?SW_SHOW:SW_HIDE);
  }
}

//--------------------------------------------------------
// Create a child window "control"
static HWND CreateControl(LPCTSTR          aType,
                          DWORD            aStyle,
                          HINSTANCE        aHInst, 
                          HWND             aHdlg, 
                          int              aId, 
                          const nsAString& aStr, 
                          const nsRect&    aRect)
{
  char* pStr = nsDeviceContextWin::GetACPString(aStr);
  if (pStr == NULL) return NULL;

  HWND hWnd = ::CreateWindow (aType, pStr,
                              WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | aStyle,
                              aRect.x, aRect.y, aRect.width, aRect.height,
                              (HWND)aHdlg, (HMENU)aId,
                              aHInst, NULL);
  if (hWnd == NULL) return NULL;

  delete [] pStr;

  // get the native font for the dialog and 
  // set it into the new control
  HFONT hFont = (HFONT)::SendMessage(aHdlg, WM_GETFONT, (WPARAM)0, (LPARAM)0);
  if (hFont != NULL) {
    ::SendMessage(hWnd, WM_SETFONT, (WPARAM) hFont, (LPARAM)0);
  }
  return hWnd;
}

//--------------------------------------------------------
// Create a Radio Button
static HWND CreateRadioBtn(HINSTANCE        aHInst, 
                           HWND             aHdlg, 
                           int              aId, 
                           const char*      aStr, 
                           const nsRect&    aRect)
{
  nsString cStr;
  cStr.AssignWithConversion(aStr);
  return CreateControl("BUTTON", BS_RADIOBUTTON, aHInst, aHdlg, aId, cStr, aRect);
}

//--------------------------------------------------------
// Create a Group Box
static HWND CreateGroupBox(HINSTANCE        aHInst, 
                           HWND             aHdlg, 
                           int              aId, 
                           const nsAString& aStr, 
                           const nsRect&    aRect)
{
  return CreateControl("BUTTON", BS_GROUPBOX, aHInst, aHdlg, aId, aStr, aRect);
}

//--------------------------------------------------------
// Localizes and initializes the radio buttons and group
void InitializeExtendedDialog(HWND hdlg, PRInt16 aHowToEnableFrameUI) 
{
  // Localize the new controls in the print dialog
  nsCOMPtr<nsIStringBundle> strBundle;
  if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
    PRInt32 i = 0;
    while (gAllPropKeys[i].mKeyStr != NULL) {
      SetText(hdlg, gAllPropKeys[i].mKeyId, strBundle, gAllPropKeys[i].mKeyStr);
      i++;
    }
  }

  // Set up radio buttons
  if (aHowToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
    SetRadio(hdlg, rad4, PR_FALSE);  
    SetRadio(hdlg, rad5, PR_TRUE); 
    SetRadio(hdlg, rad6, PR_FALSE);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad5;

  } else if (aHowToEnableFrameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
    SetRadio(hdlg, rad4, PR_FALSE);  
    SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad6, PR_TRUE);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad6;


  } else {  // nsIPrintSettings::kFrameEnableNone
    // we are using this function to disabe the group box
    SetRadio(hdlg, grp3, PR_FALSE, PR_FALSE); 
    // now disable radiobuttons
    SetRadio(hdlg, rad4, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad6, PR_FALSE, PR_FALSE); 
  }

}


//--------------------------------------------------------
// Special Hook Procedure for handling the print dialog messages
UINT CALLBACK PrintHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) 
{

  if (uiMsg == WM_COMMAND) {
    UINT id = LOWORD(wParam);
    if (id == rad4 || id == rad5 || id == rad6) {
      gFrameSelectedRadioBtn = id;
      SetRadioOfGroup(hdlg, id);
    }

  } else if (uiMsg == WM_INITDIALOG) {
    PRINTDLG * printDlg = (PRINTDLG *)lParam;
    if (printDlg == NULL) return 0L;

    PRInt16 howToEnableFrameUI = (PRInt16)printDlg->lCustData;

    HINSTANCE hInst = (HINSTANCE)::GetWindowLong(hdlg, GWL_HINSTANCE);
    if (hInst == NULL) return 0L;

    // Start by getting the local rects of several of the controls
    // so we can calculate where the new controls are
    HWND wnd = ::GetDlgItem(hdlg, grp1);
    if (wnd == NULL) return 0L;
    RECT dlgRect;
    GetLocalRect(wnd, dlgRect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad1); // this is the top control "All"
    if (wnd == NULL) return 0L;
    RECT rad1Rect;
    GetLocalRect(wnd, rad1Rect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad2); // this is the bottom control "Selection"
    if (wnd == NULL) return 0L;
    RECT rad2Rect;
    GetLocalRect(wnd, rad2Rect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad3); // this is the middle control "Pages"
    if (wnd == NULL) return 0L;
    RECT rad3Rect;
    GetLocalRect(wnd, rad3Rect, hdlg);

    HWND okWnd = ::GetDlgItem(hdlg, IDOK);
    if (okWnd == NULL) return 0L;
    RECT okRect;
    GetLocalRect(okWnd, okRect, hdlg);

    wnd = ::GetDlgItem(hdlg, grp4); // this is the "Print range" groupbox
    if (wnd == NULL) return 0L;
    RECT prtRect;
    GetLocalRect(wnd, prtRect, hdlg);


    // calculate various different "gaps" for layout purposes

    int rbGap     = rad3Rect.top - rad1Rect.bottom;     // gap between radiobtns
    int grpBotGap = dlgRect.bottom - rad2Rect.bottom;   // gap from bottom rb to bottom of grpbox
    int grpGap    = dlgRect.top - prtRect.bottom ;      // gap between group boxes
    int top       = dlgRect.bottom + grpGap;            
    int radHgt    = rad1Rect.bottom - rad1Rect.top + 1; // top of new group box
    int y         = top+(rad1Rect.top-dlgRect.top);     // starting pos of first radio
    int rbWidth   = dlgRect.right - rad1Rect.left - 5;  // measure from rb left to the edge of the groupbox
                                                        // (5 is arbitrary)
    nsRect rect;

    // Create and position the radio buttons
    //
    // If any one control cannot be created then 
    // hide the others and bail out
    //
    rect.SetRect(rad1Rect.left, y, rbWidth,radHgt);
    HWND rad4Wnd = CreateRadioBtn(hInst, hdlg, rad4, kAsLaidOutOnScreenStr, rect);
    if (rad4Wnd == NULL) return 0L;
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad5Wnd = CreateRadioBtn(hInst, hdlg, rad5, kTheSelectedFrameStr, rect);
    if (rad5Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad6Wnd = CreateRadioBtn(hInst, hdlg, rad6, kEachFrameSeparately, rect);
    if (rad6Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + grpBotGap;

    // Create and position the group box
    rect.SetRect (dlgRect.left, top, dlgRect.right-dlgRect.left+1, y-top+1);
    HWND grpBoxWnd = CreateGroupBox(hInst, hdlg, grp3, NS_LITERAL_STRING("Print Frame"), rect);
    if (grpBoxWnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      Show(rad6Wnd, FALSE); // hide
      return 0L;
    }

    // Here we figure out the old height of the dlg
    // then figure it's gap from the old grpbx to the bottom
    // then size the dlg
    RECT pr, cr; 
    ::GetWindowRect(hdlg, &pr);
    ::GetClientRect(hdlg, &cr);

    int dlgHgt = (cr.bottom - cr.top) + 1;
    int bottomGap = dlgHgt - okRect.bottom;
    pr.bottom += (dlgRect.bottom-dlgRect.top) + grpGap + 1 - (dlgHgt-dlgRect.bottom) + bottomGap;

    ::SetWindowPos(hdlg, NULL, pr.left, pr.top, pr.right-pr.left+1, pr.bottom-pr.top+1, 
                   SWP_NOMOVE|SWP_NOREDRAW|SWP_NOZORDER);

    // figure out the new height of the dialog
    ::GetClientRect(hdlg, &cr);
    dlgHgt = (cr.bottom - cr.top) + 1;
 
    // Reposition the OK and Cancel btns
    int okHgt = okRect.bottom - okRect.top + 1;
    ::SetWindowPos(okWnd, NULL, okRect.left, dlgHgt-bottomGap-okHgt, 0, 0, 
                   SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER);

    HWND cancelWnd = ::GetDlgItem(hdlg, IDCANCEL);
    if (cancelWnd == NULL) return 0L;

    RECT cancelRect;
    GetLocalRect(cancelWnd, cancelRect, hdlg);
    int cancelHgt = cancelRect.bottom - cancelRect.top + 1;
    ::SetWindowPos(cancelWnd, NULL, cancelRect.left, dlgHgt-bottomGap-cancelHgt, 0, 0, 
                   SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER);

    // localize and initialize the groupbox and radiobuttons
    InitializeExtendedDialog(hdlg, howToEnableFrameUI);

    // Looks like we were able to extend the dialog
    gDialogWasExtended = PR_TRUE;
  }
  return 0L;
}

//----------------------------------------------------------------------------------
// Returns a Global Moveable Memory Handle to a DevMode
// from the Printer byt the name of aPrintName
static HGLOBAL CreateGlobalDevModeAndInit(LPTSTR aPrintName, nsIPrintSettings* aPS)
{
  HGLOBAL hGlobalDevMode = NULL;

  nsresult rv = NS_ERROR_FAILURE;
  HANDLE hPrinter = NULL;
  BOOL status = ::OpenPrinter(aPrintName, &hPrinter, NULL);
  if (status) {

    LPDEVMODE   pNewDevMode;
    DWORD       dwNeeded, dwRet;

    // Allocate a buffer of the correct size.
    dwNeeded = ::DocumentProperties(gParentWnd, hPrinter, aPrintName, NULL, NULL, 0);

    pNewDevMode = (LPDEVMODE)malloc(dwNeeded);
    if (!pNewDevMode) return NULL;

    hGlobalDevMode = (HGLOBAL)::GlobalAlloc(GHND, dwNeeded);
    if (!hGlobalDevMode) {
      free(pNewDevMode);
    }

    dwRet = ::DocumentProperties(gParentWnd, hPrinter, aPrintName, pNewDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet != IDOK) {
      free(pNewDevMode);
      ::GlobalFree(hGlobalDevMode);
      ::ClosePrinter(hPrinter);
      return NULL;
    }

    // Lock memory and copy contents from DEVMODE (current printer)
    // to Global Memory DEVMODE
    LPDEVMODE devMode = (DEVMODE *)::GlobalLock(hGlobalDevMode);
    if (devMode) {
      memcpy(devMode, pNewDevMode, dwNeeded);
      // Initialize values from the PrintSettings
      SetupDevModeFromSettings(devMode, aPS);
      ::GlobalUnlock(hGlobalDevMode);
    } else {
      ::GlobalFree(hGlobalDevMode);
      hGlobalDevMode = NULL;
    }

    free(pNewDevMode);

    ::ClosePrinter(hPrinter);

  } else {
    return NULL;
  }

  return hGlobalDevMode;
}

//------------------------------------------------------------------
// helper
static PRUnichar * GetDefaultPrinterNameFromGlobalPrinters()
{
  PRUnichar * printerName;
  LPTSTR lpPrtName;
  GlobalPrinters::GetInstance()->GetDefaultPrinterName(lpPrtName);
  nsString str;
#ifdef UNICODE
  str.AppendWithConversion((PRUnichar *)lpPrtName);
#else 
  str.AssignWithConversion((char*)lpPrtName);
#endif
  printerName = ToNewUnicode(str);
  free(lpPrtName);
  return printerName;
}


//------------------------------------------------------------------
void 
nsDeviceContextSpecWin::GetDevMode(LPDEVMODE &aDevMode)
{
  if (mIsDEVMODEGlobalHandle) {
    if (mGlobalDevMode) {
      aDevMode = (DEVMODE *)::GlobalLock(mGlobalDevMode);
    } else {
      aDevMode = NULL;
    }
  } else {
    aDevMode = mDevMode;
  }
}

//------------------------------------------------------------------
// Displays the native Print Dialog
nsresult 
nsDeviceContextSpecWin::ShowNativePrintDialog(nsIWidget *aWidget, PRBool aQuiet)
{
  NS_ENSURE_ARG_POINTER(aWidget);
  nsresult  rv = NS_ERROR_FAILURE;
  gDialogWasExtended  = PR_FALSE;

  HGLOBAL hGlobalDevMode = NULL;
  HGLOBAL hDevNames      = NULL;

  // Get the Print Name to be used
  PRUnichar * printerName;
  mPrintSettings->GetPrinterName(&printerName);

  // If there is no name then use the default printer
  if (!printerName || (printerName && !*printerName)) {
    printerName = GetDefaultPrinterNameFromGlobalPrinters();
  }

  NS_ASSERTION(printerName, "We have to have a printer name");
  if (!printerName) return NS_ERROR_FAILURE;

  if (!aQuiet) {
    // Now create a DEVNAMES struct so the the dialog is initialized correctly.
    PRUint32 len = nsCRT::strlen(printerName);
    PRUint32 len2 = len+sizeof(DEVNAMES);
    hDevNames = (HGLOBAL)::GlobalAlloc(GHND, len+sizeof(DEVNAMES)+1);
    DEVNAMES* pDevNames = (DEVNAMES*)::GlobalLock(hDevNames);
    pDevNames->wDriverOffset = sizeof(DEVNAMES);
    pDevNames->wDeviceOffset = sizeof(DEVNAMES);
    pDevNames->wOutputOffset = sizeof(DEVNAMES)+len+1;
    pDevNames->wDefault      = 0;
    char* device = &(((char*)pDevNames)[pDevNames->wDeviceOffset]);
    strcpy(device, (char*)NS_ConvertUCS2toUTF8(printerName).get());
    ::GlobalUnlock(hDevNames);

    // Create a Moveable Memory Object that holds a new DevMode
    // from the Printer Name
    // The PRINTDLG.hDevMode requires that it be a moveable memory object
    // NOTE: We only need to free hGlobalDevMode when the dialog is cancelled
    // When the user prints, it comes back in the printdlg struct and 
    // is used and cleaned up later
#ifdef UNICODE
    hGlobalDevMode = CreateGlobalDevModeAndInit(printerName, mPrintSettings);
#else 
    hGlobalDevMode = CreateGlobalDevModeAndInit((char*)NS_ConvertUCS2toUTF8(printerName).get(), mPrintSettings);
#endif
  } else {
    // For aQuiet create a LPDEVMODE from the printer name
    // set it into the mDeviceMode
    // then transfer the appropriate PrintSettnigs to it.
    return GetDataFromPrinter(printerName, mPrintSettings);
  }

  // Prepare to Display the Print Dialog
  PRINTDLG  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLG));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner   = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  prntdlg.hDevMode    = hGlobalDevMode;
  prntdlg.hDevNames   = hDevNames;
  prntdlg.hDC         = NULL;
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNIC | PD_HIDEPRINTTOFILE | PD_USEDEVMODECOPIESANDCOLLATE;

  // if there is a current selection then enable the "Selection" radio button
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  if (mPrintSettings != nsnull) {
    PRBool isOn;
    mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    if (!isOn) {
      prntdlg.Flags |= PD_NOSELECTION;
    }
    mPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  // Determine whether we have a completely native dialog
  // or whether we cshould extend it
  // true  - do only the native
  // false - extend the dialog
  PRPackedBool doExtend = PR_FALSE;
  nsCOMPtr<nsIStringBundle> strBundle;
  if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
    nsAutoString doExtendStr;
    if (NS_SUCCEEDED(GetLocalizedString(strBundle, "extend", doExtendStr))) {
      doExtend = doExtendStr.Equals(NS_LITERAL_STRING("true"));
    }
  }

  prntdlg.nFromPage           = 0xFFFF;
  prntdlg.nToPage             = 0xFFFF;
  prntdlg.nMinPage            = 1;
  prntdlg.nMaxPage            = 0xFFFF;
  prntdlg.nCopies             = 1;
  prntdlg.lpfnSetupHook       = NULL;
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate      = NULL;
  prntdlg.hSetupTemplate      = NULL;

  prntdlg.hInstance           = NULL;
  prntdlg.lpPrintTemplateName = NULL;

  if (!doExtend) {
    prntdlg.lCustData         = NULL;
    prntdlg.lpfnPrintHook     = NULL;
  } else {
    // Set up print dialog "hook" procedure for extending the dialog
    prntdlg.lCustData         = (DWORD)howToEnableFrameUI;
    prntdlg.lpfnPrintHook     = (LPPRINTHOOKPROC)PrintHookProc;
    prntdlg.Flags            |= PD_ENABLEPRINTHOOK;
  }

  BOOL result = ::PrintDlg(&prntdlg);

  if (TRUE == result) {
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);
    if ( NULL != devnames ) {

      char* device = &(((char *)devnames)[devnames->wDeviceOffset]);
      char* driver = &(((char *)devnames)[devnames->wDriverOffset]);

      // Setup local Data members
      SetDeviceName(device);
      SetDriverName(driver);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
      printf("printer: driver %s, device %s  flags: %d\n", driver, device, prntdlg.Flags);
#endif
      // fill the print options with the info from the dialog
      if (mPrintSettings != nsnull) {
        nsString printerName;
        printerName.AssignWithConversion(device);

        mPrintSettings->SetPrinterName(printerName.get());

        if (prntdlg.Flags & PD_SELECTION) {
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);

        } else if (prntdlg.Flags & PD_PAGENUMS) {
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
          mPrintSettings->SetStartPageRange(prntdlg.nFromPage);
          mPrintSettings->SetEndPageRange( prntdlg.nToPage);

        } else { // (prntdlg.Flags & PD_ALLPAGES)
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
        }

        if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
          // make sure the dialog got extended
          if (gDialogWasExtended) {
            // check to see about the frame radio buttons
            switch (gFrameSelectedRadioBtn) {
              case rad4: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
                break;
              case rad5: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
                break;
              case rad6: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
                break;
            } // switch
          } else {
            // if it didn't get extended then have it default to printing
            // each frame separately
            mPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
          }
        } else {
          mPrintSettings->SetPrintFrameType(nsIPrintSettings::kNoFrames);
        }
      }
      ::GlobalUnlock(prntdlg.hDevNames);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    PRBool  printSelection = prntdlg.Flags & PD_SELECTION;
    PRBool  printAllPages  = prntdlg.Flags & PD_ALLPAGES;
    PRBool  printNumPages  = prntdlg.Flags & PD_PAGENUMS;
    PRInt32 fromPageNum    = 0;
    PRInt32 toPageNum      = 0;

    if (printNumPages) {
      fromPageNum = prntdlg.nFromPage;
      toPageNum   = prntdlg.nToPage;
    } 
    if (printSelection) {
      printf("Printing the selection\n");

    } else if (printAllPages) {
      printf("Printing all the pages\n");

    } else {
      printf("Printing from page no. %d to %d\n", fromPageNum, toPageNum);
    }
#endif
    
      SetGlobalDevMode(prntdlg.hDevMode);

      // Set into DevMode Paper Size and Orientation here
      // remove comment if you want to override the values from
      // the native setup with those specified in the Page Setup
      // mainly Paper Size, Orientation
      if (aQuiet) {
        SetupPaperInfoFromSettings();
      }

    }
  } else {
    ::GlobalFree(hGlobalDevMode);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}


#ifdef MOZ_REQUIRE_CURRENT_SDK
//------------------------------------------------------------------
// Callback for Property Sheet
BOOL APIENTRY PropSheetCallBack(HWND hdlg, UINT uiMsg, UINT wParam, LONG lParam)
{
  if (uiMsg == WM_COMMAND) {
    UINT id = LOWORD(wParam);
    if (id == rad4 || id == rad5 || id == rad6) {
      gFrameSelectedRadioBtn = id;
      SetRadioOfGroup(hdlg, id);
    }

  } else if (uiMsg == WM_INITDIALOG) {
    // Create the groupbox and Radiobuttons on the "Options" Property Sheet

    // We temporarily borrowed the global value for initialization
    // now clear it before the dialog appears
    PRInt16 howToEnableFrameUI = gFrameSelectedRadioBtn;
    gFrameSelectedRadioBtn     = 0;

    HINSTANCE hInst = (HINSTANCE)::GetWindowLong(hdlg, GWL_HINSTANCE);
    if (hInst == NULL) return 0L;

    // Get default font for the dialog & then its font metrics
    // we need the text height to determine the height of the radio buttons
    TEXTMETRIC metrics;
    HFONT hFont = (HFONT)::SendMessage(hdlg, WM_GETFONT, (WPARAM)0, (LPARAM)0);
    HDC localDC = ::GetDC(hdlg);
    ::SelectObject(localDC, (HGDIOBJ)hFont);
    ::GetTextMetrics(localDC, &metrics);
    ::ReleaseDC(hdlg, localDC);

    // calculate various different "gaps" for layout purposes
     RECT dlgr; 
    ::GetWindowRect(hdlg, &dlgr);

    int horzGap    = 5;                                 // generic horz gap
    int vertGap    = 5;                                 // generic vert gap
    int rbGap      = metrics.tmHeight / 2;               // gap between radiobtns
    int top        = vertGap*2;                            // start at the top
    int radHgt     = metrics.tmHeight;                   // top of new group box
    int y          = top;                                // starting pos of first radio
    int x          = horzGap*2;
    int rbWidth    = dlgr.right - dlgr.left - (5*horzGap);  
    int grpWidth   = dlgr.right - dlgr.left - (2*horzGap);  

    nsRect rect;

    // Create and position the radio buttons
    //
    // If any one control cannot be created then 
    // hide the others and bail out
    //
    x += horzGap*2;
    y += vertGap + metrics.tmHeight;
    rect.SetRect(x, y, rbWidth,radHgt);
    HWND rad4Wnd = CreateRadioBtn(hInst, hdlg, rad4, kAsLaidOutOnScreenStr, rect);
    if (rad4Wnd == NULL) return 0L;
    y += radHgt + rbGap;

    rect.SetRect(x, y, rbWidth, radHgt);
    HWND rad5Wnd = CreateRadioBtn(hInst, hdlg, rad5, kTheSelectedFrameStr, rect);
    if (rad5Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + rbGap;

    rect.SetRect(x, y, rbWidth, radHgt);
    HWND rad6Wnd = CreateRadioBtn(hInst, hdlg, rad6, kEachFrameSeparately, rect);
    if (rad6Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + (vertGap*2);

    x -= horzGap*2;
    // Create and position the group box
    rect.SetRect (x, top, grpWidth, y-top+1);
    HWND grpBoxWnd = CreateGroupBox(hInst, hdlg, grp3, NS_LITERAL_STRING("Print Frame"), rect);
    if (grpBoxWnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      Show(rad6Wnd, FALSE); // hide
      return 0L;
    }

    // localize and initialize the groupbox and radiobuttons
    InitializeExtendedDialog(hdlg, howToEnableFrameUI);

    // Looks like we were able to extend the dialog
    gDialogWasExtended = PR_TRUE;
  }
  return 0L;
}

//------------------------------------------------------------------
// Creates the "Options" Property Sheet
static HPROPSHEETPAGE ExtendPrintDialog(HWND aHWnd, char* aTitle)
{
  // The resource "OPTPROPSHEET" comes out of the widget/build/widget.rc file
  HINSTANCE hInst = (HINSTANCE)::GetWindowLong(aHWnd, GWL_HINSTANCE);
  PROPSHEETPAGE psp;
  memset(&psp, 0, sizeof(PROPSHEETPAGE));
  psp.dwSize      = sizeof(PROPSHEETPAGE);
  psp.dwFlags     = PSP_USETITLE | PSP_PREMATURE;
  psp.hInstance   = hInst;
  psp.pszTemplate = "OPTPROPSHEET";
  psp.pfnDlgProc  = PropSheetCallBack;
  psp.pszTitle    = aTitle?aTitle:"Options";

  HPROPSHEETPAGE newPropSheet = ::CreatePropertySheetPage(&psp);
  return newPropSheet;

}

//------------------------------------------------------------------
// Displays the native Print Dialog
nsresult 
nsDeviceContextSpecWin::ShowNativePrintDialogEx(nsIWidget *aWidget, PRBool aQuiet)
{
  NS_ENSURE_ARG_POINTER(aWidget);

  nsresult  rv = NS_ERROR_FAILURE;
  gDialogWasExtended  = PR_FALSE;

  // Create a Moveable Memory Object that holds a new DevMode
  // from the Printer Name
  // The PRINTDLG.hDevMode requires that it be a moveable memory object
  // NOTE: We only need to free hGlobalDevMode when the dialog is cancelled
  // When the user prints, it comes back in the printdlg struct and 
  // is used and cleaned up later
  PRUnichar * printerName;
  mPrintSettings->GetPrinterName(&printerName);
  HGLOBAL hGlobalDevMode = NULL;
  if (printerName) {
#ifdef UNICODE
    hGlobalDevMode = CreateGlobalDevModeAndInit(printerName, mPrintSettings);
#else 
    hGlobalDevMode = CreateGlobalDevModeAndInit((char*)NS_ConvertUCS2toUTF8(printerName).get(), mPrintSettings);
#endif
  }

  // Prepare to Display the Print Dialog
  PRINTDLGEX  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLGEX));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner   = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  prntdlg.hDevMode    = hGlobalDevMode;
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNDC | PD_HIDEPRINTTOFILE | PD_USEDEVMODECOPIESANDCOLLATE |
                        PD_NOCURRENTPAGE;
  prntdlg.nStartPage  = START_PAGE_GENERAL;

  // if there is a current selection then enable the "Selection" radio button
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  if (mPrintSettings != nsnull) {
    PRBool isOn;
    mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    if (!isOn) {
      prntdlg.Flags |= PD_NOSELECTION;
    }
    mPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  // Determine whether we have a completely native dialog
  // or whether we cshould extend it
  // true  - do only the native
  // false - extend the dialog
  PRPackedBool doExtend = PR_FALSE;
  nsCOMPtr<nsIStringBundle> strBundle;
  if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
    nsAutoString doExtendStr;
    if (NS_SUCCEEDED(GetLocalizedString(strBundle, "extend", doExtendStr))) {
      doExtend = doExtendStr.EqualsIgnoreCase("true");
    }
  }

  // At the moment we can only support one page range
  // from all the documentation I can find, it appears that this 
  // will get cleanup automatically when the struct goes away
  const int kNumPageRanges     = 1;
  LPPRINTPAGERANGE pPageRanges = (LPPRINTPAGERANGE) GlobalAlloc(GPTR, kNumPageRanges * sizeof(PRINTPAGERANGE));
  if (!pPageRanges)
      return E_OUTOFMEMORY;

  prntdlg.nPageRanges    = 0;
  prntdlg.nMaxPageRanges = kNumPageRanges;
  prntdlg.lpPageRanges   = pPageRanges;
  prntdlg.nMinPage       = 1;
  prntdlg.nMaxPage       = 0xFFFF;
  prntdlg.nCopies        = 1;

  if (doExtend && !aQuiet) {
    // lLcalize the Property Sheet (Tab) title
    char* pTitle = NULL;
    nsString optionsStr;
    if (NS_SUCCEEDED(GetLocalizedString(strBundle, "options", optionsStr))) {
      pTitle = nsDeviceContextWin::GetACPString(optionsStr);
    }

    // Temporarily borrow this variable for setting up the radiobuttons
    // if we don't use this, we will need to define a new global var
    gFrameSelectedRadioBtn = howToEnableFrameUI;
    HPROPSHEETPAGE psp[1];
    psp[0] = ExtendPrintDialog((HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW), pTitle);
    prntdlg.nPropertyPages      = 1;
    prntdlg.lphPropertyPages    = psp;
  }

  if (PR_TRUE == aQuiet){
    prntdlg.Flags = PD_ALLPAGES | PD_RETURNDEFAULT | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE;
  }

  HRESULT result = ::PrintDlgEx(&prntdlg);

  if (S_OK == result && 
      (prntdlg.dwResultAction == PD_RESULT_PRINT || 
       (prntdlg.dwResultAction == PD_RESULT_CANCEL && aQuiet))) {
    if (mPrintSettings && prntdlg.hDevMode != NULL) {
      // when it is quite use the printsettings passed 
      if (!aQuiet) {
        LPDEVMODE devMode = (LPDEVMODE)::GlobalLock(prntdlg.hDevMode);
        SetPrintSettingsFromDevMode(mPrintSettings, devMode);
        ::GlobalUnlock(prntdlg.hDevMode);
      }
    }
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);
    if ( NULL != devnames ) {

      char* device = &(((char *)devnames)[devnames->wDeviceOffset]);
      char* driver = &(((char *)devnames)[devnames->wDriverOffset]);

      // Setup local Data members
      SetDeviceName(device);
      SetDriverName(driver);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
      printf("printer: driver %s, device %s  flags: %d\n", driver, device, prntdlg.Flags);
#endif
      ::GlobalUnlock(prntdlg.hDevNames);

      // fill the print options with the info from the dialog
      if (mPrintSettings != nsnull) {

        if (prntdlg.Flags & PD_SELECTION) {
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);

        } else if (prntdlg.Flags & PD_PAGENUMS) {
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
          mPrintSettings->SetStartPageRange(pPageRanges->nFromPage);
          mPrintSettings->SetEndPageRange(pPageRanges->nToPage);

        } else { // (prntdlg.Flags & PD_ALLPAGES)
          mPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
        }

        if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
          // make sure the dialog got extended
          if (gDialogWasExtended) {
            // check to see about the frame radio buttons
            switch (gFrameSelectedRadioBtn) {
              case rad4: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
                break;
              case rad5: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
                break;
              case rad6: 
                mPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
                break;
            } // switch
          } else {
            // if it didn't get extended then have it default to printing
            // each frame separately
            mPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
          }
        } else {
          mPrintSettings->SetPrintFrameType(nsIPrintSettings::kNoFrames);
        }
      }


#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    PRBool  printSelection = prntdlg.Flags & PD_SELECTION;
    PRBool  printAllPages  = prntdlg.Flags & PD_ALLPAGES;
    PRBool  printNumPages  = prntdlg.Flags & PD_PAGENUMS;
    PRInt32 fromPageNum    = 0;
    PRInt32 toPageNum      = 0;

    if (printNumPages) {
      fromPageNum = pPageRanges->nFromPage;
      toPageNum   = pPageRanges->nToPage;
    } 
    if (printSelection) {
      printf("Printing the selection\n");

    } else if (printAllPages) {
      printf("Printing all the pages\n");

    } else {
      printf("Printing from page no. %d to %d\n", fromPageNum, toPageNum);
    }
#endif
    
      SetGlobalDevMode(prntdlg.hDevMode);

      // Set into DevMode Paper Size and Orientation here
      // remove comment if you want to override the values from
      // the native setup with those specified in the Page Setup
      // mainly Paper Size, Orientation
      if (aQuiet) {
       SetupPaperInfoFromSettings();
      }

    }
  } else {
    if (hGlobalDevMode) ::GlobalFree(hGlobalDevMode);
    return NS_ERROR_ABORT;
  }

  ::GlobalFree(pPageRanges);

  return NS_OK;
}
#endif // MOZ_REQUIRE_CURRENT_SDK

//----------------------------------------------------------------------------------
// Setup the object's data member with the selected printer's data
nsresult
nsDeviceContextSpecWin::GetDataFromPrinter(const PRUnichar * aName, nsIPrintSettings* aPS)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!GlobalPrinters::GetInstance()->PrintersAreAllocated()) {
    return rv;
  }

  HANDLE hPrinter = NULL;
  BOOL status = ::OpenPrinter((char*)NS_ConvertUCS2toUTF8(aName).get(), &hPrinter, NULL);
  if (status) {

    LPDEVMODE   pDevMode;
    DWORD       dwNeeded, dwRet;

    // Allocate a buffer of the correct size.
    dwNeeded = DocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aName).get(), 
                                  NULL, NULL, 0);

    pDevMode = (LPDEVMODE)malloc(dwNeeded);

    // Get the default DevMode for the printer and modify it for our needs.
    dwRet = DocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aName).get(),
                               pDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet != IDOK) {
       free(pDevMode);
       ::ClosePrinter(hPrinter);
       return rv;
    }

    if (aPS) {
      SetupDevModeFromSettings(pDevMode, aPS);
    }

    SetDevMode(pDevMode);

    SetDeviceName((char*)NS_ConvertUCS2toUTF8(aName).get());
  
    // The driver should be NULL for Win95/Win98
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    if (VER_PLATFORM_WIN32_NT == os.dwPlatformId) {
      SetDriverName("WINSPOOL");
    } else {
      SetDriverName(NULL);
    }
    ::ClosePrinter(hPrinter);
    rv = NS_OK;
  } else {
    rv = NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
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
  LPDEVMODE devMode;

  GetDevMode(devMode);
  NS_ASSERTION(devMode, "DevMode can't be NULL here");
  if (devMode) {
    SetupDevModeFromSettings(devMode, mPrintSettings);
  }
  UnlockDevMode();
}

//----------------------------------------------------------------------------------
nsresult 
nsDeviceContextSpecWin::ShowXPPrintDialog(PRBool aQuiet)
{
  nsresult rv = NS_ERROR_FAILURE;

  NS_ASSERTION(mPrintSettings, "Can't have a null PrintSettings!");

  // if there is a current selection then enable the "Selection" radio button
  PRBool isOn;
  mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
  nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
  }

  PRBool canPrint = PR_FALSE;
  if (!aQuiet ) {
    rv = NS_ERROR_FAILURE;

    // create a nsISupportsArray of the parameters 
    // being passed to the window
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array) return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> psSupports(do_QueryInterface(mPrintSettings));
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
      if (buttonPressed == 1) {
        canPrint = PR_TRUE;
      } else {
        rv = NS_ERROR_ABORT;
      }
    }
  } else {
    canPrint = PR_TRUE;
  }

  if (canPrint) {
    if (mPrintSettings != nsnull) {
      PRUnichar *printerName = nsnull;
      mPrintSettings->GetPrinterName(&printerName);

      if (printerName != nsnull) {
        // Gets DEVMODE, Device and Driver Names
        rv = GetDataFromPrinter(printerName, mPrintSettings);

        nsMemory::Free(printerName);
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
    return NS_OK;
  }

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return rv;
}

//***********************************************************
//  Printer Enumerator
//***********************************************************
nsPrinterEnumeratorWin::nsPrinterEnumeratorWin()
{
  NS_INIT_REFCNT();
}

nsPrinterEnumeratorWin::~nsPrinterEnumeratorWin()
{
  // Do not free printers here
  // GlobalPrinters::GetInstance()->FreeGlobalPrinters();
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorWin, nsIPrinterEnumerator)


static void CleanupArray(PRUnichar**& aArray, PRInt32& aCount)
{
  for (PRInt32 i = aCount - 1; i >= 0; i--) {
    nsMemory::Free(aArray[i]);
  }
  nsMemory::Free(aArray);
  aArray = NULL;
  aCount = 0;
}

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

  nsCOMPtr<nsDeviceContextSpecWin> devSpecWin = new nsDeviceContextSpecWin();
  if (!devSpecWin) return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(GlobalPrinters::GetInstance()->EnumeratePrinterList())) {
    return NS_ERROR_FAILURE;
  }

  devSpecWin->GetDataFromPrinter(aPrinterName);

  LPDEVMODE devmode;
  devSpecWin->GetDevMode(devmode);
  NS_ASSERTION(devmode, "DevMode can't be NULL here");
  if (devmode) {
    nsDeviceContextSpecWin::SetPrintSettingsFromDevMode(aPrintSettings, devmode);
  }
  devSpecWin->UnlockDevMode();

  // Free them, we won't need them for a while
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  return NS_OK;
}


//----------------------------------------------------------------------------------
// Enumerate all the Printers from the global array and pass their
// names back (usually to script)
NS_IMETHODIMP 
nsPrinterEnumeratorWin::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  if (NS_FAILED(GlobalPrinters::GetInstance()->EnumeratePrinterList())) {
    return NS_ERROR_FAILURE;
  }

  if (aCount) 
    *aCount = 0;
  else 
    return NS_ERROR_NULL_POINTER;
  
  if (aResult) 
    *aResult = nsnull;
  else 
    return NS_ERROR_NULL_POINTER;
  
  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  PRInt32 numItems    = numPrinters;

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(numItems * sizeof(PRUnichar*));
  if (!array) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRInt32 count      = 0;
  PRInt32 printerInx = 0;
  while( count < numItems ) {
    LPTSTR name = GlobalPrinters::GetInstance()->GetItemFromList(printerInx);
    nsString newName; 
    newName.AssignWithConversion(name);
    PRUnichar *str = ToNewUnicode(newName);
    if (!str) {
      CleanupArray(array, count);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[count++] = str;
  }
  *aCount  = count;
  *aResult = array;

  return NS_OK;

}

//----------------------------------------------------------------------------------
// Display the AdvancedDocumentProperties for the selected Printer
NS_IMETHODIMP nsPrinterEnumeratorWin::DisplayPropertiesDlg(const PRUnichar *aPrinterName, nsIPrintSettings* aPrintSettings)
{
  nsresult rv = NS_ERROR_FAILURE;
  HANDLE hPrinter = NULL;
  BOOL status = ::OpenPrinter((char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), &hPrinter, NULL);
  if (status) {

    LPDEVMODE   pDevMode;
    LPDEVMODE   pNewDevMode;
    DWORD       dwNeeded, dwRet;

    // Allocate a buffer of the correct size.
    dwNeeded = ::DocumentProperties(gParentWnd, hPrinter,
                                   (char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), NULL, NULL, 0);

    pNewDevMode = (LPDEVMODE)malloc(dwNeeded);

    dwRet = ::DocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), 
                               pNewDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet != IDOK) {
       free(pNewDevMode);
       ::ClosePrinter(hPrinter);
       return NS_ERROR_FAILURE;
    }

    pDevMode = (LPDEVMODE)malloc(dwNeeded);
    dwRet = ::DocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), 
                               pDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet != IDOK) {
       free(pDevMode);
       free(pNewDevMode);
       ::ClosePrinter(hPrinter);
       return NS_ERROR_FAILURE;
    }


    if (pDevMode && pNewDevMode) {
      SetupDevModeFromSettings(pDevMode, aPrintSettings);

      // Display the Dialog and get the new DevMode
#if 0 // need more to do more work to see why AdvancedDocumentProperties fails 
      // when cancel is pressed
      LONG stat = ::AdvancedDocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), pNewDevMode, pDevMode);
#else
      LONG stat = ::DocumentProperties(gParentWnd, hPrinter, (char*)NS_ConvertUCS2toUTF8(aPrinterName).get(), pDevMode, NULL, DM_IN_PROMPT|DM_OUT_BUFFER);
#endif
      if (stat == IDOK) {
        // Now set the print options from the native Page Setup
        nsDeviceContextSpecWin::SetPrintSettingsFromDevMode(aPrintSettings, pDevMode);
      }
      free(pDevMode);
      free(pNewDevMode);
      rv = NS_OK;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }

    ::ClosePrinter(hPrinter);

  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
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
  mPrinters = new nsVoidArray();
  NS_ASSERTION(mPrinters, "Printers Array is NULL!");
}

//----------------------------------------------------------------------------------
void 
GlobalPrinters::FreeGlobalPrinters()
{
  if (mPrinters != nsnull) {
    for (int i=0;i<mPrinters->Count();i++) {
      free((LPTSTR)mPrinters->ElementAt(i));
    }
    delete mPrinters;
    mPrinters = nsnull;
  }
}

//----------------------------------------------------------------------------------
nsresult 
GlobalPrinters::EnumerateNativePrinters()
{
  nsresult rv = NS_ERROR_FAILURE;

  TCHAR szDefaultPrinterName[1024];    
  DWORD status = GetProfileString("devices", 0, ",", szDefaultPrinterName, sizeof(szDefaultPrinterName)/sizeof(TCHAR));
  if (status > 0) {
    DWORD count = 0;
    LPTSTR sPtr   = (LPTSTR)szDefaultPrinterName;
    LPTSTR ePtr   = (LPTSTR)(szDefaultPrinterName+(status*sizeof(TCHAR)));
    LPTSTR prvPtr = sPtr;
    while (sPtr < ePtr) {
      if (*sPtr == NULL) {
        LPTSTR name = _tcsdup(prvPtr);
        mPrinters->AppendElement(name);
#if defined(DEBUG_rods) || defined(DEBUG_dcone)
        printf("Printer Name:    %s\n" , prvPtr);
#endif
        prvPtr = sPtr+1;
        count++;
      }
      sPtr++;
    }
    rv = NS_OK;
  }

  return rv;
}

//------------------------------------------------------------------
// Uses the GetProfileString to get the default printer from the registry
void 
GlobalPrinters::GetDefaultPrinterName(LPTSTR& aDefaultPrinterName)
{
  aDefaultPrinterName = nsnull;
  TCHAR szDefaultPrinterName[1024];    
  DWORD status = GetProfileString("windows", "device", 0, szDefaultPrinterName, sizeof(szDefaultPrinterName)/sizeof(TCHAR));
  if (status > 0) {
    TCHAR comma = (TCHAR)',';
    LPTSTR sPtr = (LPTSTR)szDefaultPrinterName;
    while (*sPtr != comma && *sPtr != NULL) 
      sPtr++;
    if (*sPtr == comma) {
      *sPtr = NULL;
    }
    aDefaultPrinterName = _tcsdup(szDefaultPrinterName);
  } else {
    aDefaultPrinterName = _tcsdup("");
  }

#ifdef DEBUG_rods
  printf("DEFAULT PRINTER [%s]\n", aDefaultPrinterName);
#endif
}

//----------------------------------------------------------------------------------
// This goes and gets the list of of avilable printers and puts
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
  LPTSTR defPrinterName;
  GetDefaultPrinterName(defPrinterName);

  // put the default printer at the beginning of list
  if (defPrinterName != nsnull) {
    for (PRInt32 i=0;i<mPrinters->Count();i++) {
      LPTSTR name = (LPTSTR)mPrinters->ElementAt(i);
      if (!_tcscmp(name, defPrinterName)) {
        if (i > 0) {
          LPTSTR ptr = (LPTSTR)mPrinters->ElementAt(0);
          mPrinters->ReplaceElementAt((void*)name, 0);
          mPrinters->ReplaceElementAt((void*)ptr, i);
        }
        break;
      }
    }
    free(defPrinterName);
  }

  // make sure we at least tried to get the printers
  if (!PrintersAreAllocated()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

