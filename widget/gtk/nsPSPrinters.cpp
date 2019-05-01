/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCUPSShim.h"
#include "nsIServiceManager.h"
#include "nsPSPrinters.h"
#include "nsReadableUtils.h"  // StringBeginsWith()
#include "nsCUPSShim.h"
#include "mozilla/Preferences.h"

#include "prlink.h"
#include "prenv.h"
#include "plstr.h"

using namespace mozilla;

#define NS_CUPS_PRINTER "CUPS/"
#define NS_CUPS_PRINTER_LEN (sizeof(NS_CUPS_PRINTER) - 1)

/* dummy printer name for the gfx/src/ps driver */
#define NS_POSTSCRIPT_DRIVER_NAME "PostScript/"

nsCUPSShim gCupsShim;

nsPSPrinterList::nsPSPrinterList() {
  // Should we try cups?
  if (Preferences::GetBool("print.postscript.cups.enabled", true) &&
      !gCupsShim.IsInitialized()) {
    gCupsShim.Init();
  }
}

/* Check whether the PostScript module has been disabled at runtime */
bool nsPSPrinterList::Enabled() {
  const char* val = PR_GetEnv("MOZILLA_POSTSCRIPT_ENABLED");
  if (val && (val[0] == '0' || !PL_strcasecmp(val, "false"))) return false;

  // is the PS module enabled?
  return Preferences::GetBool("print.postscript.enabled", true);
}

/* Fetch a list of printers handled by the PostsScript module */
void nsPSPrinterList::GetPrinterList(nsTArray<nsCString>& aList) {
  aList.Clear();

  // Query CUPS for a printer list. The default printer goes to the
  // head of the output list; others are appended.
  if (gCupsShim.IsInitialized()) {
    cups_dest_t* dests;

    int num_dests = (gCupsShim.mCupsGetDests)(&dests);
    if (num_dests) {
      for (int i = 0; i < num_dests; i++) {
        nsAutoCString fullName(NS_CUPS_PRINTER);
        fullName.Append(dests[i].name);
        if (dests[i].instance != nullptr) {
          fullName.Append('/');
          fullName.Append(dests[i].instance);
        }
        if (dests[i].is_default)
          aList.InsertElementAt(0, fullName);
        else
          aList.AppendElement(fullName);
      }
    }
    (gCupsShim.mCupsFreeDests)(num_dests, dests);
  }

  // Build the "classic" list of printers -- those accessed by running
  // an opaque command. This list always contains a printer named "default".
  // In addition, we look for either an environment variable
  // MOZILLA_POSTSCRIPT_PRINTER_LIST or a preference setting
  // print.printer_list, which contains a space-separated list of printer
  // names.
  aList.AppendElement(NS_LITERAL_CSTRING(NS_POSTSCRIPT_DRIVER_NAME "default"));

  nsAutoCString list(PR_GetEnv("MOZILLA_POSTSCRIPT_PRINTER_LIST"));
  if (list.IsEmpty()) {
    Preferences::GetCString("print.printer_list", list);
  }
  if (!list.IsEmpty()) {
    // For each printer (except "default" which was already added),
    // construct a string "PostScript/<name>" and append it to the list.
    char* state;

    for (char* name = PL_strtok_r(list.BeginWriting(), " ", &state);
         nullptr != name; name = PL_strtok_r(nullptr, " ", &state)) {
      if (0 != strcmp(name, "default")) {
        nsAutoCString fullName(NS_POSTSCRIPT_DRIVER_NAME);
        fullName.Append(name);
        aList.AppendElement(fullName);
      }
    }
  }
}

/* Identify the printer type */
nsPSPrinterList::PrinterType nsPSPrinterList::GetPrinterType(
    const nsACString& aName) {
  if (StringBeginsWith(aName, NS_LITERAL_CSTRING(NS_POSTSCRIPT_DRIVER_NAME)))
    return kTypePS;
  else if (StringBeginsWith(aName, NS_LITERAL_CSTRING(NS_CUPS_PRINTER)))
    return kTypeCUPS;
  else
    return kTypeUnknown;
}
