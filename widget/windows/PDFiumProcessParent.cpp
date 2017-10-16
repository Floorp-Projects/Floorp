/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumProcessParent.h"
#include "nsIRunnable.h"
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "WinUtils.h"
#endif
#include "nsDeviceContextSpecWin.h"
#include "PDFiumParent.h"

using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {
namespace widget {

PDFiumProcessParent::PDFiumProcessParent()
  : GeckoChildProcessHost(GeckoProcessType_PDFium)
{
  MOZ_COUNT_CTOR(PDFiumProcessParent);
}

PDFiumProcessParent::~PDFiumProcessParent()
{
  MOZ_COUNT_DTOR(PDFiumProcessParent);
}

bool
PDFiumProcessParent::Launch()
{
  return SyncLaunch();
}

void
PDFiumProcessParent::Delete()
{
}

} // namespace widget
} // namespace mozilla
