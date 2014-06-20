/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
    class ECC_API CC_FeatureInfo
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_FeatureInfo)
    protected:
        CC_FeatureInfo() { }

        //Base class needs dtor to be declared as virtual
        virtual ~CC_FeatureInfo() {};

    public:
        /**
           Get the physical button number on which this feature is configured

           @return cc_int32_t - button assgn to the feature
         */
        virtual cc_int32_t getButton() = 0;

        /**
           Get the featureID

           @return cc_int32_t - button assgn to the feature
         */
        virtual cc_int32_t getFeatureID() = 0;

        /**
           Get the feature Name

           @return string - handle of the feature created
         */
        virtual std::string getDisplayName() = 0;

        /**
           Get the speeddial Number

           @return string - handle of the feature created
         */
        virtual std::string getSpeedDialNumber() = 0;

        /**
           Get the contact

           @return string - handle of the feature created
         */
        virtual std::string getContact() = 0;

        /**
           Get the retrieval prefix

           @return string - handle of the feature created
         */
        virtual std::string getRetrievalPrefix() = 0;

        /**
           Get the feature option mask

           @return cc_int32_t - button assgn to the feature
         */
        virtual cc_int32_t getFeatureOptionMask() = 0;
    };
};
