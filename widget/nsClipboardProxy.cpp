/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "nsArrayUtils.h"
#include "nsClipboardProxy.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "PermissionMessageUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsClipboardProxy, nsIClipboard, nsIClipboardProxy)

nsClipboardProxy::nsClipboardProxy() : mClipboardCaps(false, false) {}

NS_IMETHODIMP
nsClipboardProxy::SetData(nsITransferable* aTransferable,
                          nsIClipboardOwner* anOwner, int32_t aWhichClipboard) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  ContentChild* child = ContentChild::GetSingleton();

  IPCDataTransfer ipcDataTransfer;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcDataTransfer,
                                                false, child, nullptr);

  bool isPrivateData = aTransferable->GetIsPrivateData();
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
      aTransferable->GetRequestingPrincipal();
  nsContentPolicyType contentPolicyType = aTransferable->GetContentPolicyType();
  child->SendSetClipboard(ipcDataTransfer, isPrivateData,
                          IPC::Principal(requestingPrincipal),
                          contentPolicyType, aWhichClipboard);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable* aTransferable,
                          int32_t aWhichClipboard) {
  nsTArray<nsCString> types;
  aTransferable->FlavorsTransferableCanImport(types);

  IPCDataTransfer dataTransfer;
  ContentChild::GetSingleton()->SendGetClipboard(types, aWhichClipboard,
                                                 &dataTransfer);
  return nsContentUtils::IPCTransferableToTransferable(
      dataTransfer, false /* aAddDataFlavor */, aTransferable,
      ContentChild::GetSingleton());
}

NS_IMETHODIMP
nsClipboardProxy::EmptyClipboard(int32_t aWhichClipboard) {
  ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                         int32_t aWhichClipboard,
                                         bool* aHasType) {
  *aHasType = false;

  ContentChild::GetSingleton()->SendClipboardHasType(aFlavorList,
                                                     aWhichClipboard, aHasType);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::SupportsSelectionClipboard(bool* aIsSupported) {
  *aIsSupported = mClipboardCaps.supportsSelectionClipboard();
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::SupportsFindClipboard(bool* aIsSupported) {
  *aIsSupported = mClipboardCaps.supportsFindClipboard();
  return NS_OK;
}

void nsClipboardProxy::SetCapabilities(
    const ClipboardCapabilities& aClipboardCaps) {
  mClipboardCaps = aClipboardCaps;
}
