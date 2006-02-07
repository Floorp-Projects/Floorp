/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Jessica Blanco <jblanco@us.ibm.com>
 */

#ifndef nsPrintOptionsImpl_h__
#define nsPrintOptionsImpl_h__

#include "nsCOMPtr.h"
#include "nsIPrintOptions.h"  
#include "nsIPrintSettingsService.h"  
#include "nsIPrefBranch.h"

//class nsIPrintSettings;

//*****************************************************************************
//***    nsPrintOptions
//*****************************************************************************
class NS_GFX nsPrintOptions : public nsIPrintOptions, public nsIPrintSettingsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTOPTIONS
  NS_DECL_NSIPRINTSETTINGSSERVICE

  nsPrintOptions();
  virtual ~nsPrintOptions();

protected:
  void ReadBitFieldPref(const char * aPrefId, PRInt32 anOption);
  void WriteBitFieldPref(const char * aPrefId, PRInt32 anOption);
  void ReadJustification(const char * aPrefId, PRInt16& aJust, PRInt16 aInitValue);
  void WriteJustification(const char * aPrefId, PRInt16 aJust);
  void ReadInchesToTwipsPref(const char * aPrefId, nscoord&  aTwips);
  void WriteInchesFromTwipsPref(const char * aPrefId, nscoord aTwips);

  nsresult ReadPrefString(const char * aPrefId, nsString& aString);
  nsresult WritePrefString(const char * aPrefId, nsString& aString);
  nsresult WritePrefString(PRUnichar*& aStr, const char* aPrefId);
  nsresult ReadPrefDouble(const char * aPrefId, double& aVal);
  nsresult WritePrefDouble(const char * aPrefId, double aVal);

  virtual nsresult ReadPrefs(nsIPrintSettings* aPS, const nsString& aPrefName, PRUint32 aFlags);
  virtual nsresult WritePrefs(nsIPrintSettings* aPS, const nsString& aPrefName, PRUint32 aFlags);
  const char* GetPrefName(const char *     aPrefName, 
                          const nsString&  aPrinterName);
  
  // May be implemented by the platform-specific derived class                       
  virtual nsresult _CreatePrintSettings(nsIPrintSettings **_retval);

  // Members 
  nsCOMPtr<nsIPrintSettings> mGlobalPrintSettings;

  nsCAutoString mPrefName;

  nsCOMPtr<nsIPrefBranch> mPrefBranch;

  static nsFont* sDefaultFont;
};



#endif /* nsPrintOptions_h__ */
