/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_DeviceInfo)
    protected:
        CC_DeviceInfo() { }

        //Base class needs dtor to be declared as virtual
        virtual ~CC_DeviceInfo() {};

    public:
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
