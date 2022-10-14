/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacUtilsImpl_h___
#define nsMacUtilsImpl_h___

#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"

using mozilla::Atomic;
using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;

class nsIFile;

namespace nsMacUtilsImpl {

// Return the repo directory and the repo object directory respectively.
// These should only be used on Mac developer builds to determine the path
// to the repo or object directory.
nsresult GetRepoDir(nsIFile** aRepoDir);
nsresult GetObjDir(nsIFile** aObjDir);

#if defined(MOZ_SANDBOX) || defined(__aarch64__)
bool GetAppPath(nsCString& aAppPath);
#endif /* MOZ_SANDBOX || __aarch64__ */

#if defined(MOZ_SANDBOX) && defined(DEBUG)
nsresult GetBloatLogDir(nsCString& aDirectoryPath);
nsresult GetDirectoryPath(const char* aPath, nsCString& aDirectoryPath);
#endif /* MOZ_SANDBOX && DEBUG */

void EnableTCSMIfAvailable();
bool IsTCSMAvailable();
uint32_t GetPhysicalCPUCount();
nsresult GetArchitecturesForBundle(uint32_t* aArchMask);
nsresult GetArchitecturesForBinary(const char* aPath, uint32_t* aArchMask);

#if defined(__aarch64__)
// Pre-translate binaries to avoid translation delays when launching
// x64 child process instances for the first time. i.e. on first launch
// after installation or after an update. Translations are cached so
// repeated launches of the binaries do not encounter delays.
int PreTranslateXUL();
int PreTranslateBinary(nsCString aBinaryPath);
#endif
}  // namespace nsMacUtilsImpl

#endif /* nsMacUtilsImpl_h___ */
