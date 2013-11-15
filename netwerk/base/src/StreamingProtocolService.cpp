/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "StreamingProtocolService.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIURI.h"
#include "necko-config.h"

#ifdef NECKO_PROTOCOL_rtsp
#include "RtspControllerChild.h"
#include "RtspController.h"
#endif

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS1(StreamingProtocolControllerService,
                   nsIStreamingProtocolControllerService)

/* static */
StaticRefPtr<StreamingProtocolControllerService> sSingleton;

/* static */
already_AddRefed<StreamingProtocolControllerService>
StreamingProtocolControllerService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new StreamingProtocolControllerService();
    ClearOnShutdown(&sSingleton);
  }
  nsRefPtr<StreamingProtocolControllerService> service = sSingleton.get();
  return service.forget();
}

NS_IMETHODIMP
StreamingProtocolControllerService::Create(nsIChannel *aChannel, nsIStreamingProtocolController **aResult)
{
  nsRefPtr<nsIStreamingProtocolController> mediacontroller;
  nsCOMPtr<nsIURI> uri;
  nsAutoCString scheme;

  NS_ENSURE_ARG_POINTER(aChannel);
  aChannel->GetURI(getter_AddRefs(uri));

  nsresult rv = uri->GetScheme(scheme);
  if (NS_FAILED(rv)) return rv;

#ifdef NECKO_PROTOCOL_rtsp
  if (scheme.EqualsLiteral("rtsp")) {
    if (IsNeckoChild()) {
      mediacontroller = new RtspControllerChild(aChannel);
    } else {
      mediacontroller = new RtspController(aChannel);
    }
  }
#endif

  if (!mediacontroller) {
    return NS_ERROR_NO_INTERFACE;
  }

  mediacontroller->Init(uri);

  mediacontroller.forget(aResult);
  return NS_OK;
}

} // namespace net
} // namespace mozilla
