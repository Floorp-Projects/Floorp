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

#include "CSFLog.h"

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
        CC_CallPtr callPtr = CC_SIPCCCall::wrap(handles[i]);
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
        CC_LinePtr linePtr = CC_SIPCCLine::wrap(lines[i]);
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
        CC_FeatureInfoPtr featurePtr = CC_SIPCCFeatureInfo::wrap(features[i]);
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
        CC_CallServerInfoPtr callServerPtr = CC_SIPCCCallServerInfo::wrap(callServers[i]);
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
