/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cert_storage.h"

nsresult construct_cert_storage(nsISupports* outer, REFNSIID iid,
                                void** result) {
  // Forward to the main thread synchronously.
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozilla::SyncRunnable::DispatchToThread(
      mainThread, new mozilla::SyncRunnable(
                      NS_NewRunnableFunction("psm::Constructor", [&]() {
                        rv = cert_storage_constructor(outer, iid, result);
                      })));
  return rv;
}