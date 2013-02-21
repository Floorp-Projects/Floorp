/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"

#include "CC_SIPCCFeatureInfo.h"
#include "CC_SIPCCDeviceInfo.h"

extern "C"
{
#include "ccapi_device.h"
#include "ccapi_feature_info.h"
}

using namespace std;
using namespace CSF;

CSF_IMPLEMENT_WRAP(CC_SIPCCFeatureInfo, cc_featureinfo_ref_t);

CC_SIPCCFeatureInfo::CC_SIPCCFeatureInfo (cc_featureinfo_ref_t featureinfo) : featureinfo_ref(featureinfo)
{
//    CCAPI_Device_retainFeatureInfo(featureinfo);
}

CC_SIPCCFeatureInfo::~CC_SIPCCFeatureInfo()
{
//    CCAPI_Feature_releaseFeatureInfo(featureinfo_ref);
}

cc_int32_t CC_SIPCCFeatureInfo::getButton()
{
    return CCAPI_featureInfo_getButton(featureinfo_ref);
}

cc_int32_t CC_SIPCCFeatureInfo::getFeatureID()
{
    return CCAPI_featureInfo_getFeatureID(featureinfo_ref);
}

string CC_SIPCCFeatureInfo::getDisplayName()
{
    return CCAPI_featureInfo_getDisplayName(featureinfo_ref);
}

string CC_SIPCCFeatureInfo::getSpeedDialNumber()
{
    return CCAPI_featureInfo_getSpeedDialNumber(featureinfo_ref);
}

string CC_SIPCCFeatureInfo::getContact()
{
    return CCAPI_featureInfo_getContact(featureinfo_ref);
}

string CC_SIPCCFeatureInfo::getRetrievalPrefix()
{
    return CCAPI_featureInfo_getRetrievalPrefix(featureinfo_ref);
}

cc_int32_t CC_SIPCCFeatureInfo::getFeatureOptionMask()
{
    return CCAPI_featureInfo_getFeatureOptionMask(featureinfo_ref);
}
