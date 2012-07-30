/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsPrintOptionsWin.h"
#include "nsPrintSettingsWin.h"

#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
const char kPrinterEnumeratorContractID[] = "@mozilla.org/gfx/printerenumerator;1";

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsWin.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsWin::nsPrintOptionsWin()
{

}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsWin::~nsPrintOptionsWin()
{
}

/* nsIPrintSettings CreatePrintSettings (); */
nsresult nsPrintOptionsWin::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  *_retval = nullptr;
  nsPrintSettingsWin* printSettings = new nsPrintSettingsWin(); // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = printSettings); // ref count

  return NS_OK;
}

