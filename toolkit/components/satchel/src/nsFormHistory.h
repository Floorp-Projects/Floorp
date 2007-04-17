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
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#ifndef __nsFormHistory__
#define __nsFormHistory__

#include "nsIFormHistory.h"
#include "nsIAutoCompleteResultTypes.h"
#include "nsIFormSubmitObserver.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"
#include "mdb.h"
#include "nsIServiceManager.h"
#include "nsToolkitCompsCID.h"

class nsFormHistory : public nsIFormHistory2,
                      public nsIObserver,
                      public nsIFormSubmitObserver,
                      public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFORMHISTORY2
  NS_DECL_NSIOBSERVER
  
  // nsIFormSubmitObserver
  NS_IMETHOD Notify(nsIDOMHTMLFormElement* formElt, nsIDOMWindowInternal* window, nsIURI* actionURL, PRBool* cancelSubmit);

  nsFormHistory();
  virtual ~nsFormHistory();
  nsresult Init();

  static nsFormHistory *GetInstance()
    {
      if (!gFormHistory) {
        nsCOMPtr<nsIFormHistory2> fh = do_GetService(NS_FORMHISTORY_CONTRACTID);
      }
      return gFormHistory;
    }

  nsresult AutoCompleteSearch(const nsAString &aInputName, const nsAString &aInputValue,
                              nsIAutoCompleteMdbResult2 *aPrevResult, nsIAutoCompleteResult **aNewResult);

  static mdb_column kToken_ValueColumn;
  static mdb_column kToken_NameColumn;

protected:
  // Database I/O
  nsresult OpenDatabase();
  nsresult OpenExistingFile(const char *aPath);
  nsresult CreateNewFile(const char *aPath);
  nsresult CloseDatabase();
  nsresult CreateTokens();
  nsresult Flush();
  nsresult CopyRowsFromTable(nsIMdbTable *sourceTable);
  
  mdb_err UseThumb(nsIMdbThumb *aThumb, PRBool *aDone);
  
  nsresult AppendRow(const nsAString &aValue, const nsAString &aName, nsIMdbRow **aResult);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const nsAString &aValue);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString &aValue);
  
  PRBool RowMatch(nsIMdbRow *aRow, const nsAString &aInputName, const nsAString &aInputValue, PRUnichar **aValue);
  
  PR_STATIC_CALLBACK(int) SortComparison(const void *v1, const void *v2, void *closureVoid);

  nsresult EntriesExistInternal(const nsAString *aName, const nsAString *aValue, PRBool *_retval);

  nsresult RemoveEntriesInternal(const nsAString *aName);

  nsresult InitByteOrder(PRBool aForce);
  nsresult GetByteOrder(nsAString& aByteOrder);
  nsresult SaveByteOrder(const nsAString& aByteOrder);

  static PRBool FormHistoryEnabled();

  static nsFormHistory *gFormHistory;

  static PRBool gFormHistoryEnabled;
  static PRBool gPrefsInitialized;

  nsCOMPtr<nsIMdbFactory> mMdbFactory;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsIMdbEnv* mEnv;
  nsIMdbStore* mStore;
  nsIMdbTable* mTable;
  PRInt64 mFileSizeOnDisk;
  nsCOMPtr<nsIMdbRow> mMetaRow;
  PRPackedBool mReverseByteOrder;
  
  // database tokens
  mdb_scope kToken_RowScope;
  mdb_kind kToken_Kind;
  mdb_column kToken_ByteOrder;
};

#endif // __nsFormHistory__
