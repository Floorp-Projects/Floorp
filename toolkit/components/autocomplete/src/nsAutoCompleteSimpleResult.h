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

#ifndef __nsAutoCompleteSimpleResult__
#define __nsAutoCompleteSimpleResult__

#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSimpleResult.h"

#include "nsVoidArray.h"
#include "nsString.h"
#include "prtypes.h"
#include "nsCOMPtr.h"

class nsAutoCompleteSimpleResult : public nsIAutoCompleteSimpleResult
{
public:
  nsAutoCompleteSimpleResult();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT
  NS_DECL_NSIAUTOCOMPLETESIMPLERESULT

private:
  ~nsAutoCompleteSimpleResult() {}

protected:

  // What we really want is an array of structs with value/comment/image/style contents.
  // But then we'd either have to use COM or manage object lifetimes ourselves.
  // Having four arrays of string simplifies this, but is stupid.
  nsStringArray mValues;
  nsStringArray mComments;
  nsStringArray mImages;
  nsStringArray mStyles;

  nsString mSearchString;
  nsString mErrorDescription;
  PRInt32 mDefaultIndex;
  PRUint32 mSearchResult;

  nsCOMPtr<nsIAutoCompleteSimpleResultListener> mListener;
};

#endif // __nsAutoCompleteSimpleResult__
