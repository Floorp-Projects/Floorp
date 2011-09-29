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
 * The Original Code is autocomplete code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
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

#include "nsAutoCompleteSimpleResult.h"

NS_IMPL_ISUPPORTS2(nsAutoCompleteSimpleResult,
                   nsIAutoCompleteResult,
                   nsIAutoCompleteSimpleResult)

nsAutoCompleteSimpleResult::nsAutoCompleteSimpleResult() :
  mDefaultIndex(-1),
  mSearchResult(RESULT_NOMATCH)
{
}

// searchString
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetSearchString(nsAString &aSearchString)
{
  aSearchString = mSearchString;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetSearchString(const nsAString &aSearchString)
{
  mSearchString.Assign(aSearchString);
  return NS_OK;
}

// searchResult
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetSearchResult(PRUint16 *aSearchResult)
{
  *aSearchResult = mSearchResult;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetSearchResult(PRUint16 aSearchResult)
{
  mSearchResult = aSearchResult;
  return NS_OK;
}

// defaultIndex
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetDefaultIndex(PRInt32 *aDefaultIndex)
{
  *aDefaultIndex = mDefaultIndex;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetDefaultIndex(PRInt32 aDefaultIndex)
{
  mDefaultIndex = aDefaultIndex;
  return NS_OK;
}

// errorDescription
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetErrorDescription(nsAString & aErrorDescription)
{
  aErrorDescription = mErrorDescription;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetErrorDescription(
                                             const nsAString &aErrorDescription)
{
  mErrorDescription.Assign(aErrorDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::AppendMatch(const nsAString& aValue,
                                        const nsAString& aComment,
                                        const nsAString& aImage,
                                        const nsAString& aStyle)
{
  CheckInvariants();

  if (! mValues.AppendElement(aValue))
    return NS_ERROR_OUT_OF_MEMORY;
  if (! mComments.AppendElement(aComment)) {
    mValues.RemoveElementAt(mValues.Length() - 1);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (! mImages.AppendElement(aImage)) {
    mValues.RemoveElementAt(mValues.Length() - 1);
    mComments.RemoveElementAt(mComments.Length() - 1);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (! mStyles.AppendElement(aStyle)) {
    mValues.RemoveElementAt(mValues.Length() - 1);
    mComments.RemoveElementAt(mComments.Length() - 1);
    mImages.RemoveElementAt(mImages.Length() - 1);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetMatchCount(PRUint32 *aMatchCount)
{
  CheckInvariants();

  *aMatchCount = mValues.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetValueAt(PRInt32 aIndex, nsAString& _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < PRInt32(mValues.Length()),
                 NS_ERROR_ILLEGAL_VALUE);
  CheckInvariants();

  _retval = mValues[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetLabelAt(PRInt32 aIndex, nsAString& _retval)
{
  return GetValueAt(aIndex, _retval);
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetCommentAt(PRInt32 aIndex, nsAString& _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < PRInt32(mComments.Length()),
                 NS_ERROR_ILLEGAL_VALUE);
  CheckInvariants();
  _retval = mComments[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetImageAt(PRInt32 aIndex, nsAString& _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < PRInt32(mImages.Length()),
                 NS_ERROR_ILLEGAL_VALUE);
  CheckInvariants();
  _retval = mImages[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetStyleAt(PRInt32 aIndex, nsAString& _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < PRInt32(mStyles.Length()),
                 NS_ERROR_ILLEGAL_VALUE);
  CheckInvariants();
  _retval = mStyles[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetListener(nsIAutoCompleteSimpleResultListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::RemoveValueAt(PRInt32 aRowIndex,
                                          bool aRemoveFromDb)
{
  NS_ENSURE_TRUE(aRowIndex >= 0 && aRowIndex < PRInt32(mValues.Length()),
                 NS_ERROR_ILLEGAL_VALUE);

  nsAutoString removedValue(mValues[aRowIndex]);
  mValues.RemoveElementAt(aRowIndex);
  mComments.RemoveElementAt(aRowIndex);
  mImages.RemoveElementAt(aRowIndex);
  mStyles.RemoveElementAt(aRowIndex);

  if (mListener)
    mListener->OnValueRemoved(this, removedValue, aRemoveFromDb);

  return NS_OK;
}
