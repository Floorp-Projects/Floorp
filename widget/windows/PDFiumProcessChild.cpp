/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumProcessChild.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace widget {

PDFiumProcessChild::PDFiumProcessChild(ProcessId aParentPid)
  : ProcessChild(aParentPid)
{
}

PDFiumProcessChild::~PDFiumProcessChild()
{
}

bool
PDFiumProcessChild::Init(int aArgc, char* aArgv[])
{
  BackgroundHangMonitor::Startup();
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
