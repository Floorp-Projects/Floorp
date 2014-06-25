/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_StreamingProtocolControllerService_h
#define mozilla_net_StreamingProtocolControllerService_h

#include "mozilla/StaticPtr.h"
#include "nsIStreamingProtocolService.h"
#include "nsIStreamingProtocolController.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

/**
 * This class implements a service to help to create streaming protocol controller.
 */
class StreamingProtocolControllerService : public nsIStreamingProtocolControllerService
{
private:
  virtual ~StreamingProtocolControllerService() {};

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMINGPROTOCOLCONTROLLERSERVICE

  StreamingProtocolControllerService() {};
  static already_AddRefed<StreamingProtocolControllerService> GetInstance();
};
} // namespace dom
} // namespace mozilla

#endif //mozilla_net_StreamingProtocolControllerService_h
