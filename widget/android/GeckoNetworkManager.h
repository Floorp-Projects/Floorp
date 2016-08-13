/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoNetworkManager_h
#define GeckoNetworkManager_h

#include "GeneratedJNINatives.h"
#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsINetworkLinkService.h"

#include "mozilla/Services.h"

namespace mozilla {

class GeckoNetworkManager final
    : public java::GeckoNetworkManager::Natives<GeckoNetworkManager>
{
    GeckoNetworkManager() = delete;

public:
    static void
    OnConnectionChanged(int32_t aType, jni::String::Param aSubType,
                        bool aIsWifi, int32_t aGateway)
    {
        hal::NotifyNetworkChange(hal::NetworkInformation(
                aType, aIsWifi, aGateway));

        nsCOMPtr<nsIObserverService> os = services::GetObserverService();
        if (os) {
            os->NotifyObservers(nullptr,
                                NS_NETWORK_LINK_TYPE_TOPIC,
                                aSubType->ToString().get());
        }
    }

    static void
    OnStatusChanged(jni::String::Param aStatus)
    {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os) {
            os->NotifyObservers(nullptr,
                                NS_NETWORK_LINK_TOPIC,
                                aStatus->ToString().get());
        }
    }
};

} // namespace mozilla

#endif // GeckoNetworkManager_h
