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

#ifndef __nsIAutoCompleteResultTypes__
#define __nsIAutoCompleteResultTypes__

#include "nsIAutoCompleteResult.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "mdb.h"

class nsIAutoCompleteBaseResult : public nsIAutoCompleteResult
{
public:
  NS_IMETHOD SetSearchString(const nsAString &aSearchString) = 0;
  NS_IMETHOD SetErrorDescription(const nsAString &aErrorDescription) = 0;
  NS_IMETHOD SetDefaultIndex(PRInt32 aDefaultIndex) = 0;
  NS_IMETHOD SetSearchResult(PRUint32 aSearchResult) = 0;
};

class nsIAutoCompleteMdbResult : public nsIAutoCompleteBaseResult
{
public:
  enum eDataType {
    kUnicharType,
    kCharType,
    kIntType
  };

  NS_IMETHOD Init(nsIMdbEnv *aEnv, nsIMdbTable *aTable) = 0;
  NS_IMETHOD SetTokens(mdb_scope aValueToken, eDataType aValueType, mdb_scope aCommentToken, eDataType aCommentType) = 0;
  NS_IMETHOD AddRow(nsIMdbRow *aRow) = 0;
  NS_IMETHOD RemoveRowAt(PRUint32 aRowIndex) = 0;
  NS_IMETHOD GetRowAt(PRUint32 aRowIndex, nsIMdbRow **aRow) = 0;
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString &aValue) = 0;
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsACString &aValue) = 0;
  NS_IMETHOD GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt32 *aValue) = 0;
};

#endif // __nsIAutoCompleteResultTypes__
