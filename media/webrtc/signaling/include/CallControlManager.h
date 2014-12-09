/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

#include "CC_Observer.h"
#include "ECC_Observer.h"
#include "ECC_Types.h"

#include <string>
#include <vector>

/**
 *  @mainpage Enhanced Call Control
 *
 *  @section intro_sec Introduction
 *  This wraps and aggregates the SIPCC and CTI call control stacks, media stacks, and various additional
 *  components and glue necessary to start, configure and run them, and presents a high-level C++ API
 *  for connection, device selection and status, and call control.
 *
 *  @section main_outline Outline
 *  @li The main entry point is CSF::CallControlManager, which is used to configure and start a
 *  	call control stack.
 *  @li Configuration and events are raised to the CSF::ECC_Observer interface.
 *  @li Call Control is performed via CSF::CC_Device, CSF::CC_Line and CSF::CC_Call.
 *  @li Call Control events are raised to the CSF::CC_Observer interface.
 *  @li Audio/Video device selection and global media configuration is performed via CSF::AudioControl
 *      and CSF::VideoControl.  Per-call media operations (mute, volume, etc) are integrated onto
 *      the CSF::CC_Call and CSF::CC_CallInfo interfaces.
 */

namespace CSF
{
	DECLARE_NS_PTR(CallControlManager)
	/**
	 * CallControlManager
	 *
	 * The class is partitioned into several blocks of functionality:
	 * - Create/Destroy - Initialisation and clean shutdown.
	 * 					  Destroy is optional if the destructor is used properly.
	 * - Observer - Register for events when any state changes.  Optional but strongly advised.
	 *
	 * Methods are generally synchronous (at present).
	 */
    class ECC_API CallControlManager
    {
    protected:
        virtual ~CallControlManager();

    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CallControlManager)
		/**
		 *  Use create() to create a CallControlManager instance.
		 *
		 *  CallControlManager cleans up its resources in its destructor, implicitly disconnect()in if required.
		 *  Use the destroy() method if you need to force a cleanup earlier.  It is a bad idea to continue using
		 *  CallControlManager or any of its related objects after destroy().
		 */
        static CallControlManagerPtr create();
        virtual bool destroy() = 0;

        /**
           CC_Observer is for core call control events (on CC_Device, CC_Line and CC_Call).
           ECC_Observer is for "value add" features of CallControlManager.

           Client can add multiple observers if they have different Observer objects that handle
           different event scenarios, but generally it's probably sufficient to only register one observer.

           @param[in] observer - This is a pointer to a CC_Observer-derived class that the client
                                 must instantiate to receive notifications on this client object.
         */
        virtual void addCCObserver ( CC_Observer * observer ) = 0;
        virtual void removeCCObserver ( CC_Observer * observer ) = 0;

        virtual void addECCObserver ( ECC_Observer * observer ) = 0;
        virtual void removeECCObserver ( ECC_Observer * observer ) = 0;

        virtual void setMultiClusterMode(bool allowMultipleClusters) = 0;
        virtual void setSIPCCLoggingMask(const cc_int32_t mask) = 0;
        virtual void setAuthenticationString(const std::string &authString) = 0;
        virtual void setSecureCachePath(const std::string &secureCachePath) = 0;

        // Add local codecs
        virtual void setAudioCodecs(int codecMask) = 0;
        virtual void setVideoCodecs(int codecMask) = 0;

        virtual bool registerUser(const std::string& deviceName, const std::string& user, const std::string& password, const std::string& domain) = 0;
        virtual bool disconnect() = 0;
        virtual std::string getPreferredDeviceName() = 0;
        virtual std::string getPreferredLineDN() = 0;
        virtual ConnectionStatusEnum::ConnectionStatus getConnectionStatus() = 0;
        virtual std::string getCurrentServer() = 0;

        /* P2P MODE */
        virtual bool startP2PMode(const std::string& user) = 0;

        /* SDP MODE */
        virtual bool startSDPMode() = 0;

        /**
         * Obtain the device object, from which call control can be done.
         * getAvailablePhoneDetails lists all known devices which the user is likely to be able to control.
         */
        virtual CC_DevicePtr getActiveDevice() = 0;
        virtual PhoneDetailsVtrPtr getAvailablePhoneDetails() = 0;
        virtual PhoneDetailsPtr getAvailablePhoneDetails(const std::string& deviceName) = 0;

        /**
         * Obtain the audio/video object, from which video setup can be done.
         * This relates to global tuning, device selection, preview window positioning, etc, not to
         * per-call settings or control.
         *
         * These objects are unavailable except while in softphone mode.
         */
        virtual VideoControlPtr getVideoControl() = 0;
        virtual AudioControlPtr getAudioControl() = 0;

        virtual bool setProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key, std::string& value) = 0;
        virtual std::string getProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key) = 0;

    protected:
        CallControlManager() {}
    private:
        CallControlManager(const CallControlManager&);
        CallControlManager& operator=(const CallControlManager&);
    };
} //end namespace CSF
