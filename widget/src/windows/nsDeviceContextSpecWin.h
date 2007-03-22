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

#ifndef nsDeviceContextSpecWin_h___
#define nsDeviceContextSpecWin_h___

#include "nsCOMPtr.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintOptions.h" // For nsIPrinterEnumerator
#include "nsIPrintSettings.h"
#include "nsIWidget.h"
#include "nsISupportsPrimitives.h"
#include <windows.h>

class nsDeviceContextSpecWin : public nsIDeviceContextSpec
#ifndef MOZ_CAIRO_GFX
  , public nsISupportsVoid
#endif
{
public:
  nsDeviceContextSpecWin();

  NS_DECL_ISUPPORTS

#ifdef MOZ_CAIRO_GFX
  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **surface);
  NS_IMETHOD BeginDocument(PRUnichar*  aTitle, 
                           PRUnichar*  aPrintToFileName,
                           PRInt32     aStartPage, 
                           PRInt32     aEndPage) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD EndDocument() { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD BeginPage() { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD EndPage() { return NS_ERROR_NOT_IMPLEMENTED; }
#else
  // kill these when we move to CAIRO_GFX
  NS_IMETHOD GetType(PRUint16 *aType) { *aType = nsISupportsPrimitive::TYPE_VOID; return NS_OK; }
  NS_IMETHOD GetData(void * *aData);
  NS_IMETHOD SetData(void * aData) { return NS_ERROR_FAILURE; }
  NS_IMETHOD ToString(char **_retval) { return NS_ERROR_FAILURE; }
#endif

  NS_IMETHOD Init(nsIWidget* aWidget, nsIPrintSettings* aPS, PRBool aIsPrintPreview);

  void GetDriverName(char *&aDriverName) const   { aDriverName = mDriverName;     }
  void GetDeviceName(char *&aDeviceName) const   { aDeviceName = mDeviceName;     }

  // The GetDevMode will return a pointer to a DevMode
  // whether it is from the Global memory handle or just the DevMode
  // To get the DevMode from the Global memory Handle it must lock it 
  // So this call must be paired with a call to UnlockGlobalHandle
  void GetDevMode(LPDEVMODE &aDevMode);

  // helper functions
  nsresult GetDataFromPrinter(const PRUnichar * aName, nsIPrintSettings* aPS = nsnull);

  static nsresult SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, 
                                              LPDEVMODE         aDevMode);

protected:

  void SetDeviceName(char* aDeviceName);
  void SetDriverName(char* aDriverName);
  void SetDevMode(LPDEVMODE aDevMode);

  void SetupPaperInfoFromSettings();

  virtual ~nsDeviceContextSpecWin();

  char*     mDriverName;
  char*     mDeviceName;
  LPDEVMODE mDevMode;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
};


//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorWin : public nsIPrinterEnumerator
{
public:
  nsPrinterEnumeratorWin();
  ~nsPrinterEnumeratorWin();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR

private:
  // helper 
  nsresult DoEnumeratePrinters(PRBool aDoExtended, PRUint32* aCount, PRUnichar*** aResult);
};

#endif
