/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMPROCESSCHILD_H_
#define PDFIUMPROCESSCHILD_H_

#include "mozilla/ipc/ProcessChild.h"
#include "PDFiumChild.h"

namespace mozilla {
namespace widget {

/**
 * Contains the PDFiumChild object that facilitates IPC communication to/from
 * the instance of the PDFium library that is run in this process.
 */
class PDFiumProcessChild final : public mozilla::ipc::ProcessChild
{
protected:
  typedef mozilla::ipc::ProcessChild ProcessChild;

public:
  explicit PDFiumProcessChild(ProcessId aParentPid);
  ~PDFiumProcessChild();

  // ProcessChild functions.
  bool Init(int aArgc, char* aArgv[]) override;
  void CleanUp() override;

private:
  DISALLOW_COPY_AND_ASSIGN(PDFiumProcessChild);

  PDFiumChild mPDFiumActor;
#if defined(MOZ_SANDBOX)
  PRLibrary*  mPDFium;
#endif
};

} // namespace widget
} // namespace mozilla

#endif // PDFIUMPROCESSCHILD_H_
