/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoCompleteSimpleResult__
#define __nsAutoCompleteSimpleResult__

#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSimpleResult.h"

#include "nsString.h"
#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsAutoCompleteSimpleResult MOZ_FINAL : public nsIAutoCompleteSimpleResult
{
public:
  nsAutoCompleteSimpleResult();
  inline void CheckInvariants() {
    NS_ASSERTION(mValues.Length() == mComments.Length(), "Arrays out of sync");
    NS_ASSERTION(mValues.Length() == mImages.Length(),   "Arrays out of sync");
    NS_ASSERTION(mValues.Length() == mStyles.Length(),   "Arrays out of sync");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT
  NS_DECL_NSIAUTOCOMPLETESIMPLERESULT

private:
  ~nsAutoCompleteSimpleResult() {}

protected:

  // What we really want is an array of structs with value/comment/image/style contents.
  // But then we'd either have to use COM or manage object lifetimes ourselves.
  // Having four arrays of string simplifies this, but is stupid.
  nsTArray<nsString> mValues;
  nsTArray<nsString> mComments;
  nsTArray<nsString> mImages;
  nsTArray<nsString> mStyles;

  nsString mSearchString;
  nsString mErrorDescription;
  PRInt32 mDefaultIndex;
  PRUint32 mSearchResult;

  bool mTypeAheadResult;

  nsCOMPtr<nsIAutoCompleteSimpleResultListener> mListener;
};

#endif // __nsAutoCompleteSimpleResult__
