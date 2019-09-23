/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ModuleEvaluator_h
#define mozilla_ModuleEvaluator_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/UntrustedModulesData.h"
#include "mozilla/Vector.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"

namespace mozilla {

class ModuleRecord;

/**
 * This class performs trustworthiness evaluation for incoming DLLs.
 */
class MOZ_RAII ModuleEvaluator final {
 public:
  ModuleEvaluator();

  explicit operator bool() const;

  Maybe<ModuleTrustFlags> GetTrust(const ModuleRecord& aModuleRecord) const;

 private:
  static bool ResolveKnownFolder(REFKNOWNFOLDERID aFolderId,
                                 nsIFile** aOutFile);

 private:
  Maybe<ModuleVersion> mExeVersion;  // Version number of the running EXE image
  nsCOMPtr<nsIFile> mExeDirectory;
  nsCOMPtr<nsIFile> mSysDirectory;
  nsCOMPtr<nsIFile> mWinSxSDirectory;
  Vector<nsString> mKeyboardLayoutDlls;
};

}  // namespace mozilla

#endif  // mozilla_ModuleEvaluator_h
