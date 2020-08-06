/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CUPSPrinterList.h"

#include "mozilla/Assertions.h"

namespace mozilla {

CUPSPrinterList::~CUPSPrinterList() {
  MOZ_ASSERT(mShim.IsInitialized());
  // cupsFreeDests is safe to call on a zero-length or null array.
  mShim.mCupsFreeDests(mNumPrinters, mPrinters);
}

void CUPSPrinterList::Initialize() {
  MOZ_ASSERT(mShim.IsInitialized());
  mNumPrinters = mShim.mCupsGetDests(&mPrinters);
}

cups_dest_t* CUPSPrinterList::FindPrinterByName(const char* aName) {
  MOZ_ASSERT(aName);
  return mShim.mCupsGetDest(aName, NULL, mNumPrinters, mPrinters);
}

cups_dest_t* CUPSPrinterList::GetPrinter(int i) {
  if (i < 0 || i >= mNumPrinters)
    MOZ_CRASH("CUPSPrinterList::GetPrinter out of range");
  return mPrinters + i;
}

}  // namespace mozilla
