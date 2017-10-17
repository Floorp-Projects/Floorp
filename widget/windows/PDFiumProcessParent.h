/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMPROCESSPARENT_H
#define PDFIUMPROCESSPARENT_H

#include "chrome/common/child_process_host.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

class nsIRunnable;
class nsDeviceContextSpecWin;

#ifdef MOZ_ENABLE_SKIA_PDF
namespace mozilla {
namespace widget {
class PDFiumParent;
}
}
#endif

namespace mozilla {
namespace widget {

class PDFiumProcessParent final : public mozilla::ipc::GeckoChildProcessHost
{
public:
  PDFiumProcessParent();
  ~PDFiumProcessParent();

  bool Launch();

  void Delete();

  bool CanShutdown() override { return true; }

private:

  DISALLOW_COPY_AND_ASSIGN(PDFiumProcessParent);

  RefPtr<PDFiumParent> mPDFiumParentActor;
  nsCOMPtr<nsIThread> mLaunchThread;
};

} // namespace widget
} // namespace mozilla

#endif // ifndef PDFIUMPROCESSPARENT_H
