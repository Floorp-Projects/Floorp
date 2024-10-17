/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Foundation/Foundation.h>

#include "MicrosoftEntraSSOUtils.h"
#include "nsIURI.h"
#include "nsHttpChannel.h"
#include "mozilla/Logging.h"

namespace {
static mozilla::LazyLogModule gMacOSWebAuthnServiceLog("macOSSingleSignOn");
}  // namespace

NS_ASSUME_NONNULL_BEGIN

namespace mozilla {
namespace net {

class API_AVAILABLE(macos(13.3)) MicrosoftEntraSSOUtils final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MicrosoftEntraSSOUtils)

  MicrosoftEntraSSOUtils() = default;
  void AddMicrosoftEntraSSOInternal(nsHttpChannel* channel);

 private:
  ~MicrosoftEntraSSOUtils() = default;
};

void MicrosoftEntraSSOUtils::AddMicrosoftEntraSSOInternal(
    nsHttpChannel* channel) {
  return;
}

API_AVAILABLE(macos(13.3))
void AddMicrosoftEntraSSO(nsHttpChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<MicrosoftEntraSSOUtils> service = new MicrosoftEntraSSOUtils();
  service->AddMicrosoftEntraSSOInternal(aChannel);
  MOZ_LOG(gMacOSWebAuthnServiceLog, mozilla::LogLevel::Debug,
          ("MicrosoftEntraSSOUtils::AddMicrosoftEntraSSO end"));
}
}  // namespace net
}  // namespace mozilla

NS_ASSUME_NONNULL_END
