/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsSharedPrefHandler_h__
#define nsSharedPrefHandler_h__

// Interfaces needed
#include "ipcITransactionService.h"
#include "ipcITransactionObserver.h"

// Includes
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"

// Local includes
#include "prefapi.h"

// Forward declarations
class nsPrefService;

// --------------------------------------------------------------------------------------
//  nsSharedPrefHandler
// --------------------------------------------------------------------------------------

class nsSharedPrefHandler : public ipcITransactionObserver
{
  friend nsresult NS_CreateSharedPrefHandler(nsPrefService*);
    
  NS_DECL_ISUPPORTS
  NS_DECL_IPCITRANSACTIONOBSERVER
    
public:    
  nsresult        OnSessionBegin();
  nsresult        OnSessionEnd();

  nsresult        OnSavePrefs();
  
  nsresult        OnPrefChanged(PRBool defaultPref,
                                PrefHashEntry* pref,
                                PrefValue newValue);

  void            ReadingUserPrefs(PRBool isReading)
                  { mReadingUserPrefs = isReading; }

  PRBool          IsPrefShared(const char* prefName);
  
protected:
                  nsSharedPrefHandler();
  virtual         ~nsSharedPrefHandler();

  nsresult        Init(nsPrefService *aOwner);
  nsresult        ReadExceptionFile();
  nsresult        EnsureTransactionService();
  
protected:
  nsPrefService   *mPrefService;      // weak ref
  
  nsCOMPtr<ipcITransactionService> mTransService;
  const nsCString mPrefsTSQueueName;
  
  PRPackedBool    mSessionActive;
  PRPackedBool    mReadingUserPrefs;
  PRPackedBool    mProcessingTransaction;
  
  nsAutoVoidArray mExceptionList;
};

// --------------------------------------------------------------------------------------

extern nsSharedPrefHandler *gSharedPrefHandler; // greasy, but used by procedural code.

/**
 * Global method to create gSharedPrefHandler
 *
 * @param aOwner
 */

extern nsresult NS_CreateSharedPrefHandler(nsPrefService *aOwner);
                                        
#endif // nsSharedPrefHandler_h__
