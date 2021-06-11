/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventListener.h"

#include "nsThreadManager.h"  // NS_IsMainThread

namespace mozilla {
namespace extensions {

NS_IMETHODIMP ExtensionEventListener::CallListener(
    const nsTArray<JS::Value>& aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace extensions
}  // namespace mozilla
