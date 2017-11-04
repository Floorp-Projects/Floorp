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
namespace gfx {
  class PrintTargetEMF;
}
}
#endif

namespace mozilla {
namespace widget {

class PDFiumProcessParent final : public mozilla::ipc::GeckoChildProcessHost
{
public:
  typedef mozilla::gfx::PrintTargetEMF PrintTargetEMF;

  PDFiumProcessParent();
  ~PDFiumProcessParent();

  bool Launch(PrintTargetEMF* aTarget);

  void Delete();

  bool CanShutdown() override { return true; }

  PDFiumParent* GetActor() const { return mPDFiumParentActor; }
private:

  DISALLOW_COPY_AND_ASSIGN(PDFiumProcessParent);

  RefPtr<PDFiumParent> mPDFiumParentActor;
  nsCOMPtr<nsIThread> mLaunchThread;
};

} // namespace widget
} // namespace mozilla

#endif // ifndef PDFIUMPROCESSPARENT_H
