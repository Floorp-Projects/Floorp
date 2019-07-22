/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file does not make sense on its own. It must be #included along with
// StaticPrefsListEnd.h in all headers that contribute prefs to the StaticPrefs
// namespace.

#include "StaticPrefsBase.h"
#include "MainThreadUtils.h"  // for NS_IsMainThread()

namespace mozilla {
namespace StaticPrefs {

// For a VarCache pref like this:
//
//   VARCACHE_PREF($POLICY, "my.pref", my_pref, my_pref, int32_t, 99)
//
// we generate an extern variable declaration and three getter
// declarations/definitions.
//
//     extern int32_t sVarCache_my_pref;
//     inline int32_t my_pref() {
//       if (UpdatePolicy::$POLICY != UpdatePolicy::Once) {
//         return sVarCache_my_pref;
//       }
//       MaybeInitOncePrefs();
//       return sVarCache_my_pref();
//     }
//     inline const char* GetPrefName_my_pref() { return "my.pref"; }
//     inline int32_t GetPrefDefault_my_pref() { return 99; }
//
// The extern declaration of the variable is necessary for bindgen to see it
// and generate Rust bindings.
//
#define PREF(name, cpp_type, default_value)
#define VARCACHE_PREF(policy, name, base_id, full_id, cpp_type, default_value) \
  extern cpp_type sVarCache_##full_id;                                         \
  inline StripAtomic<cpp_type> full_id() {                                     \
    if (UpdatePolicy::policy != UpdatePolicy::Once) {                          \
      MOZ_DIAGNOSTIC_ASSERT(                                                   \
          IsAtomic<cpp_type>::value || NS_IsMainThread(),                      \
          "Non-atomic static pref '" name                                      \
          "' being accessed on background thread by getter");                  \
      return sVarCache_##full_id;                                              \
    }                                                                          \
    MaybeInitOncePrefs();                                                      \
    return sVarCache_##full_id;                                                \
  }                                                                            \
  inline const char* GetPrefName_##base_id() { return name; }                  \
  inline StripAtomic<cpp_type> GetPrefDefault_##base_id() {                    \
    return default_value;                                                      \
  }
