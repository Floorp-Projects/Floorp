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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef nsDeviceContextSpecB_h___
#define nsDeviceContextSpecB_h___

#include "nsCOMPtr.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h" 
#include "nsIPrintOptions.h" 
// For public interface?
#include "nsIWidget.h" 
#include "nsPrintdBeOS.h" 
 
class nsDeviceContextSpecBeOS : public nsIDeviceContextSpec
{
public:
/**
 * Construct a nsDeviceContextSpecBeOS, which is an object which contains and manages a mac printrecord
 * @update  dc 12/02/98
 */
  nsDeviceContextSpecBeOS();

  NS_DECL_ISUPPORTS

/**
 * Initialize the nsDeviceContextSpecBeOS for use.  This will allocate a printrecord for use
 * @update   dc 2/16/98
 * @param aWidget         Unused
 * @param aPS             Settings for this print job
 * @param aIsPrintPreview Unused
 * @return error status
 */
  NS_IMETHOD Init(nsIWidget *aWidget,
                  nsIPrintSettings* aPS,
                  PRBool aIsPrintPreview);
  
  
/**
 * Closes the printmanager if it is open.
 * @update   dc 2/13/98
 * @return error status
 */
  NS_IMETHOD ClosePrintManager();

  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

/**
 * Destructor for nsDeviceContextSpecBeOS, this will release the printrecord
 * @update  dc 2/16/98
 */
protected:
  virtual ~nsDeviceContextSpecBeOS();
 
public:
  int InitializeGlobalPrinters();
  void FreeGlobalPrinters();

protected:
  nsCOMPtr<nsIPrintSettings> mPrintSettings;	
};

//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorBeOS : public nsIPrinterEnumerator
{
public:
  nsPrinterEnumeratorBeOS();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR

protected:
};

#endif
