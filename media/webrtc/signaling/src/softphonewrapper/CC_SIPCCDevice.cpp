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

#include "CC_Common.h"

#include "CC_SIPCCDevice.h"
#include "CC_SIPCCDeviceInfo.h"
#include "CC_SIPCCFeatureInfo.h"
#include "CC_SIPCCCall.h"

extern "C"
{
#include "cpr_types.h"
#include "config_api.h"
#include "ccapi_device.h"
#include "ccapi_device_info.h"
#include "ccapi_device_listener.h"
}

using namespace std;
using namespace CSF;

#include "CSFLogStream.h"
static const char* logTag = "CC_SIPCCDevice";

CSF_IMPLEMENT_WRAP(CC_SIPCCDevice, cc_device_handle_t);

/* static */
CC_DevicePtr CC_SIPCCDevice::createAndValidateXML (bool isXMLString, const string & configFileNameOrXMLString)
{
    cc_device_handle_t deviceHandle = CCAPI_Device_getDeviceID();

    CC_SIPCCDevicePtr pSIPCCDevice = CC_SIPCCDevice::wrap(deviceHandle);

    if (!pSIPCCDevice->checkXMLPhoneConfigValidity(isXMLString, configFileNameOrXMLString))
    {
        if (!isXMLString)
        {
            CSFLogError(logTag, "Phone Config file \"%s\" is not valid.", configFileNameOrXMLString.c_str());
        }
        else
        {
            CSFLogErrorS(logTag, "Phone Config XML is not valid.");
        }

        CSFLogInfoS(logTag, "pSIPCC requires that the phone config contain (at a minimum) a \"port config\", a \"proxy config\" and a \"line config\".");

        wrapper.release(deviceHandle);

        return NULL_PTR(CC_Device);
    }

    return pSIPCCDevice;
}

CC_DevicePtr CC_SIPCCDevice::createDevice ()
{
    cc_device_handle_t deviceHandle = CCAPI_Device_getDeviceID();

    CC_SIPCCDevicePtr pSIPCCDevice = CC_SIPCCDevice::wrap(deviceHandle);

    return pSIPCCDevice;
}

CC_SIPCCDevice::CC_SIPCCDevice (cc_device_handle_t aDeviceHandle)
: deviceHandle(aDeviceHandle)
{
	enableVideo(true);
	enableCamera(true);
}

bool CC_SIPCCDevice::checkXMLPhoneConfigValidity (bool isXMLString, const string & configFileNameOrXMLString)
{
    return (CCAPI_Config_checkValidity(deviceHandle, configFileNameOrXMLString.c_str(), (isXMLString) ? TRUE : 0 ) != 0);
}

CC_DeviceInfoPtr CC_SIPCCDevice::getDeviceInfo ()
{
    cc_deviceinfo_ref_t deviceInfoRef = CCAPI_Device_getDeviceInfo(deviceHandle);
    CC_DeviceInfoPtr deviceInfoPtr = CC_SIPCCDeviceInfo::wrap(deviceInfoRef);

    //A call to CCAPI_Device_getDeviceInfo() needs a matching call to CCAPI_Device_releaseDeviceInfo()
    //However, the CC_SIPCCDeviceInfo() ctor/dtor does a retain/release internally, so I need to explicitly release
    //here to match up with the call to CCAPI_Device_getDeviceInfo().

    CCAPI_Device_releaseDeviceInfo(deviceInfoRef);

    //CCAPI_Device_getDeviceInfo() --> requires release be called.
    //CC_SIPCCDeviceInfo::CC_SIPCCDeviceInfo() -> Call retain (wrapped in smart_ptr)
    //CCAPI_Device_releaseDeviceInfo() --> this maps to the call to CCAPI_Device_getDeviceInfo()
    //CC_SIPCCDeviceInfo::~CC_SIPCCDeviceInfo() --> CCAPI_Device_releaseDeviceInfo() (when smart pointer destroyed)

    return deviceInfoPtr;
}

std::string CC_SIPCCDevice::toString()
{
    std::stringstream sstream;
    sstream << "0x" << std::setw( 5 ) << std::setfill( '0' ) << std::hex << deviceHandle;
    return sstream.str();
}

CC_CallPtr CC_SIPCCDevice::createCall ()
{
    cc_call_handle_t callHandle = CCAPI_Device_CreateCall(deviceHandle);

    return CC_SIPCCCall::wrap(callHandle);
}

void CC_SIPCCDevice::enableVideo(bool enable)
{
    CCAPI_Device_enableVideo(deviceHandle, enable);
}

void CC_SIPCCDevice::enableCamera(bool enable)    
{
    CCAPI_Device_enableCamera(deviceHandle, enable);
}

void CC_SIPCCDevice::setDigestNamePasswd (char *name, char *pw)
{
    CCAPI_Device_setDigestNamePasswd(deviceHandle, name, pw);
}
