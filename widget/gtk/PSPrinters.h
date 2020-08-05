/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PSPrinters_h___
#define PSPrinters_h___

#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

class PSPrinters {
  PSPrinters() = delete;

 public:

  /**
   * Is the PostScript module enabled or disabled?
   * @return true if enabled,
   *         false if not.
   */
  static bool Enabled();

  /**
   * Obtain a list of printers (print destinations) supported by the
   * PostScript module, Each entry will be in the form <type>/<name>,
   * where <type> is a printer type string, and <name> is the actual
   * printer name.
   *
   * @param aList Upon return, this is populated with the list of
   *              printer names as described above, replacing any
   *              previous contents. Each entry is a UTF8 string.
   *              There should always be at least one entry. The
   *              first entry is the default print destination.
   */
  static void GetPrinterList(nsTArray<nsCString>& aList);

  /**
   * Identify if a printer is a PS printer from its name.
   * @param aName The printer's full name as a UTF8 string, including
   *              the <type> portion as described for GetPrinterList().
   * @return The PrinterType value for this name.
   */
  static bool IsPSPrinter(const nsACString& aName);
};

}  // namespace mozilla

#endif /* PSPrinters_h___ */
