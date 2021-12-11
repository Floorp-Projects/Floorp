/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PoisonIOInterposer_h
#define mozilla_PoisonIOInterposer_h

#include "mozilla/Types.h"
#include <stdio.h>

MOZ_BEGIN_EXTERN_C

/** Register file handle to be ignored by poisoning IO interposer. This function
 * and the corresponding UnRegister function are necessary for exchange of
 * handles between binaries not using the same CRT on Windows (which happens
 * when one of them links the static CRT). In such cases, giving file
 * descriptors or FILEs
 * doesn't work because _get_osfhandle fails with "invalid parameter". */
void MozillaRegisterDebugHandle(intptr_t aHandle);

/** Register file descriptor to be ignored by poisoning IO interposer */
void MozillaRegisterDebugFD(int aFd);

/** Register file to be ignored by poisoning IO interposer */
void MozillaRegisterDebugFILE(FILE* aFile);

/** Unregister file handle from being ignored by poisoning IO interposer */
void MozillaUnRegisterDebugHandle(intptr_t aHandle);

/** Unregister file descriptor from being ignored by poisoning IO interposer */
void MozillaUnRegisterDebugFD(int aFd);

/** Unregister file from being ignored by poisoning IO interposer */
void MozillaUnRegisterDebugFILE(FILE* aFile);

MOZ_END_EXTERN_C

#if defined(XP_MACOSX) || defined(XP_WIN)

#  ifdef __cplusplus
namespace mozilla {

/**
 * Check if a file is registered as a debug file.
 */
bool IsDebugFile(intptr_t aFileID);

/**
 * Initialize IO poisoning, this is only safe to do on the main-thread when no
 * other threads are running.
 *
 * Please, note that this probably has performance implications as all
 */
void InitPoisonIOInterposer();

#    ifdef XP_MACOSX
/**
 * Check that writes are dirty before reporting I/O (Mac OS X only)
 * This is necessary for late-write checks on Mac OS X, but reading the buffer
 * from file to see if we're writing dirty bits is expensive, so we don't want
 * to do this for everything else that uses
 */
void OnlyReportDirtyWrites();
#    endif /* XP_MACOSX */

/**
 * Clear IO poisoning, this is only safe to do on the main-thread when no other
 * threads are running.
 * Never called! See bug 1647107.
 */
void ClearPoisonIOInterposer();

}  // namespace mozilla
#  endif /* __cplusplus */

#else /* defined(XP_MACOSX) || defined(XP_WIN) */

#  ifdef __cplusplus
namespace mozilla {
inline bool IsDebugFile(intptr_t aFileID) { return true; }
inline void InitPoisonIOInterposer() {}
inline void ClearPoisonIOInterposer() {}
#    ifdef XP_MACOSX
inline void OnlyReportDirtyWrites() {}
#    endif /* XP_MACOSX */
}  // namespace mozilla
#  endif   /* __cplusplus */

#endif /* XP_WIN || XP_MACOSX */

#endif  // mozilla_PoisonIOInterposer_h
