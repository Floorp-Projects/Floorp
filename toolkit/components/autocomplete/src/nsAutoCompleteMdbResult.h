/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsAutoCompleteResultBase__
#define __nsAutoCompleteResultBase__

#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteResultTypes.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "mdb.h"

class nsAutoCompleteMdbResult : public nsIAutoCompleteMdbResult
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT

  nsAutoCompleteMdbResult();
  virtual ~nsAutoCompleteMdbResult();

  // nsIAutoCompleteBaseResult
  NS_IMETHOD SetSearchString(const nsAString &aSearchString);
  NS_IMETHOD SetErrorDescription(const nsAString &aErrorDescription);
  NS_IMETHOD SetDefaultIndex(PRInt32 aDefaultIndex);
  NS_IMETHOD SetSearchResult(PRUint32 aSearchResult);

  // nsIAutoCompleteMdbResult
  NS_IMETHOD Init(nsIMdbEnv *aEnv, nsIMdbTable *aTable);
  NS_IMETHOD SetTokens(mdb_scope aValueToken, eDataType aValueType, mdb_scope aCommentToken, eDataType aCommentType);
  NS_IMETHOD AddRow(nsIMdbRow *aRow);
  NS_IMETHOD RemoveRowAt(PRUint32 aRowIndex);
  NS_IMETHOD GetRowAt(PRUint32 aRowIndex, nsIMdbRow **aRow);
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString &aValue);
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsACString &aValue);
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt32 *aValue);
  
protected:
  nsAutoVoidArray mResults;

  nsAutoString mSearchString;
  nsAutoString mErrorDescription;
  PRInt32 mDefaultIndex;
  PRUint32 mSearchResult;
  
  nsIMdbEnv *mEnv;
  nsIMdbTable *mTable;
  
  mdb_scope mValueToken;
  eDataType mValueType;
  mdb_scope mCommentToken;
  eDataType mCommentType;
};

#endif // __nsAutoCompleteResultBase__
