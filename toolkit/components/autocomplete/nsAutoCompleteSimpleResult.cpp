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
                                const nsAString& aFinalCompleteValue)
    : mValue(aValue)
    , mComment(aComment)
    , mImage(aImage)
    , mStyle(aStyle)
    , mFinalCompleteValue(aFinalCompleteValue)
  {
  }

  nsString mValue;
  nsString mComment;
  nsString mImage;
  nsString mStyle;
  nsString mFinalCompleteValue;
};

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
                                          const nsAString& aFinalCompleteValue)
{
  CHECK_MATCH_INDEX(aIndex, true);

  AutoCompleteSimpleResultMatch match(aValue, aComment, aImage, aStyle, aFinalCompleteValue);

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
                                        const nsAString& aFinalCompleteValue)
{
  return InsertMatchAt(mMatches.Length(), aValue, aComment, aImage, aStyle,
                       aFinalCompleteValue);
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
  return GetValueAt(aIndex, _retval);
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
  if (_retval.Length() == 0)
    _retval = mMatches[aIndex].mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::SetListener(nsIAutoCompleteSimpleResultListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteSimpleResult::RemoveValueAt(int32_t aRowIndex,
                                          bool aRemoveFromDb)
{
  CHECK_MATCH_INDEX(aRowIndex, false);

  nsString value = mMatches[aRowIndex].mValue;
  mMatches.RemoveElementAt(aRowIndex);

  if (mListener)
    mListener->OnValueRemoved(this, value, aRemoveFromDb);

  return NS_OK;
}
