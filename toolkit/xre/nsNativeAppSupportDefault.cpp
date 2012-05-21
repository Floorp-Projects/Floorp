/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeAppSupportBase.h"

nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult )
{
  nsNativeAppSupportBase* native = new nsNativeAppSupportBase();
  if (!native) return NS_ERROR_OUT_OF_MEMORY;

  *aResult = native;
  NS_ADDREF( *aResult );

  return NS_OK;
}
