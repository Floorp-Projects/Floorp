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

#include "nsAutoCompleteMdbResult.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"

NS_INTERFACE_MAP_BEGIN(nsAutoCompleteMdbResult)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteResult)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteBaseResult)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteMdbResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAutoCompleteResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsAutoCompleteMdbResult)
NS_IMPL_RELEASE(nsAutoCompleteMdbResult)

nsAutoCompleteMdbResult::nsAutoCompleteMdbResult() :
  mDefaultIndex(-1),
  mSearchResult(nsIAutoCompleteResult::RESULT_IGNORED)
{
}

nsAutoCompleteMdbResult::~nsAutoCompleteMdbResult()
{

}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteResult

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetSearchString(nsAString &aSearchString)
{
  aSearchString = mSearchString;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetSearchResult(PRUint16 *aSearchResult)
{
  *aSearchResult = mSearchResult;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetDefaultIndex(PRInt32 *aDefaultIndex)
{
  *aDefaultIndex = mDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetErrorDescription(nsAString & aErrorDescription)
{
  aErrorDescription = mErrorDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetMatchCount(PRUint32 *aMatchCount)
{
  *aMatchCount = mResults.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetValueAt(PRInt32 aIndex, nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < mResults.Count(), NS_ERROR_ILLEGAL_VALUE);

  nsIMdbRow *row = mResults.ObjectAt(aIndex);
  if (!row) return NS_OK;

  if (mValueType == kUnicharType) {
    GetRowValue(row, mValueToken, _retval);
  } else if (mValueType == kCharType) {
    nsCAutoString value;
    GetUTF8RowValue(row, mValueToken, value);
    _retval = NS_ConvertUTF8toUCS2(value);
  }
  
  /* // TESTING: return ordinaly labeled values   
  char *value = new char(20);
  sprintf(value, "foopy (%d)", aIndex);
  
  nsAutoString result;
  result.AssignWithConversion(value);
  _retval = result;*/

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetCommentAt(PRInt32 aIndex, nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < mResults.Count(), NS_ERROR_ILLEGAL_VALUE);

  nsIMdbRow *row = mResults.ObjectAt(aIndex);
  if (!row) return NS_OK;

  if (mCommentType == kUnicharType) {
    GetRowValue(row, mCommentToken, _retval);
  } else if (mCommentType == kCharType) {
    nsCAutoString value;
    GetUTF8RowValue(row, mCommentToken, value);
    _retval = NS_ConvertUTF8toUCS2(value);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetStyleAt(PRInt32 aIndex, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteBaseResult

NS_IMETHODIMP
nsAutoCompleteMdbResult::SetSearchString(const nsAString &aSearchString)
{
  mSearchString.Assign(aSearchString);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::SetErrorDescription(const nsAString &aErrorDescription)
{
  mErrorDescription.Assign(aErrorDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::SetDefaultIndex(PRInt32 aDefaultIndex)
{
  mDefaultIndex = aDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::SetSearchResult(PRUint32 aSearchResult)
{
  mSearchResult = aSearchResult;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIAutoCompleteMdbResult

NS_IMETHODIMP
nsAutoCompleteMdbResult::Init(nsIMdbEnv *aEnv, nsIMdbTable *aTable)
{
  mEnv = aEnv;
  mTable = aTable;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::SetTokens(mdb_scope aValueToken, PRInt16 aValueType, mdb_scope aCommentToken, PRInt16 aCommentType)
{
  mValueToken = aValueToken;
  mValueType = aValueType;
  mCommentToken = aCommentToken;
  mCommentType = aCommentType;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::AddRow(nsIMdbRow *aRow)
{
  mResults.AppendObject(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::RemoveRowAt(PRUint32 aRowIndex, PRBool aRemoveFromDb)
{
  nsIMdbRow *row = mResults.ObjectAt(aRowIndex);
  NS_ENSURE_TRUE(row, NS_ERROR_INVALID_ARG);

  mResults.RemoveObjectAt(aRowIndex);

  if (aRemoveFromDb && mTable && mEnv) {
    mdb_err err = mTable->CutRow(mEnv, row);
    NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetRowAt(PRUint32 aRowIndex, nsIMdbRow **aRow)
{
  *aRow = mResults.ObjectAt(aRowIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteMdbResult::GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString &aValue)
{
  mdbYarn yarn;
  mdb_err err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0)
    return NS_ERROR_FAILURE;

  aValue.Truncate(0);
  if (!yarn.mYarn_Fill)
    return NS_OK;
  
  switch (yarn.mYarn_Form) {
    case 0: // unicode
      aValue.Assign((const PRUnichar *)yarn.mYarn_Buf, yarn.mYarn_Fill/sizeof(PRUnichar));
      break;
    case 1: // utf 8
      aValue.Assign(NS_ConvertUTF8toUCS2((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill));
      break;
    default:
      return NS_ERROR_UNEXPECTED;
  }
  
  return NS_OK;
}


nsresult
nsAutoCompleteMdbResult::GetUTF8RowValue(nsIMdbRow *aRow, mdb_column aCol,
					 nsACString& aValue)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  const char* startPtr = (const char*)yarn.mYarn_Buf;
  if (startPtr)
    aValue.Assign(Substring(startPtr, startPtr + yarn.mYarn_Fill));
  else
    aValue.Truncate();
  
  return NS_OK;
}

nsresult
nsAutoCompleteMdbResult::GetIntRowValue(nsIMdbRow *aRow, mdb_column aCol,
					PRInt32 *aValue)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  if (yarn.mYarn_Buf)
    *aValue = atoi((char *)yarn.mYarn_Buf);
  else
    *aValue = 0;
  
  return NS_OK;
}
