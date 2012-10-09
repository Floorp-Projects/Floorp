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
    protected:
        CC_FeatureInfo() { }

    public:
        //Base class needs dtor to be declared as virtual
        virtual ~CC_FeatureInfo() {};

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
