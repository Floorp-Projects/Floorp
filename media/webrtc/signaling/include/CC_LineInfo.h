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
#include <bitset>
#include <set>
#include <vector>

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
    class ECC_API CC_LineInfo
    {
    protected:
        CC_LineInfo() { }

    public:
        //Base class needs dtor to be declared as virtual
        virtual ~CC_LineInfo() {};

        /**
           Get the line Name

           @return string - line Name
         */
        virtual std::string getName() = 0;

        /**
           Get the line DN Number
           @return string - line DN
         */
        virtual std::string getNumber() = 0;

        /**
           Get the physical button number on which this line is configured

           @return cc_uint32_t - button number
         */
        virtual cc_uint32_t getButton() = 0;

        /**
           Get the Line Type

           @return cc_line_feature_t - line featureID ( Line )
         */
        virtual cc_line_feature_t getLineType() = 0;

        /**

           @return bool - true if phone is currently registered with CM.
         */
        virtual bool getRegState() = 0;

        /**
           Get the CFWDAll status for the line

           @return bool - isForwarded
         */
        virtual bool isCFWDActive() = 0;

        /**
           Get the CFWDAll destination

           @return string - cfwd target
         */
        virtual std::string getCFWDName() = 0;

        /**
           Get calls on line
           
           @param [in] line - line
           @return vector<CC_CallPtr>
         */
        virtual std::vector<CC_CallPtr> getCalls (CC_LinePtr linePtr) = 0;

        /**
           Get calls on line by state

           @param [in] line - line
           @param [in] state - state
           
           @return vector<CC_CallPtr>
         */
        virtual std::vector<CC_CallPtr> getCallsByState (CC_LinePtr linePtr, cc_call_state_t state) = 0;

        /**
           Get the MWI Status

           @return cc_uint32_t - MWI status (boolean 0 => no MWI)
         */
        virtual bool getMWIStatus() = 0;

        /**
           Get the MWI Type
           
           @return cc_uint32_t - MWI Type
         */
        virtual cc_uint32_t getMWIType() = 0;

        /**
           Get the MWI new msg count

           @return cc_uint32_t - MWI new msg count
         */
        virtual cc_uint32_t getMWINewMsgCount() = 0;

        /**
           Get the MWI old msg count
           
           @return cc_uint32_t - MWI old msg count
         */
        virtual cc_uint32_t getMWIOldMsgCount() = 0;

        /**
           Get the MWI high priority new msg count
           
           @return cc_uint32_t - MWI new msg count
         */
        virtual cc_uint32_t getMWIPrioNewMsgCount() = 0;

        /**
           Get the MWI high priority old msg count

           @return cc_uint32_t - MWI old msg count
         */
        virtual cc_uint32_t getMWIPrioOldMsgCount() = 0;

        /**
           has capability - is the feature allowed

           @param [in] capability - capability being queried to see if it's available
           @return bool - is Allowed
         */
        virtual bool hasCapability(ccapi_call_capability_e capability) = 0;

        /**
           get Allowed Feature set

           @return cc_return_t - bitset of Line Capabilities.
         */
        virtual std::bitset<CCAPI_CALL_CAP_MAX> getCapabilitySet() = 0;
    };
};
