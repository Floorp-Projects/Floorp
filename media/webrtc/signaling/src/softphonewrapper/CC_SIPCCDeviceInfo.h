/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCC_DEVICEINFO_H
#define _CC_SIPCC_DEVICEINFO_H

#include "CC_DeviceInfo.h"

#include "common/Wrapper.h"

namespace CSF
{
	DECLARE_NS_PTR(CC_SIPCCDeviceInfo);
    class CC_SIPCCDeviceInfo : public CC_DeviceInfo
    {
    private:
        explicit CC_SIPCCDeviceInfo (cc_deviceinfo_ref_t aDeviceInfo);
        cc_deviceinfo_ref_t deviceinfo_ref;

        CSF_DECLARE_WRAP(CC_SIPCCDeviceInfo, cc_deviceinfo_ref_t);

    public:
        ~CC_SIPCCDeviceInfo();

    public:
        virtual std::string getDeviceName();
        virtual cc_service_cause_t getServiceCause();
        virtual cc_service_state_t getServiceState();
        virtual std::vector<CC_CallPtr> getCalls();
        virtual std::vector<CC_LinePtr> getLines();
        virtual std::vector<CC_FeatureInfoPtr> getFeatures();
        virtual std::vector<CC_CallServerInfoPtr> getCallServers();
    };
};

#endif
