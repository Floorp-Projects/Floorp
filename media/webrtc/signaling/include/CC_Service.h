/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"
#include "CC_Observer.h"

#include <vector>

extern "C"
{
#include "ccapi_types.h"
#include "ccapi_service.h"
}

namespace CSF
{
    class ECC_API CC_Service
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_Service)
    protected:
    	CC_Service() {}
        virtual ~CC_Service() {};

    public:
        /**
         * Clients use CC_Observer to receive CCAPI events (Device, Line, Call) from the service.
         */
        virtual void addCCObserver ( CC_Observer * observer ) = 0;
        virtual void removeCCObserver ( CC_Observer * observer ) = 0;

        /**
         * Use init() immediately on creating the service, and destroy() when finished with it.
         * password is required for Asterisk not CUCM.
         * deviceName is required for CUCM not Asterisk.
         */
        virtual bool init(const std::string& user, const std::string& password, const std::string& domain, const std::string& deviceName) = 0;
        virtual void destroy() = 0;

        /**
         * TODO: Set config parameters prior to starting the service.
         *		 Need to design a nice abstraction for this accommodating SIPCC and CTI.
         */

        /**
         * Use start() to attempt to register for a device and stop() to cancel a current
         * registration (or registration attempt).
         */
        virtual bool startService() = 0;
        virtual void stop() = 0;


        /**
         * Check on the current status/health of the service.
         */
        virtual bool isStarted() = 0;

        /**
         * Obtain the currently selected Device.
         * If multiple devices are discoverable (i.e. in CTI), all known devices will appear
         *   in getDevices(), but only the ActiveDevice will be controllable at any given time.
         */
        virtual CC_DevicePtr getActiveDevice() = 0;
        virtual std::vector<CC_DevicePtr> getDevices() = 0;

        /**
         * Global settings for audio and video control.  Return nullptr if Media control is not
         * available in this implementation.  Return nullptr in any case if media is not yet
         * initialized.
         * TODO: Assuming for now that media init aligns with init/destroy.
         */
        virtual AudioControlPtr getAudioControl() = 0;
        virtual VideoControlPtr getVideoControl() = 0;

        virtual bool setLocalVoipPort(int port) = 0;
        virtual bool setRemoteVoipPort(int port) = 0;
        virtual bool setP2PMode(bool mode) = 0;
        virtual bool setSDPMode(bool mode) = 0;

    private:
        CC_Service(const CC_Service& rhs);
        CC_Service& operator=(const CC_Service& rhs);
    };
}
