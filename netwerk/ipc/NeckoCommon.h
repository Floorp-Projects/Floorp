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

namespace mozilla {
namespace dom {
class BrowserChild;
}  // namespace dom
}  // namespace mozilla

#if defined(DEBUG)
#  define NECKO_ERRORS_ARE_FATAL_DEFAULT true
#else
#  define NECKO_ERRORS_ARE_FATAL_DEFAULT false
#endif

// TODO: Eventually remove NECKO_MAYBE_ABORT and DROP_DEAD (bug 575494).
// Still useful for catching listener interfaces we don't yet support across
// processes, etc.

#define NECKO_MAYBE_ABORT(msg)                                  \
  do {                                                          \
    bool abort = NECKO_ERRORS_ARE_FATAL_DEFAULT;                \
    const char* e = PR_GetEnv("NECKO_ERRORS_ARE_FATAL");        \
    if (e) abort = (*e == '0') ? false : true;                  \
    if (abort) {                                                \
      msg.AppendLiteral(                                        \
          " (set NECKO_ERRORS_ARE_FATAL=0 in your environment " \
          "to convert this error into a warning.)");            \
      MOZ_CRASH_UNSAFE(msg.get());                              \
    } else {                                                    \
      msg.AppendLiteral(                                        \
          " (set NECKO_ERRORS_ARE_FATAL=1 in your environment " \
          "to convert this warning into a fatal error.)");      \
      NS_WARNING(msg.get());                                    \
    }                                                           \
  } while (0)

#define DROP_DEAD()                                                       \
  do {                                                                    \
    nsPrintfCString msg("NECKO ERROR: '%s' UNIMPLEMENTED", __FUNCTION__); \
    NECKO_MAYBE_ABORT(msg);                                               \
    return NS_ERROR_NOT_IMPLEMENTED;                                      \
  } while (0)

#define ENSURE_CALLED_BEFORE_ASYNC_OPEN()                                      \
  do {                                                                         \
    if (LoadIsPending() || LoadWasOpened()) {                                  \
      nsPrintfCString msg("'%s' called after AsyncOpen: %s +%d", __FUNCTION__, \
                          __FILE__, __LINE__);                                 \
      NECKO_MAYBE_ABORT(msg);                                                  \
    }                                                                          \
    NS_ENSURE_TRUE(!LoadIsPending(), NS_ERROR_IN_PROGRESS);                    \
    NS_ENSURE_TRUE(!LoadWasOpened(), NS_ERROR_ALREADY_OPENED);                 \
  } while (0)

// Fails call if made after request observers (on-modify-request, etc) have been
// called

#define ENSURE_CALLED_BEFORE_CONNECT()                                  \
  do {                                                                  \
    if (LoadRequestObserversCalled()) {                                 \
      nsPrintfCString msg("'%s' called too late: %s +%d", __FUNCTION__, \
                          __FILE__, __LINE__);                          \
      NECKO_MAYBE_ABORT(msg);                                           \
      if (LoadIsPending()) return NS_ERROR_IN_PROGRESS;                 \
      MOZ_ASSERT(LoadWasOpened());                                      \
      return NS_ERROR_ALREADY_OPENED;                                   \
    }                                                                   \
  } while (0)

namespace mozilla {
namespace net {

inline bool IsNeckoChild() {
  static bool didCheck = false;
  static bool amChild = false;

  if (!didCheck) {
    didCheck = true;
    amChild = (XRE_GetProcessType() == GeckoProcessType_Content);
  }
  return amChild;
}

inline bool IsSocketProcessChild() {
  static bool amChild = (XRE_GetProcessType() == GeckoProcessType_Socket);
  return amChild;
}

class HttpChannelSecurityWarningReporter : public nsISupports {
 public:
  [[nodiscard]] virtual nsresult ReportSecurityMessage(
      const nsAString& aMessageTag, const nsAString& aMessageCategory) = 0;
  [[nodiscard]] virtual nsresult LogBlockedCORSRequest(
      const nsAString& aMessage, const nsACString& aCategory) = 0;
  [[nodiscard]] virtual nsresult LogMimeTypeMismatch(
      const nsACString& aMessageName, bool aWarning, const nsAString& aURL,
      const nsAString& aContentType) = 0;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NeckoCommon_h
