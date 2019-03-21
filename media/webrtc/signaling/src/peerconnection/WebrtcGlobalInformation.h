/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_INFORMATION_H_
#define _WEBRTC_GLOBAL_INFORMATION_H_

#include "nsString.h"
#include "mozilla/dom/BindingDeclarations.h"  // for Optional

namespace mozilla {
class PeerConnectionImpl;
class ErrorResult;

namespace dom {

class GlobalObject;
class WebrtcGlobalStatisticsCallback;
class WebrtcGlobalLoggingCallback;

class WebrtcGlobalInformation {
 public:
  MOZ_CAN_RUN_SCRIPT
  static void GetAllStats(const GlobalObject& aGlobal,
                          WebrtcGlobalStatisticsCallback& aStatsCallback,
                          const Optional<nsAString>& pcIdFilter,
                          ErrorResult& aRv);

  static void ClearAllStats(const GlobalObject& aGlobal);

  MOZ_CAN_RUN_SCRIPT
  static void GetLogging(const GlobalObject& aGlobal, const nsAString& aPattern,
                         WebrtcGlobalLoggingCallback& aLoggingCallback,
                         ErrorResult& aRv);

  static void ClearLogging(const GlobalObject& aGlobal);

  static void SetDebugLevel(const GlobalObject& aGlobal, int32_t aLevel);
  static int32_t DebugLevel(const GlobalObject& aGlobal);

  static void SetAecDebug(const GlobalObject& aGlobal, bool aEnable);
  static bool AecDebug(const GlobalObject& aGlobal);
  static void GetAecDebugLogDir(const GlobalObject& aGlobal, nsAString& aDir);

  static void StoreLongTermICEStatistics(PeerConnectionImpl& aPc);

 private:
  WebrtcGlobalInformation() = delete;
  WebrtcGlobalInformation(const WebrtcGlobalInformation& aOrig) = delete;
  WebrtcGlobalInformation& operator=(const WebrtcGlobalInformation& aRhs) =
      delete;
};

}  // namespace dom
}  // namespace mozilla

#endif  // _WEBRTC_GLOBAL_INFORMATION_H_
