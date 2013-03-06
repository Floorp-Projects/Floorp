/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"

#include "CC_SIPCCDeviceInfo.h"
#include "CC_SIPCCFeatureInfo.h"
#include "CC_SIPCCCallServerInfo.h"
#include "CC_SIPCCCall.h"
#include "CC_SIPCCLine.h"

#include "csf_common.h"

extern "C"
{
#include "cpr_types.h"
#include "ccapi_device.h"
#include "ccapi_device_info.h"
}

using namespace std;
using namespace CSF;

#define MAX_SUPPORTED_NUM_CALLS        100
#define MAX_SUPPORTED_NUM_LINES        100
#define MAX_SUPPORTED_NUM_FEATURES     100
#define MAX_SUPPORTED_NUM_CALL_SERVERS 100


CSF_IMPLEMENT_WRAP(CC_SIPCCDeviceInfo, cc_deviceinfo_ref_t);

CC_SIPCCDeviceInfo::CC_SIPCCDeviceInfo (cc_deviceinfo_ref_t deviceinfo)
: deviceinfo_ref(deviceinfo)
{
    CCAPI_Device_retainDeviceInfo(deviceinfo_ref);
}

CC_SIPCCDeviceInfo::~CC_SIPCCDeviceInfo()
{
    CCAPI_Device_releaseDeviceInfo(deviceinfo_ref);
}

string CC_SIPCCDeviceInfo::getDeviceName()
{
    return CCAPI_DeviceInfo_getDeviceName(deviceinfo_ref);
}

vector<CC_CallPtr> CC_SIPCCDeviceInfo::getCalls ()
{
    vector<CC_CallPtr> callsVector;

    cc_call_handle_t handles[MAX_SUPPORTED_NUM_CALLS] = {};
    cc_uint16_t numHandles = csf_countof(handles);

    CCAPI_DeviceInfo_getCalls(deviceinfo_ref, handles, &numHandles) ;

    for (int i=0; i<numHandles; i++)
    {
        CC_CallPtr callPtr = CC_SIPCCCall::wrap(handles[i]).get();
        callsVector.push_back(callPtr);
    }

    return callsVector;
}

vector<CC_LinePtr> CC_SIPCCDeviceInfo::getLines ()
{
    vector<CC_LinePtr> linesVector;

    cc_lineid_t lines[MAX_SUPPORTED_NUM_LINES] = {};
    cc_uint16_t numLines = csf_countof(lines);

    CCAPI_DeviceInfo_getLines(deviceinfo_ref, lines, &numLines) ;

    for (int i=0; i<numLines; i++)
    {
        CC_LinePtr linePtr = CC_SIPCCLine::wrap(lines[i]).get();
        linesVector.push_back(linePtr);
    }

    return linesVector;
}

vector<CC_FeatureInfoPtr> CC_SIPCCDeviceInfo::getFeatures ()
{
    vector<CC_FeatureInfoPtr> featuresVector;

    cc_featureinfo_ref_t features[MAX_SUPPORTED_NUM_FEATURES] = {};
    cc_uint16_t numFeatureInfos = csf_countof(features);

    CCAPI_DeviceInfo_getFeatures(deviceinfo_ref, features, &numFeatureInfos);

    for (int i=0; i<numFeatureInfos; i++)
    {
        CC_FeatureInfoPtr featurePtr =
          CC_SIPCCFeatureInfo::wrap(features[i]).get();
        featuresVector.push_back(featurePtr);
    }

    return featuresVector;
}

vector<CC_CallServerInfoPtr> CC_SIPCCDeviceInfo::getCallServers ()
{
    vector<CC_CallServerInfoPtr> callServersVector;

    cc_callserver_ref_t callServers[MAX_SUPPORTED_NUM_CALL_SERVERS] = {};
    cc_uint16_t numCallServerInfos = csf_countof(callServers);

    CCAPI_DeviceInfo_getCallServers(deviceinfo_ref, callServers, &numCallServerInfos);

    for (int i=0; i<numCallServerInfos; i++)
    {
        CC_CallServerInfoPtr callServerPtr =
          CC_SIPCCCallServerInfo::wrap(callServers[i]).get();
        callServersVector.push_back(callServerPtr);
    }

    return callServersVector;
}

cc_service_state_t CC_SIPCCDeviceInfo::getServiceState()
{
    return CCAPI_DeviceInfo_getServiceState(deviceinfo_ref);
}


cc_service_cause_t CC_SIPCCDeviceInfo::getServiceCause()
{
    return CCAPI_DeviceInfo_getServiceCause(deviceinfo_ref);
}
