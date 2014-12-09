/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include "ECC_Types.h"

using namespace std;

namespace CSF
{

namespace AuthenticationFailureCodeType
{
std::string ECC_API toString(AuthenticationFailureCode value)
{
	switch(value)
	{
    case eNoError:
        return "eNoError";
	case eNoServersConfigured:
        return "eNoServersConfigured";
	case eNoCredentialsConfigured:
        return "eNoCredentialsConfigured";
	case eCouldNotConnect:
        return "eCouldNotConnect";
	case eServerCertificateRejected:
        return "eServerCertificateRejected";
	case eCredentialsRejected:
        return "eCredentialsRejected";
	case eResponseEmpty:
        return "eResponseEmpty";
    case eResponseInvalid:
        return "eResponseInvalid";
	default:
		return "";
	}
}
}

namespace AuthenticationStatusEnum
{
std::string ECC_API toString(AuthenticationStatus value)
{
    switch(value)
	{
    case eNotAuthenticated:
        return "eNotAuthenticated";
    case eInProgress:
        return "eInProgress";
    case eAuthenticated:
        return "eAuthenticated";
    case eFailed:
        return "eFailed";
	default:
		return "";
	}
}
}

namespace DeviceRetrievalFailureCodeType
{
std::string ECC_API toString(DeviceRetrievalFailureCode value)
{
    switch(value)
	{
    case eNoError:
        return "eNoError";
    case eNoServersConfigured:
        return "eNoServersConfigured";
    case eNoDeviceNameConfigured:
        return "eNoDeviceNameConfigured";
    case eCouldNotConnect:
        return "eCouldNotConnect";
    case eFileNotFound:
        return "eFileNotFound";
    case eFileEmpty:
        return "eFileEmpty";
    case eFileInvalid:
        return "eFileInvalid";
	default:
		return "";
	}
}
}

namespace ConnectionStatusEnum
{
std::string ECC_API toString(ConnectionStatus value)
{
    switch(value)
	{
    case eIdle:
        return "eIdle";
    case eNone:
        return "eNone";
    case eFetchingDeviceConfig:
        return "eFetchingDeviceConfig";
    case eRegistering:
        return "eRegistering";
    case eReady:
        return "eReady";
    case eConnectedButNoDeviceReady:
        return "eConnectedButNoDeviceReady";
    case eRetrying:
        return "eRetrying";
    case eFailed:
        return "eFailed";
	default:
		return "";
	}
}
}

namespace ServiceStateType
{
std::string ECC_API toString(ServiceState value)
{
    switch(value)
	{
    case eUnknown:
        return "eUnknown";
    case eInService:
        return "eInService";
    case eOutOfService:
        return "eOutOfService";
	default:
		return "";
	}
}
}

namespace AvailablePhoneEventType
{
std::string ECC_API toString(AvailablePhoneEvent value)
{
    switch(value)
	{
    case eFound:
        return "eFound";
    case eUpdated:
        return "eUpdated";
    case eLost:
        return "eLost";
	default:
		return "";
	}
}
}

}
