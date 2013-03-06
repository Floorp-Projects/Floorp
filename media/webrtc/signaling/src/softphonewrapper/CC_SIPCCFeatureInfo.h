/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCC_FEATUREINFO_H
#define _CC_SIPCC_FEATUREINFO_H

#include "CC_FeatureInfo.h"

#include "common/Wrapper.h"

namespace CSF
{
	DECLARE_NS_PTR(CC_SIPCCFeatureInfo);
    class CC_SIPCCFeatureInfo : public CC_FeatureInfo
    {
    private:
        cc_featureinfo_ref_t featureinfo_ref;
        CC_SIPCCFeatureInfo (cc_featureinfo_ref_t aFeatureInfo);
        CSF_DECLARE_WRAP(CC_SIPCCFeatureInfo, cc_featureinfo_ref_t);

    public:
        ~CC_SIPCCFeatureInfo();

    public:
        virtual cc_int32_t getButton();
        virtual cc_int32_t getFeatureID();
        virtual std::string getDisplayName();
        virtual std::string getSpeedDialNumber();
        virtual std::string getContact();
        virtual std::string getRetrievalPrefix();
        virtual cc_int32_t getFeatureOptionMask();
    };
};

#endif
