/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Additional definitions and implementation for fuzzing code */

#ifndef mozilla_Fuzzing_h
#define mozilla_Fuzzing_h

#ifdef FUZZING_SNAPSHOT
#  include "mozilla/fuzzing/NyxWrapper.h"

#  ifdef __cplusplus
#    include "mozilla/fuzzing/Nyx.h"
#    include "mozilla/ScopeExit.h"

#    define MOZ_FUZZING_NYX_RELEASE(id)                       \
      if (mozilla::fuzzing::Nyx::instance().is_enabled(id)) { \
        mozilla::fuzzing::Nyx::instance().release();          \
      }

#    define MOZ_FUZZING_NYX_GUARD(id)                           \
      auto nyxGuard = mozilla::MakeScopeExit([&] {              \
        if (mozilla::fuzzing::Nyx::instance().is_enabled(id)) { \
          mozilla::fuzzing::Nyx::instance().release();          \
        }                                                       \
      });
#  endif

#  define MOZ_FUZZING_HANDLE_CRASH_EVENT2(aType, aReason)   \
    do {                                                    \
      nyx_handle_event(aType, __FILE__, __LINE__, aReason); \
    } while (false)

#  define MOZ_FUZZING_HANDLE_CRASH_EVENT4(aType, aFilename, aLine, aReason) \
    do {                                                                    \
      nyx_handle_event(aType, aFilename, aLine, aReason);                   \
    } while (false)

#else
#  define MOZ_FUZZING_NYX_RELEASE(id)
#  define MOZ_FUZZING_NYX_GUARD(id)
#  define MOZ_FUZZING_HANDLE_CRASH_EVENT2(aType, aReason) \
    do {                                                  \
    } while (false)
#  define MOZ_FUZZING_HANDLE_CRASH_EVENT4(aType, aFilename, aLine, aReason) \
    do {                                                                    \
    } while (false)
#endif

#endif /* mozilla_Fuzzing_h */
