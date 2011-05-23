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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Nesse <bnesse@netscape.com>
 *   Mats Palmgren <matspal@gmail.com>
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

#ifndef mozilla_Preferences_h
#define mozilla_Preferences_h

#ifndef MOZILLA_INTERNAL_API
#error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

class nsIFile;
class nsCString;
class nsString;

namespace mozilla {

class Preferences : public nsIPrefService,
                    public nsIPrefServiceInternal,
                    public nsIObserver,
                    public nsIPrefBranchInternal,
                    public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFSERVICE
  NS_DECL_NSIPREFSERVICEINTERNAL
  NS_FORWARD_NSIPREFBRANCH(mRootBranch->)
  NS_FORWARD_NSIPREFBRANCH2(mRootBranch->)
  NS_DECL_NSIOBSERVER

  Preferences();
  virtual ~Preferences();

  nsresult Init();

  /**
   * Returns the singleton instance which is addreffed.
   */
  static Preferences* GetInstance();

  /**
   * Finallizes global members.
   */
  static void Shutdown();

  /**
   * Returns shared pref service instance
   * NOTE: not addreffed.
   */
  static nsIPrefService* GetService() { return sPreferences; }

  /**
   * Returns shared pref branch instance.
   * NOTE: not addreffed.
   */
  static nsIPrefBranch2* GetRootBranch()
  {
    return sPreferences ? sPreferences->mRootBranch.get() : nsnull;
  }

  /**
   * Gets int or bool type pref value with default value if failed to get
   * the pref.
   */
  static PRBool GetBool(const char* aPref, PRBool aDefault = PR_FALSE)
  {
    PRBool result = aDefault;
    GetBool(aPref, &result);
    return result;
  }

  static PRInt32 GetInt(const char* aPref, PRInt32 aDefault = 0)
  {
    PRInt32 result = aDefault;
    GetInt(aPref, &result);
    return result;
  }

  /**
   * Gets int or bool type pref value with raw return value of nsIPrefBranch.
   *
   * @param aPref       A pref name.
   * @param aResult     Must not be NULL.  The value is never modified when
   *                    these methods fail.
   */
  static nsresult GetBool(const char* aPref, PRBool* aResult);
  static nsresult GetInt(const char* aPref, PRInt32* aResult);

  /**
   * Gets string type pref value with raw return value of nsIPrefBranch.
   *
   * @param aPref       A pref name.
   * @param aResult     Must not be NULL.  The value is never modified when
   *                    these methods fail.
   */
  static nsresult GetChar(const char* aPref, nsCString* aResult);
  static nsresult GetChar(const char* aPref, nsString* aResult);
  static nsresult GetLocalizedString(const char* aPref, nsString* aResult);

  /**
   * Sets various type pref values.
   */
  static nsresult SetBool(const char* aPref, PRBool aValue);
  static nsresult SetInt(const char* aPref, PRInt32 aValue);
  static nsresult SetChar(const char* aPref, const char* aValue);
  static nsresult SetChar(const char* aPref, const nsCString &aValue);
  static nsresult SetChar(const char* aPref, const PRUnichar* aValue);
  static nsresult SetChar(const char* aPref, const nsString &aValue);

  /**
   * Clears user set pref.
   */
  static nsresult ClearUser(const char* aPref);

  /**
   * Adds/Removes the observer for the root pref branch.
   * The observer is referenced strongly if AddStrongObserver is used.  On the
   * other hand, it is referenced weakly, if AddWeakObserver is used.
   * See nsIPrefBran2.idl for the detail.
   */
  static nsresult AddStrongObserver(nsIObserver* aObserver, const char* aPref);
  static nsresult AddWeakObserver(nsIObserver* aObserver, const char* aPref);
  static nsresult RemoveObserver(nsIObserver* aObserver, const char* aPref);

protected:
  nsresult NotifyServiceObservers(const char *aSubject);
  nsresult UseDefaultPrefFile();
  nsresult UseUserPrefFile();
  nsresult ReadAndOwnUserPrefFile(nsIFile *aFile);
  nsresult ReadAndOwnSharedUserPrefFile(nsIFile *aFile);
  nsresult SavePrefFileInternal(nsIFile* aFile);
  nsresult WritePrefFile(nsIFile* aFile);
  nsresult MakeBackupPrefFile(nsIFile *aFile);

private:
  nsCOMPtr<nsIPrefBranch2> mRootBranch;
  nsCOMPtr<nsIFile>        mCurrentFile;

  static Preferences*      sPreferences;
  static PRBool            sShutdown;

  /**
   * Init static members.  TRUE if it succeeded.  Otherwise, FALSE.
   */
  static PRBool InitStaticMembers();
};

} // namespace mozilla

#endif // mozilla_Preferences_h
