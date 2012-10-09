/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
