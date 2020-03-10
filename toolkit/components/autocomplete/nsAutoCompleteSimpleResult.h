/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoCompleteSimpleResult__
#define __nsAutoCompleteSimpleResult__

#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSimpleResult.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

struct AutoCompleteSimpleResultMatch {
  AutoCompleteSimpleResultMatch(const nsAString& aValue,
                                const nsAString& aComment,
                                const nsAString& aImage,
                                const nsAString& aStyle,
                                const nsAString& aFinalCompleteValue,
                                const nsAString& aLabel)
      : mValue(aValue),
        mComment(aComment),
        mImage(aImage),
        mStyle(aStyle),
        mFinalCompleteValue(aFinalCompleteValue),
        mLabel(aLabel) {}

  nsString mValue;
  nsString mComment;
  nsString mImage;
  nsString mStyle;
  nsString mFinalCompleteValue;
  nsString mLabel;
};

class nsAutoCompleteSimpleResult final : public nsIAutoCompleteSimpleResult {
 public:
  nsAutoCompleteSimpleResult();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT
  NS_DECL_NSIAUTOCOMPLETESIMPLERESULT

  nsresult AppendResult(nsIAutoCompleteResult* aResult);

 private:
  ~nsAutoCompleteSimpleResult() = default;

 protected:
  typedef nsTArray<AutoCompleteSimpleResultMatch> MatchesArray;
  MatchesArray mMatches;

  nsString mSearchString;
  nsString mErrorDescription;
  int32_t mDefaultIndex;
  uint32_t mSearchResult;

  nsCOMPtr<nsIAutoCompleteSimpleResultListener> mListener;
};

#endif  // __nsAutoCompleteSimpleResult__
