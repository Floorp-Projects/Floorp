/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -------------------------------------------------------------------
To Build This:

  You need to add this to the the makefile.win in mozilla/dom/base:

        .\$(OBJDIR)\nsFlyOwnPrintDialog.obj	\


  And this to the makefile.win in mozilla/content/build:

WIN_LIBS=                                       \
        winspool.lib                           \
        comctl32.lib                           \
        comdlg32.lib

---------------------------------------------------------------------- */

#include "plstr.h"
#include <windows.h>
#include <tchar.h>

#include <unknwn.h>
#include <commdlg.h>

#include "mozilla/BackgroundHangMonitor.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsWin.h"
#include "nsIPrinterList.h"

#include "nsRect.h"

#include "nsCRT.h"
#include "prenv.h" /* for PR_GetEnv */

#include <windows.h>
#include <winspool.h>

// For Localization

// For NS_CopyUnicodeToNative
#include "nsNativeCharsetUtils.h"

// This is for extending the dialog
#include <dlgs.h>

#include "nsWindowsHelpers.h"
#include "WinUtils.h"

//-----------------------------------------------
// Global Data
//-----------------------------------------------

static HWND gParentWnd = nullptr;

//----------------------------------------------------------------------------------
// Returns a Global Moveable Memory Handle to a DevMode
// from the Printer by the name of aPrintName
//
// NOTE:
//   This function assumes that aPrintName has already been converted from
//   unicode
//
static nsReturnRef<nsHGLOBAL> CreateGlobalDevModeAndInit(
    const nsString& aPrintName, nsIPrintSettings* aPS) {
  nsHPRINTER hPrinter = nullptr;
  // const cast kludge for silly Win32 api's
  LPWSTR printName =
      const_cast<wchar_t*>(static_cast<const wchar_t*>(aPrintName.get()));
  BOOL status = ::OpenPrinterW(printName, &hPrinter, nullptr);
  if (!status) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Make sure hPrinter is closed on all paths
  nsAutoPrinter autoPrinter(hPrinter);

  // Get the buffer size
  LONG needed = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, nullptr,
                                      nullptr, 0);
  if (needed < 0) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Allocate a buffer of the correct size.
  nsAutoDevMode newDevMode(
      (LPDEVMODEW)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, needed));
  if (!newDevMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  nsHGLOBAL hDevMode = ::GlobalAlloc(GHND, needed);
  nsAutoGlobalMem globalDevMode(hDevMode);
  if (!hDevMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  LONG ret = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, newDevMode,
                                   nullptr, DM_OUT_BUFFER);
  if (ret != IDOK) {
    return nsReturnRef<nsHGLOBAL>();
  }

  // Lock memory and copy contents from DEVMODE (current printer)
  // to Global Memory DEVMODE
  LPDEVMODEW devMode = (DEVMODEW*)::GlobalLock(hDevMode);
  if (!devMode) {
    return nsReturnRef<nsHGLOBAL>();
  }

  memcpy(devMode, newDevMode.get(), needed);
  // Initialize values from the PrintSettings
  nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(aPS);
  MOZ_ASSERT(psWin);
  psWin->CopyToNative(devMode);

  // Sets back the changes we made to the DevMode into the Printer Driver
  ret = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, devMode, devMode,
                              DM_IN_BUFFER | DM_OUT_BUFFER);
  if (ret != IDOK) {
    ::GlobalUnlock(hDevMode);
    return nsReturnRef<nsHGLOBAL>();
  }

  ::GlobalUnlock(hDevMode);

  return globalDevMode.out();
}

//------------------------------------------------------------------
// helper
static void GetDefaultPrinterNameFromGlobalPrinters(nsAString& aPrinterName) {
  nsCOMPtr<nsIPrinterList> printerList =
      do_GetService("@mozilla.org/gfx/printerlist;1");
  if (printerList) {
    nsIPrinter* printer = nullptr;
    printerList->GetSystemDefaultPrinter(&printer);
    if (printer) {
      printer->GetName(aPrinterName);
    }
  }
}

//------------------------------------------------------------------
// Displays the native Print Dialog
static nsresult ShowNativePrintDialog(HWND aHWnd,
                                      nsIPrintSettings* aPrintSettings) {
  // NS_ENSURE_ARG_POINTER(aHWnd);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  // Get the Print Name to be used
  nsString printerName;
  aPrintSettings->GetPrinterName(printerName);

  // If there is no name then use the default printer
  if (printerName.IsEmpty()) {
    GetDefaultPrinterNameFromGlobalPrinters(printerName);
  } else {
    HANDLE hPrinter = nullptr;
    if (!::OpenPrinterW(const_cast<wchar_t*>(
                            static_cast<const wchar_t*>(printerName.get())),
                        &hPrinter, nullptr)) {
      // If the last used printer is not found, we should use default printer.
      GetDefaultPrinterNameFromGlobalPrinters(printerName);
    } else {
      ::ClosePrinter(hPrinter);
    }
  }

  // Now create a DEVNAMES struct so the the dialog is initialized correctly.

  uint32_t len = printerName.Length();
  nsHGLOBAL hDevNames =
      ::GlobalAlloc(GHND, sizeof(wchar_t) * (len + 1) + sizeof(DEVNAMES));
  nsAutoGlobalMem autoDevNames(hDevNames);
  if (!hDevNames) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  DEVNAMES* pDevNames = (DEVNAMES*)::GlobalLock(hDevNames);
  if (!pDevNames) {
    return NS_ERROR_FAILURE;
  }
  pDevNames->wDriverOffset = sizeof(DEVNAMES) / sizeof(wchar_t);
  pDevNames->wDeviceOffset = sizeof(DEVNAMES) / sizeof(wchar_t);
  pDevNames->wOutputOffset = sizeof(DEVNAMES) / sizeof(wchar_t) + len;
  pDevNames->wDefault = 0;

  memcpy(pDevNames + 1, printerName.get(), (len + 1) * sizeof(wchar_t));
  ::GlobalUnlock(hDevNames);

  // Create a Moveable Memory Object that holds a new DevMode
  // from the Printer Name
  // The PRINTDLG.hDevMode requires that it be a moveable memory object
  // NOTE: autoDevMode is automatically freed when any error occurred
  nsAutoGlobalMem autoDevMode(
      CreateGlobalDevModeAndInit(printerName, aPrintSettings));

  // Prepare to Display the Print Dialog
  PRINTDLGW prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLGW));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner = aHWnd;
  prntdlg.hDevMode = autoDevMode.get();
  prntdlg.hDevNames = hDevNames;
  prntdlg.hDC = nullptr;
  prntdlg.Flags =
      PD_ALLPAGES | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;

  // if there is a current selection then enable the "Selection" radio button
  bool isOn;
  aPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
  if (!isOn) {
    prntdlg.Flags |= PD_NOSELECTION;
  }

  int32_t pg = 1;
  aPrintSettings->GetStartPageRange(&pg);
  prntdlg.nFromPage = pg;

  aPrintSettings->GetEndPageRange(&pg);
  prntdlg.nToPage = pg;

  prntdlg.nMinPage = 1;
  prntdlg.nMaxPage = 0xFFFF;
  prntdlg.nCopies = 1;
  prntdlg.lpfnSetupHook = nullptr;
  prntdlg.lpSetupTemplateName = nullptr;
  prntdlg.hPrintTemplate = nullptr;
  prntdlg.hSetupTemplate = nullptr;

  prntdlg.hInstance = nullptr;
  prntdlg.lpPrintTemplateName = nullptr;

  prntdlg.lCustData = 0;
  prntdlg.lpfnPrintHook = nullptr;

  BOOL result;
  {
    mozilla::widget::WinUtils::AutoSystemDpiAware dpiAwareness;
    mozilla::BackgroundHangMonitor().NotifyWait();
    result = ::PrintDlgW(&prntdlg);
  }

  if (TRUE == result) {
    // check to make sure we don't have any nullptr pointers
    NS_ENSURE_TRUE(aPrintSettings && prntdlg.hDevMode, NS_ERROR_FAILURE);

    if (prntdlg.hDevNames == nullptr) {
      return NS_ERROR_FAILURE;
    }
    // Lock the deviceNames and check for nullptr
    DEVNAMES* devnames = (DEVNAMES*)::GlobalLock(prntdlg.hDevNames);
    if (devnames == nullptr) {
      return NS_ERROR_FAILURE;
    }

    char16_t* device = &(((char16_t*)devnames)[devnames->wDeviceOffset]);
    char16_t* driver = &(((char16_t*)devnames)[devnames->wDriverOffset]);

    // Check to see if the "Print To File" control is checked
    // then take the name from devNames and set it in the PrintSettings
    //
    // NOTE:
    // As per Microsoft SDK documentation the returned value offset from
    // devnames->wOutputOffset is either "FILE:" or nullptr
    // if the "Print To File" checkbox is checked it MUST be "FILE:"
    // We assert as an extra safety check.
    if (prntdlg.Flags & PD_PRINTTOFILE) {
      char16ptr_t fileName = &(((wchar_t*)devnames)[devnames->wOutputOffset]);
      NS_ASSERTION(wcscmp(fileName, L"FILE:") == 0, "FileName must be `FILE:`");
      aPrintSettings->SetToFileName(nsDependentString(fileName));
      aPrintSettings->SetPrintToFile(true);
    } else {
      // clear "print to file" info
      aPrintSettings->SetPrintToFile(false);
      aPrintSettings->SetToFileName(EmptyString());
    }

    nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
    if (!psWin) {
      return NS_ERROR_FAILURE;
    }

    // Setup local Data members
    psWin->SetDeviceName(nsDependentString(device));
    psWin->SetDriverName(nsDependentString(driver));

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    wprintf(L"printer: driver %s, device %s  flags: %d\n", driver, device,
            prntdlg.Flags);
#endif
    // fill the print options with the info from the dialog

    aPrintSettings->SetPrinterName(nsDependentString(device));

    if (prntdlg.Flags & PD_SELECTION) {
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);

    } else if (prntdlg.Flags & PD_PAGENUMS) {
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
      aPrintSettings->SetStartPageRange(prntdlg.nFromPage);
      aPrintSettings->SetEndPageRange(prntdlg.nToPage);

    } else {  // (prntdlg.Flags & PD_ALLPAGES)
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
    }

    // Unlock DeviceNames
    ::GlobalUnlock(prntdlg.hDevNames);

    // Transfer the settings from the native data to the PrintSettings
    LPDEVMODEW devMode = (LPDEVMODEW)::GlobalLock(prntdlg.hDevMode);
    if (!devMode || !prntdlg.hDC) {
      return NS_ERROR_FAILURE;
    }
    psWin->SetDevMode(devMode);  // copies DevMode
    psWin->CopyFromNative(prntdlg.hDC, devMode);
    ::GlobalUnlock(prntdlg.hDevMode);
    ::DeleteDC(prntdlg.hDC);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    bool printSelection = prntdlg.Flags & PD_SELECTION;
    bool printAllPages = prntdlg.Flags & PD_ALLPAGES;
    bool printNumPages = prntdlg.Flags & PD_PAGENUMS;
    int32_t fromPageNum = 0;
    int32_t toPageNum = 0;

    if (printNumPages) {
      fromPageNum = prntdlg.nFromPage;
      toPageNum = prntdlg.nToPage;
    }
    if (printSelection) {
      printf("Printing the selection\n");

    } else if (printAllPages) {
      printf("Printing all the pages\n");

    } else {
      printf("Printing from page no. %d to %d\n", fromPageNum, toPageNum);
    }
#endif

  } else {
    ::SetFocus(aHWnd);
    aPrintSettings->SetIsCancelled(true);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

//----------------------------------------------------------------------------------
//-- Show Print Dialog
//----------------------------------------------------------------------------------
nsresult NativeShowPrintDialog(HWND aHWnd, nsIPrintSettings* aPrintSettings) {
  nsresult rv = ShowNativePrintDialog(aHWnd, aPrintSettings);
  if (aHWnd) {
    ::DestroyWindow(aHWnd);
  }

  return rv;
}
