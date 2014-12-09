/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"
#include "ECC_Types.h"

namespace CSF
{
	/**
	 * These callbacks relate to CallControlManager's "value add" features relating to authentication,
	 * configuration, setup, service health and management of SIP.
	 *
	 * They do not relate to call control - see also CC_Observer.
	 */
	class ECC_API ECC_Observer
	{
	public:
		virtual void onAvailablePhoneEvent (AvailablePhoneEventType::AvailablePhoneEvent event,
											const PhoneDetailsPtr availablePhoneDetails) = 0;

		virtual void onAuthenticationStatusChange (AuthenticationStatusEnum::AuthenticationStatus) = 0;
		virtual void onConnectionStatusChange(ConnectionStatusEnum::ConnectionStatus status) = 0;
	};
}
