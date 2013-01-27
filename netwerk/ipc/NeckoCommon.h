/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NeckoCommon_h
#define mozilla_net_NeckoCommon_h

#include "nsXULAppAPI.h"
#include "prenv.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"

namespace mozilla { namespace dom {
class TabChild;
}}

#if defined(DEBUG) || defined(ENABLE_TESTS)
# define NECKO_ERRORS_ARE_FATAL_DEFAULT true
#else
# define NECKO_ERRORS_ARE_FATAL_DEFAULT false
#endif 

// TODO: Eventually remove NECKO_MAYBE_ABORT and DROP_DEAD (bug 575494).
// Still useful for catching listener interfaces we don't yet support across
// processes, etc.

#define NECKO_MAYBE_ABORT(msg)                                                 \
  do {                                                                         \
    bool abort = NECKO_ERRORS_ARE_FATAL_DEFAULT;                               \
    const char *e = PR_GetEnv("NECKO_ERRORS_ARE_FATAL");                       \
    if (e)                                                                     \
      abort = (*e == '0') ? false : true;                                      \
    if (abort) {                                                               \
      msg.Append(" (set NECKO_ERRORS_ARE_FATAL=0 in your environment to "      \
                      "convert this error into a warning.)");                  \
      NS_RUNTIMEABORT(msg.get());                                              \
    } else {                                                                   \
      msg.Append(" (set NECKO_ERRORS_ARE_FATAL=1 in your environment to "      \
                      "convert this warning into a fatal error.)");            \
      NS_WARNING(msg.get());                                                   \
    }                                                                          \
  } while (0)

#define DROP_DEAD()                                                            \
  do {                                                                         \
    nsPrintfCString msg("NECKO ERROR: '%s' UNIMPLEMENTED",                     \
                        __FUNCTION__);                                         \
    NECKO_MAYBE_ABORT(msg);                                                    \
    return NS_ERROR_NOT_IMPLEMENTED;                                           \
  } while (0)

#define ENSURE_CALLED_BEFORE_ASYNC_OPEN()                                      \
  do {                                                                         \
    if (mIsPending || mWasOpened) {                                            \
      nsPrintfCString msg("'%s' called after AsyncOpen: %s +%d",               \
                          __FUNCTION__, __FILE__, __LINE__);                   \
      NECKO_MAYBE_ABORT(msg);                                                  \
    }                                                                          \
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);                         \
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);                      \
  } while (0)

// Fails call if made after request observers (on-modify-request, etc) have been
// called

#define ENSURE_CALLED_BEFORE_CONNECT()                                         \
  do {                                                                         \
    if (mRequestObserversCalled) {                                             \
      nsPrintfCString msg("'%s' called too late: %s +%d",                      \
                          __FUNCTION__, __FILE__, __LINE__);                   \
      NECKO_MAYBE_ABORT(msg);                                                  \
      if (mIsPending)                                                          \
        return NS_ERROR_IN_PROGRESS;                                           \
      MOZ_ASSERT(mWasOpened);                                                  \
      return NS_ERROR_ALREADY_OPENED;                                          \
    }                                                                          \
  } while (0)

namespace mozilla {
namespace net {

inline bool 
IsNeckoChild() 
{
  static bool didCheck = false;
  static bool amChild = false;

  if (!didCheck) {
    // This allows independent necko-stacks (instead of single stack in chrome)
    // to still be run.  
    // TODO: Remove eventually when no longer supported (bug 571126)
    const char * e = PR_GetEnv("NECKO_SEPARATE_STACKS");
    if (!e) 
      amChild = (XRE_GetProcessType() == GeckoProcessType_Content);
    didCheck = true;
  }
  return amChild;
}

namespace NeckoCommonInternal {
  extern bool gSecurityDisabled;
  extern bool gRegisteredBool;
}

// This should always return true unless xpcshell tests are being used
inline bool
UsingNeckoIPCSecurity()
{

  if (!NeckoCommonInternal::gRegisteredBool) {
    Preferences::AddBoolVarCache(&NeckoCommonInternal::gSecurityDisabled,
                                 "network.disable.ipc.security");
    NeckoCommonInternal::gRegisteredBool = true;
  }
  return !NeckoCommonInternal::gSecurityDisabled;
}

inline bool
MissingRequiredTabChild(mozilla::dom::TabChild* tabChild,
                        const char* context)
{
  if (UsingNeckoIPCSecurity()) {
    // Bug 833935: during navigation away from page some loads may lack
    // TabParent: we don't want to kill browser for that.  Doesn't happen in
    // test harness, so fail in debug mode so we can catch new code that fails
    // to pass security info.
    MOZ_ASSERT(tabChild);

    if (!tabChild) {
      printf_stderr("WARNING: child tried to open %s IPDL channel w/o "
                    "security info\n", context);
      return true;
    }
  }
  return false;
}


} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoCommon_h

