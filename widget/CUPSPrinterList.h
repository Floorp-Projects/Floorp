/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCUPSPrinterList_h___
#define nsCUPSPrinterList_h___

#include "nsCUPSShim.h"

namespace mozilla {

/**
 * @brief Enumerates all CUPS printers and provides lookup by name.
 */
class CUPSPrinterList {
 public:
  CUPSPrinterList(const CUPSPrinterList&) = delete;
  explicit CUPSPrinterList(const nsCUPSShim& aShim) : mShim(aShim) {}
  ~CUPSPrinterList();

  /**
   * @brief fetches the printer list.
   *
   * This is a blocking call.
   */
  void Initialize();

  /**
   * @brief Looks up a printer by CUPS name.
   */
  cups_dest_t* FindPrinterByName(const char* name);

  int NumPrinters() const { return mNumPrinters; }
  /**
   * @brief Gets a printer by index.
   *
   * This does NOT copy the printer's data.
   */
  cups_dest_t* GetPrinter(int i);

 private:
  const nsCUPSShim& mShim;
  cups_dest_t* mPrinters = nullptr;
  int mNumPrinters = 0;
};

}  // namespace mozilla

#endif /* nsCUPSPrinter_h___ */
