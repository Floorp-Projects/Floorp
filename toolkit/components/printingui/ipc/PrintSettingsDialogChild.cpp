/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintSettingsDialogChild.h"

using mozilla::Unused;

namespace mozilla {
namespace embedding {

PrintSettingsDialogChild::PrintSettingsDialogChild() : mReturned(false) {
  MOZ_COUNT_CTOR(PrintSettingsDialogChild);
}

PrintSettingsDialogChild::~PrintSettingsDialogChild() {
  MOZ_COUNT_DTOR(PrintSettingsDialogChild);
}

mozilla::ipc::IPCResult PrintSettingsDialogChild::Recv__delete__(
    const PrintDataOrNSResult& aData) {
  if (aData.type() == PrintDataOrNSResult::Tnsresult) {
    mResult = aData.get_nsresult();
    MOZ_ASSERT(NS_FAILED(mResult), "expected a failure result");
  } else {
    mResult = NS_OK;
    mData = aData.get_PrintData();
  }
  mReturned = true;
  return IPC_OK();
}

}  // namespace embedding
}  // namespace mozilla
