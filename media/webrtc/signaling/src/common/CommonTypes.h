/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once


#include <string>

namespace csf
{

namespace ProviderStateEnum
{
	enum ProviderState
	{
		Ready,
		Registering,
		AwaitingIpAddress,
		FetchingDeviceConfig,
		Idle,
		RecoveryPending,
		Connected
	};
	const std::string toString(ProviderState);
}
namespace LoginErrorStatusEnum
{
	enum LoginErrorStatus {
		Ok,								// No Error
		Unknown,						// Unknown Error
		NoCallManagerConfigured,		// No Primary or Backup Call Manager
		NoDevicesFound,					// No devices
		NoCsfDevicesFound,				// Devices but none of type CSF
		PhoneConfigGenError,			// Could not generate phone config
		SipProfileGenError,			    // Could not build SIP profile
		ConfigNotSet,					// Config not set before calling login()
		CreateConfigProviderFailed,		// Could not create ConfigProvider
		CreateSoftPhoneProviderFailed,	// Could not create SoftPhoneProvider
		MissingUsername,				// Username argument missing,
		ManualLogout,			        // logout() has been called
		LoggedInElseWhere,				// Another process has the mutex indicating it is logged in
		AuthenticationFailure,			// Authentication failure (probably bad password, but best not to say for sure)
		CtiCouldNotConnect,				// Could not connect to CTI service
		InvalidServerSearchList
	};
	const std::string toString(LoginErrorStatus);
}

namespace ErrorCodeEnum
{
	enum ErrorCode
	{
		Ok,
		Unknown,
		InvalidState,
		InvalidArgument
	};
	const std::string toString(ErrorCode);
}

} // namespace csf

