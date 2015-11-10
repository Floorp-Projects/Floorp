/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoCompleteSimpleResult.h"

#define CHECK_MATCH_INDEX(_index, _insert)                                     \
  if (_index < 0 ||                                                            \
      static_cast<MatchesArray::size_type>(_index) > mMatches.Length() ||      \
      (!_insert && static_cast<MatchesArray::size_type>(_index) == mMatches.Length())) { \
    MOZ_ASSERT(false, "Trying to use an invalid index on mMatches");           \
    return NS_ERROR_ILLEGAL_VALUE;                                             \
  }                                                                            \

NS_IMPL_ISUPPORTS(nsAutoCompleteSimpleResult,
                  nsIAutoCompleteResult,
                  nsIAutoCompleteSimpleResult)

struct AutoCompleteSimpleResultMatch
{
  AutoCompleteSimpleResultMatch(const nsAString& aValue,
                                const nsAString& aComment,
                                const nsAString& aImage,
                                const nsAString& aStyle,
                                const nsAString& aFinalCompleteValue,
                                const nsAString& aLabel)
    : mValue(aValue)
    , mComment(aComment)
    , mImage(aImage)
    , mStyle(aStyle)
    , mFinalCompleteValue(aFinalCompleteValue)
    , mLabel(aLabel)
  {
  }

  nsString mValue;
  nsString mComment;
  nsString mImage;
  nsString mStyle;
  nsString mFinalCompleteValue;
  nsString mLabel;
};

nsAutoCompleteSimpleResult::nsAutoCompleteSimpleResult() :
  mDefaultIndex(-1),
  mSearchResult(RESULT_NOMATCH),
  mTypeAheadResult(false)
{
}

nsresult
nsAutoCompleteSimpleResult::AppendResult(nsIAutoCompleteResult* aResult)
{
  nsAutoString searchString;
  nsresult rv = aResult->GetSearchString(searchString);
  NS_ENSURE_SUCCESS(rv, rv);
  mSearchString = searchString;

  uint16_t searchResult;
  rv = aResult->GetSearchResult(&searchResult);
  NS_ENSURE_SUCCESS(rv, rv);
  mSearchResult = searchResult;

  nsAutoString errorDescription;
  if (NS_SUCCEEDED(aResult->GetErrorDescription(errorDescription)) &&
      !errorDescription.IsEmpty()) {
    mErrorDescription = errorDescription;
  }

  bool typeAheadResult = false;
  if (NS_SUCCEEDED(aResult->GetTypeAheadResult(&typeAheadResult)) &&
      typeAheadResult) {
    mTypeAheadResult = typeAheadResult;
  }

  int32_t defaultIndex = -1;
  if (NS_SUCCEEDED(aResult->GetDefaultIndex(&defaultIndex)) &&
      defaultIndex >= 0) {
    mDefaultIndex = defaultIndex;
  }

  nsCOMPtr<nsIAutoCompleteSimpleResult> simpleResult =
    do_QueryInterface(aResult);
  if (simpleResult) {
    nsCOMPtr<nsIAutoCompleteSimpleResultListener> listener;
    if (NS_SUCCEEDED(simpleResult->GetListener(getter_AddRefs(listener))) &&
        listener) {
      listener.swap(mListener);
    }
  }

  // Copy matches.
  uint32_t matchCount = 0;
  rv = aResult->GetMatchCount(&matchCount);
  NS_ENSURE_SUCCESS(rv, rv);
  for (size_t i = 0; i < matchCount; ++i) {
    nsAutoString value, comment, image, style, finalCompleteValue, label;

    rv = aResult->GetValueAt(i, value);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aResult->GetCommentAt(i, comment);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aResult->GetImageAt(i, image);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aResult->GetStyleAt(i, style);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aResult->GetFinalCompleteValueAt(i, finalCompleteValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aResult->GetLabelAt(i, label);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AppendMatch(value, comment, image, style, finalCompleteValue, label);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
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
nsAutoCompleteSimpleResult::GetSearchResult(uint16_t *aSearchResult)
{
  *aSearchResult = mSearchResult;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetSearchResult(uint16_t aSearchResult)
{
  mSearchResult = aSearchResult;
  return NS_OK;
}

// defaultIndex
NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetDefaultIndex(int32_t *aDefaultIndex)
{
  *aDefaultIndex = mDefaultIndex;
  return NS_OK;
}
NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetDefaultIndex(int32_t aDefaultIndex)
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
nsAutoCompleteSimpleResult::InsertMatchAt(int32_t aIndex,
                                          const nsAString& aValue,
                                          const nsAString& aComment,
                                          const nsAString& aImage,
                                          const nsAString& aStyle,
                                          const nsAString& aFinalCompleteValue,
                                          const nsAString& aLabel)
{
  CHECK_MATCH_INDEX(aIndex, true);

  AutoCompleteSimpleResultMatch match(aValue, aComment, aImage, aStyle, aFinalCompleteValue, aLabel);

  if (!mMatches.InsertElementAt(aIndex, match)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::AppendMatch(const nsAString& aValue,
                                        const nsAString& aComment,
                                        const nsAString& aImage,
                                        const nsAString& aStyle,
                                        const nsAString& aFinalCompleteValue,
                                        const nsAString& aLabel)
{
  return InsertMatchAt(mMatches.Length(), aValue, aComment, aImage, aStyle,
                       aFinalCompleteValue, aLabel);
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetMatchCount(uint32_t *aMatchCount)
{
  *aMatchCount = mMatches.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetValueAt(int32_t aIndex, nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetLabelAt(int32_t aIndex, nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mLabel;
  if (_retval.IsEmpty()) {
    _retval = mMatches[aIndex].mValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetCommentAt(int32_t aIndex, nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mComment;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetImageAt(int32_t aIndex, nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mImage;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetStyleAt(int32_t aIndex, nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetFinalCompleteValueAt(int32_t aIndex,
                                                    nsAString& _retval)
{
  CHECK_MATCH_INDEX(aIndex, false);
  _retval = mMatches[aIndex].mFinalCompleteValue;
  if (_retval.IsEmpty()) {
    _retval = mMatches[aIndex].mValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetListener(nsIAutoCompleteSimpleResultListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::GetListener(nsIAutoCompleteSimpleResultListener** aListener)
{
  nsCOMPtr<nsIAutoCompleteSimpleResultListener> listener(mListener);
  listener.forget(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::RemoveValueAt(int32_t aRowIndex,
                                          bool aRemoveFromDb)
{
  CHECK_MATCH_INDEX(aRowIndex, false);

  nsString value = mMatches[aRowIndex].mValue;
  mMatches.RemoveElementAt(aRowIndex);

  if (mListener) {
    mListener->OnValueRemoved(this, value, aRemoveFromDb);
  }

  return NS_OK;
}
