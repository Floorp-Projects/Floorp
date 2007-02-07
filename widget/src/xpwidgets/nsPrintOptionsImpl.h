/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Jessica Blanco <jblanco@us.ibm.com>
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

#ifndef nsPrintOptionsImpl_h__
#define nsPrintOptionsImpl_h__

#include "nsCOMPtr.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrefBranch.h"
#include "nsString.h"
#include "nsFont.h"

/**
 *   Class nsPrintOptions
 */
class nsPrintOptions : public nsIPrintOptions,
                       public nsIPrintSettingsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTOPTIONS
  NS_DECL_NSIPRINTSETTINGSSERVICE

  /**
   * method Init
   *  Initializes member variables. Every consumer that does manual
   *  creation (instead of do_CreateInstance) needs to call this method
   *  immediately after instantiation.
   */
  virtual nsresult Init();

  nsPrintOptions();
  virtual ~nsPrintOptions();

protected:
  void ReadBitFieldPref(const char * aPrefId, PRInt32 anOption);
  void WriteBitFieldPref(const char * aPrefId, PRInt32 anOption);
  void ReadJustification(const char * aPrefId, PRInt16& aJust,
                         PRInt16 aInitValue);
  void WriteJustification(const char * aPrefId, PRInt16 aJust);
  void ReadInchesToTwipsPref(const char * aPrefId, nscoord&  aTwips,
                             const char * aMarginPref);
  void WriteInchesFromTwipsPref(const char * aPrefId, nscoord aTwips);

  nsresult ReadPrefString(const char * aPrefId, nsAString& aString);
  /**
   * method WritePrefString
   *   Writes PRUnichar* to Prefs and deletes the contents of aString
   */
  nsresult WritePrefString(const char * aPrefId, const nsAString& aString);
  nsresult WritePrefString(PRUnichar*& aStr, const char* aPrefId);
  nsresult ReadPrefDouble(const char * aPrefId, double& aVal);
  nsresult WritePrefDouble(const char * aPrefId, double aVal);

  /**
   * method ReadPrefs
   * @param aPS          a pointer to the printer settings
   * @param aPrinterName the name of the printer for which to read prefs
   * @param aFlags       flag specifying which prefs to read
   */
  virtual nsresult ReadPrefs(nsIPrintSettings* aPS, const nsAString&
                             aPrinterName, PRUint32 aFlags);
  /**
   * method WritePrefs
   * @param aPS          a pointer to the printer settings
   * @param aPrinterName the name of the printer for which to write prefs
   * @param aFlags       flag specifying which prefs to read
   */
  virtual nsresult WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrefName,
                              PRUint32 aFlags);
  const char* GetPrefName(const char *     aPrefName,
                          const nsAString&  aPrinterName);

  /**
   * method _CreatePrintSettings
   *   May be implemented by the platform-specific derived class
   *
   * @return             printer settings instance
   */
  virtual nsresult _CreatePrintSettings(nsIPrintSettings **_retval);

  // Members
  nsCOMPtr<nsIPrintSettings> mGlobalPrintSettings;

  nsCString mPrefName;

  nsCOMPtr<nsIPrefBranch> mPrefBranch;

private:
  // These are not supported and are not implemented!
  nsPrintOptions(const nsPrintOptions& x);
  nsPrintOptions& operator=(const nsPrintOptions& x);
};

#endif /* nsPrintOptionsImpl_h__ */
