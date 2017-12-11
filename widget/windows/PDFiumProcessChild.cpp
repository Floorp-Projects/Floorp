/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumProcessChild.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#if defined(MOZ_SANDBOX)
#include "mozilla/sandboxTarget.h"
#endif

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace widget {

PDFiumProcessChild::PDFiumProcessChild(ProcessId aParentPid)
  : ProcessChild(aParentPid)
#if defined(MOZ_SANDBOX)
  , mPDFium(nullptr)
#endif
{
}

PDFiumProcessChild::~PDFiumProcessChild()
{
#if defined(MOZ_SANDBOX)
  if (mPDFium) {
    PR_UnloadLibrary(mPDFium);
  }
#endif
}

bool
PDFiumProcessChild::Init(int aArgc, char* aArgv[])
{
  BackgroundHangMonitor::Startup();

#if defined(MOZ_SANDBOX)
  // XXX bug 1417000
  // We really should load "pdfium.dll" after calling StartSandbox(). For
  // an unknown reason, "pdfium.dll" can not be loaded correctly after
  // StartSandbox() been called. Temporary preload this library until we fix
  // bug 1417000.
  mPDFium = PR_LoadLibrary("pdfium.dll");
  mozilla::SandboxTarget::Instance()->StartSandbox();
#endif

  mPDFiumActor.Init(ParentPid(),IOThreadChild::message_loop(),
                    IOThreadChild::channel());

  return true;
}

void
PDFiumProcessChild::CleanUp()
{
  BackgroundHangMonitor::Shutdown();
}

} // namespace widget
} // namespace mozilla
