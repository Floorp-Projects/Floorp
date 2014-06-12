/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_INFORMATION_H_
#define _WEBRTC_GLOBAL_INFORMATION_H_

#include "nsString.h"
#include "mozilla/dom/BindingDeclarations.h" // for Optional

namespace sipcc {
class PeerConnectionImpl;
}

namespace mozilla {
class ErrorResult;

namespace dom {
class GlobalObject;
class WebrtcGlobalStatisticsCallback;
class WebrtcGlobalLoggingCallback;

class WebrtcGlobalInformation
{
public:
  static void GetAllStats(const GlobalObject& aGlobal,
                          WebrtcGlobalStatisticsCallback& aStatsCallback,
                          const Optional<nsAString>& pcIdFilter,
                          ErrorResult& aRv);

  static void GetLogging(const GlobalObject& aGlobal,
                         const nsAString& aPattern,
                         WebrtcGlobalLoggingCallback& aLoggingCallback,
                         ErrorResult& aRv);

  static void SetDebugLevel(const GlobalObject& aGlobal, int32_t aLevel);
  static int32_t DebugLevel(const GlobalObject& aGlobal);

  static void SetAecDebug(const GlobalObject& aGlobal, bool aEnable);
  static bool AecDebug(const GlobalObject& aGlobal);

  static void StoreLongTermICEStatistics(sipcc::PeerConnectionImpl& aPc);

private:
  WebrtcGlobalInformation() MOZ_DELETE;
  WebrtcGlobalInformation(const WebrtcGlobalInformation& aOrig) MOZ_DELETE;
  WebrtcGlobalInformation& operator=(
    const WebrtcGlobalInformation& aRhs) MOZ_DELETE;
};

} // namespace dom
} // namespace mozilla

#endif  // _WEBRTC_GLOBAL_INFORMATION_H_

