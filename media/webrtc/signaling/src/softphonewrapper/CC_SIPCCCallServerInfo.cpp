/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "CC_Common.h"

#include "CC_SIPCCCallServerInfo.h"

extern "C"
{
#include "ccapi_device_info.h"
}

using namespace std;
using namespace CSF;

CC_SIPCCCallServerInfo::CC_SIPCCCallServerInfo (cc_callserver_ref_t call_serverinfo) : callserverinfo_ref(call_serverinfo)
{
}

CSF_IMPLEMENT_WRAP(CC_SIPCCCallServerInfo, cc_callserver_ref_t);

CC_SIPCCCallServerInfo::~CC_SIPCCCallServerInfo()
{
}

string CC_SIPCCCallServerInfo::getCallServerName()
{
    return CCAPI_DeviceInfo_getCallServerName(callserverinfo_ref);
}

cc_cucm_mode_t CC_SIPCCCallServerInfo::getCallServerMode()
{
    return CCAPI_DeviceInfo_getCallServerMode(callserverinfo_ref);
}

cc_ccm_status_t CC_SIPCCCallServerInfo::getCallServerStatus()
{
    return CCAPI_DeviceInfo_getCallServerStatus(callserverinfo_ref);
}
