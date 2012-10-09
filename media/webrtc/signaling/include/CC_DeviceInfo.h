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

#include <vector>

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
    class ECC_API CC_DeviceInfo
    {
    protected:
        CC_DeviceInfo() { }

    public:
        //Base class needs dtor to be declared as virtual
        virtual ~CC_DeviceInfo() {};

        /**
           gets the device name
           @returns - the device name as an std::string
         */
        virtual std::string getDeviceName() = 0;

        /**
           gets the service state
           @param [in] handle - reference to device info 
           @returns cc_service_state_t - INS/OOS 
         */
        virtual cc_service_state_t getServiceState() = 0;

        /**
           gets the service cause
           @param [in] handle - reference to device info 
           @returns cc_service_cause_t - reason for service state
         */
        virtual cc_service_cause_t getServiceCause() = 0;

        /**
           gets vector of CC_CallPtr from this CC_DeviceInfo

           @returns vector<CC_CallPtr> containing the CC_CallPtrs
         */
        virtual std::vector<CC_CallPtr> getCalls () = 0;

        /**
           gets list of handles to calls on the device by state
           @param [in] handle - reference to device info
           @param [in] state - call state for which the calls are requested
           @param [out] handles - array of call handle to be returned
           @param [in,out] count number allocated in array/elements returned
           @returns
         */
//        void getCallsByState (cc_call_state_t state,
//                              cc_call_handle_t handles[], cc_uint16_t *count);

        /**
           gets vector of CC_LinePtr from this CC_DeviceInfo

           @returns vector<CC_LinePtr> containing the CC_LinePtrs
         */
        virtual std::vector<CC_LinePtr> getLines () = 0;

        /**
           gets vector of features on the device

           @returns
         */
        virtual std::vector<CC_FeatureInfoPtr> getFeatures () = 0;

        /**
           gets handles of call agent servers

           @returns 
         */
        virtual std::vector<CC_CallServerInfoPtr> getCallServers () = 0;

        /**
           gets call server name
           @param [in] handle - handle of call server
           @returns name of the call server
           NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
         */
//        cc_string_t getCallServerName (cc_callserver_ref_t handle);

        /**
           gets call server mode
           @param [in] handle - handle of call server
           @returns - mode of the call server
         */
//        cc_cucm_mode_t getCallServerMode (cc_callserver_ref_t handle);

        /**
           gets calls erver name
           @param [in] handle - handle of call server
           @returns status of the call server
         */
//        cc_ccm_status_t getCallServerStatus (cc_callserver_ref_t handle);

        /**
           get the NOTIFICATION PROMPT
           @param [in] handle - reference to device info
           @returns
         */
//        cc_string_t getNotifyPrompt ();

        /**
           get the NOTIFICATION PROMPT PRIORITY
           @param [in] handle - reference to device info
           @returns
         */
//        cc_uint32_t getNotifyPromptPriority ();

        /**
           get the NOTIFICATION PROMPT Progress
           @param [in] handle - reference to device info
           @returns
         */
//        cc_uint32_t getNotifyPromptProgress ();

    private:
		// Cannot copy - clients should be passing the pointer not the object.
		CC_DeviceInfo& operator=(const CC_DeviceInfo& rhs);
		CC_DeviceInfo(const CC_DeviceInfo&);
    };
};
