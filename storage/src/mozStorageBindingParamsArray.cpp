/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "mozStorageBindingParamsArray.h"
#include "mozStorageBindingParams.h"
#include "StorageBaseStatementInternal.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// BindingParamsArray

BindingParamsArray::BindingParamsArray(
  StorageBaseStatementInternal *aOwningStatement
)
: mOwningStatement(aOwningStatement)
, mLocked(false)
{
}

void
BindingParamsArray::lock()
{
  NS_ASSERTION(mLocked == false, "Array has already been locked!");
  mLocked = true;

  // We also no longer need to hold a reference to our statement since it owns
  // us.
  mOwningStatement = nsnull;
}

const StorageBaseStatementInternal *
BindingParamsArray::getOwner() const
{
  return mOwningStatement;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
  BindingParamsArray,
  mozIStorageBindingParamsArray
)

///////////////////////////////////////////////////////////////////////////////
//// mozIStorageBindingParamsArray

NS_IMETHODIMP
BindingParamsArray::NewBindingParams(mozIStorageBindingParams **_params)
{
  NS_ENSURE_FALSE(mLocked, NS_ERROR_UNEXPECTED);

  nsCOMPtr<mozIStorageBindingParams> params(
    mOwningStatement->newBindingParams(this));
  NS_ENSURE_TRUE(params, NS_ERROR_UNEXPECTED);

  params.forget(_params);
  return NS_OK;
}

NS_IMETHODIMP
BindingParamsArray::AddParams(mozIStorageBindingParams *aParameters)
{
  NS_ENSURE_FALSE(mLocked, NS_ERROR_UNEXPECTED);

  BindingParams *params = static_cast<BindingParams *>(aParameters);

  // Check to make sure that this set of parameters was created with us.
  if (params->getOwner() != this)
    return NS_ERROR_UNEXPECTED;

  NS_ENSURE_TRUE(mArray.AppendElement(params), NS_ERROR_OUT_OF_MEMORY);

  // Lock the parameters only after we've successfully added them.
  params->lock();

  return NS_OK;
}

NS_IMETHODIMP
BindingParamsArray::GetLength(PRUint32 *_length)
{
  *_length = length();
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
