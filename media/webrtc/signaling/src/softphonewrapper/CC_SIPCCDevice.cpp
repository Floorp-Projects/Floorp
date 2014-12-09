/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

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

CSF_IMPLEMENT_WRAP(CC_SIPCCDevice, cc_device_handle_t);

CC_DevicePtr CC_SIPCCDevice::createDevice ()
{
    cc_device_handle_t deviceHandle = CCAPI_Device_getDeviceID();

    CC_SIPCCDevicePtr pSIPCCDevice = CC_SIPCCDevice::wrap(deviceHandle);

    pSIPCCDevice->enableVideo(true);
    pSIPCCDevice->enableCamera(true);

    return pSIPCCDevice.get();
}

CC_SIPCCDevice::CC_SIPCCDevice (cc_device_handle_t aDeviceHandle)
: deviceHandle(aDeviceHandle)
{
}

CC_DeviceInfoPtr CC_SIPCCDevice::getDeviceInfo ()
{
    cc_deviceinfo_ref_t deviceInfoRef = CCAPI_Device_getDeviceInfo(deviceHandle);
    CC_DeviceInfoPtr deviceInfoPtr =
        CC_SIPCCDeviceInfo::wrap(deviceInfoRef).get();

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
    std::string result;
    char tmpString[11];
    csf_sprintf(tmpString, sizeof(tmpString), "%X", deviceHandle);
    result = tmpString;
    return result;
}

CC_CallPtr CC_SIPCCDevice::createCall ()
{
    cc_call_handle_t callHandle = CCAPI_Device_CreateCall(deviceHandle);

    return CC_SIPCCCall::wrap(callHandle).get();
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
