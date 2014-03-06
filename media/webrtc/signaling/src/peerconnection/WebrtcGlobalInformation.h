/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_INFORMATION_H_
#define _WEBRTC_GLOBAL_INFORMATION_H_

#include "nsString.h"

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
                          ErrorResult& aRv);

  static void GetLogging(const GlobalObject& aGlobal,
                         const nsAString& aPattern,
                         WebrtcGlobalLoggingCallback& aLoggingCallback,
                         ErrorResult& aRv);

private:
  WebrtcGlobalInformation() MOZ_DELETE;
  WebrtcGlobalInformation(const WebrtcGlobalInformation& aOrig) MOZ_DELETE;
  WebrtcGlobalInformation& operator=(
    const WebrtcGlobalInformation& aRhs) MOZ_DELETE;
};

} // namespace dom
} // namespace mozilla

#endif  // _WEBRTC_GLOBAL_INFORMATION_H_

