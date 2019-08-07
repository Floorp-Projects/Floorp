/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsSoundProxy.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsSoundProxy, nsISound)

NS_IMETHODIMP
nsSoundProxy::Play(nsIURL* aURL) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Content);

  nsCOMPtr<nsIURI> soundURI(aURL);
  bool isChrome = false;
  // Only allow playing a chrome:// URL from the content process.
  if (!soundURI || NS_FAILED(soundURI->SchemeIs("chrome", &isChrome)) ||
      !isChrome) {
    return NS_ERROR_FAILURE;
  }

  mozilla::ipc::URIParams soundParams;
  mozilla::ipc::SerializeURI(soundURI, soundParams);
  ContentChild::GetSingleton()->SendPlaySound(soundParams);
  return NS_OK;
}

NS_IMETHODIMP
nsSoundProxy::Beep() {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Content);

  ContentChild::GetSingleton()->SendBeep();
  return NS_OK;
}

NS_IMETHODIMP
nsSoundProxy::Init() {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Content);
  MOZ_DIAGNOSTIC_ASSERT(false, "Only called by XUL in the parent process.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSoundProxy::PlayEventSound(uint32_t aEventId) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Content);

  ContentChild::GetSingleton()->SendPlayEventSound(aEventId);
  return NS_OK;
}
