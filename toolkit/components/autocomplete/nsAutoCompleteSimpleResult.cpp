/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoCompleteSimpleResult.h"

NS_IMPL_ISUPPORTS2(nsAutoCompleteSimpleResult,
                   nsIAutoCompleteResult,
                   nsIAutoCompleteSimpleResult)

nsAutoCompleteSimpleResult::nsAutoCompleteSimpleResult() :
  mDefaultIndex(-1),
  mSearchResult(RESULT_NOMATCH),
  mTypeAheadResult(false)
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

// typeAheadResult
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetTypeAheadResult(bool *aTypeAheadResult)
{
  *aTypeAheadResult = mTypeAheadResult;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetTypeAheadResult(bool aTypeAheadResult)
{
  mTypeAheadResult = aTypeAheadResult;
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
