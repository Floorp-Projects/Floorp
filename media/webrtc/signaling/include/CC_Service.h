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
    protected:
    	CC_Service() {}
    public:
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
         * Global settings for audio and video control.  Return NULL if Media control is not
         * available in this implementation.  Return NULL in any case if media is not yet
         * initialized.
         * TODO: Assuming for now that media init aligns with init/destroy.
         */
        virtual AudioControlPtr getAudioControl() = 0;
        virtual VideoControlPtr getVideoControl() = 0;

        virtual bool setLocalVoipPort(int port) = 0;
        virtual bool setRemoteVoipPort(int port) = 0;
        virtual bool setP2PMode(bool mode) = 0;
        virtual bool setROAPProxyMode(bool mode) = 0;
        virtual bool setROAPClientMode(bool mode) = 0;

    private:
        CC_Service(const CC_Service& rhs);
        CC_Service& operator=(const CC_Service& rhs);
    };
}
