/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef replace_malloc_bridge_h
#define replace_malloc_bridge_h

/*
 * The replace-malloc bridge allows bidirectional method calls between
 * a program and the replace-malloc library that has been loaded for it.
 * In Firefox, this is used to allow method calls between code in libxul
 * and code in the replace-malloc library, without libxul needing to link
 * against that library or vice-versa.
 *
 * Subsystems can add methods for their own need. Replace-malloc libraries
 * can decide to implement those methods or not.
 *
 * Replace-malloc libraries can provide such a bridge by implementing
 * a ReplaceMallocBridge-derived class, and a replace_get_bridge function
 * returning an instance of that class. The default methods in
 * ReplaceMallocBridge are expected to return values that callers would
 * understand as "the bridge doesn't implement this method", so that a
 * replace-malloc library doesn't have to implement all methods.
 *
 * The ReplaceMallocBridge class contains definitions for methods for
 * all replace-malloc libraries. Each library picks the methods it wants
 * to reply to in its ReplaceMallocBridge-derived class instance.
 * All methods of ReplaceMallocBridge must be virtual. Similarly,
 * anything passed as an argument to those methods must be plain data, or
 * an instance of a class with only virtual methods.
 *
 * Binary compatibility is expected to be maintained, such that a newer
 * Firefox can be used with an old replace-malloc library, or an old
 * Firefox can be used with a newer replace-malloc library. As such, only
 * new virtual methods should be added to ReplaceMallocBridge, and
 * each change should have a corresponding bump of the mVersion value.
 * At the same time, each virtual method should have a corresponding
 * wrapper calling the virtual method on the instance from
 * ReplaceMallocBridge::Get(), giving it the version the virtual method
 * was added.
 *
 * Parts that are not relevant to the replace-malloc library end of the
 * bridge are hidden when REPLACE_MALLOC_IMPL is not defined, which is
 * the case when including replace_malloc.h.
 */

struct ReplaceMallocBridge;

#ifdef __cplusplus

#include "mozilla/Types.h"

#ifndef REPLACE_MALLOC_IMPL
/* Returns the replace-malloc bridge if there is one to be returned. */
extern "C" MFBT_API ReplaceMallocBridge* get_bridge();
#endif

namespace mozilla {
namespace dmd {
struct DMDFuncs;
}

/* Callbacks to register debug file handles for Poison IO interpose.
 * See Mozilla(|Un)RegisterDebugHandle in xpcom/build/PoisonIOInterposer.h */
struct DebugFdRegistry
{
  virtual void RegisterHandle(intptr_t aFd);

  virtual void UnRegisterHandle(intptr_t aFd);
};

} // namespace mozilla

struct ReplaceMallocBridge
{
  ReplaceMallocBridge() : mVersion(2) {}

  /* This method was added in version 1 of the bridge. */
  virtual mozilla::dmd::DMDFuncs* GetDMDFuncs() { return nullptr; }

  /* Send a DebugFdRegistry instance to the replace-malloc library so that
   * it can register/unregister file descriptors whenever needed. The
   * instance is valid until the process dies.
   * This method was added in version 2 of the bridge. */
  virtual void InitDebugFd(mozilla::DebugFdRegistry&) {}

#ifndef REPLACE_MALLOC_IMPL
  /* Returns the replace-malloc bridge if its version is at least the
   * requested one. */
  static ReplaceMallocBridge* Get(int aMinimumVersion) {
    static ReplaceMallocBridge* sSingleton = get_bridge();
    return (sSingleton && sSingleton->mVersion >= aMinimumVersion)
      ? sSingleton : nullptr;
  }
#endif

protected:
  const int mVersion;
};

#ifndef REPLACE_MALLOC_IMPL
/* Class containing wrappers for calls to ReplaceMallocBridge methods.
 * Those wrappers need to be static methods in a class because compilers
 * complain about unused static global functions, and linkers complain
 * about multiple definitions of non-static global functions.
 * Using a separate class from ReplaceMallocBridge allows the function
 * names to be identical. */
struct ReplaceMalloc
{
  /* Don't call this method from performance critical code. Use
   * mozilla::dmd::DMDFuncs::Get() instead, it has less overhead. */
  static mozilla::dmd::DMDFuncs* GetDMDFuncs()
  {
    auto singleton = ReplaceMallocBridge::Get(/* minimumVersion */ 1);
    return singleton ? singleton->GetDMDFuncs() : nullptr;
  }

  static void InitDebugFd(mozilla::DebugFdRegistry& aRegistry)
  {
    auto singleton = ReplaceMallocBridge::Get(/* minimumVersion */ 2);
    if (singleton) {
      singleton->InitDebugFd(aRegistry);
    }
  }
};
#endif

#endif /* __cplusplus */

#endif /* replace_malloc_bridge_h */
