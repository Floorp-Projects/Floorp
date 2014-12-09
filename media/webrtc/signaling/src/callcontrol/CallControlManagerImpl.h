/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CallControlManager.h"
#include "PhoneDetailsImpl.h"
#include "CC_SIPCCService.h"
#include "mozilla/Mutex.h"


#include <set>
#include <map>

namespace CSF
{
	class CallControlManagerImpl: public CallControlManager, public CC_Observer
	{
	public:
		CallControlManagerImpl();
        virtual bool destroy();
		virtual ~CallControlManagerImpl();

		// Observers
        virtual void addCCObserver ( CC_Observer * observer );
        virtual void removeCCObserver ( CC_Observer * observer );

        virtual void addECCObserver ( ECC_Observer * observer );
        virtual void removeECCObserver ( ECC_Observer * observer );

        // Config and global setup
        virtual void setMultiClusterMode(bool allowMultipleClusters);
        virtual void setSIPCCLoggingMask(const cc_int32_t mask);
        virtual void setAuthenticationString(const std::string &authString);
        virtual void setSecureCachePath(const std::string &secureCachePath);

        // Add local codecs
        virtual void setAudioCodecs(int codecMask);
        virtual void setVideoCodecs(int codecMask);

        virtual AuthenticationStatusEnum::AuthenticationStatus getAuthenticationStatus();

        virtual bool registerUser( const std::string& deviceName, const std::string& user, const std::string& password, const std::string& domain );

        virtual bool startP2PMode(const std::string& user);

        virtual bool startSDPMode();

        virtual bool disconnect();
        virtual std::string getPreferredDeviceName();
        virtual std::string getPreferredLineDN();
        virtual ConnectionStatusEnum::ConnectionStatus getConnectionStatus();
        virtual std::string getCurrentServer();

        // Currently controlled device
        virtual CC_DevicePtr getActiveDevice();

        // All known devices
        virtual PhoneDetailsVtrPtr getAvailablePhoneDetails();
        virtual PhoneDetailsPtr getAvailablePhoneDetails(const std::string& deviceName);

        // Media setup
        virtual VideoControlPtr getVideoControl();
        virtual AudioControlPtr getAudioControl();

        virtual bool setProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key, std::string& value);
        virtual std::string getProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key);

	private: // Data Storage

        // Observers
		mozilla::Mutex m_lock;
		std::set<CC_Observer *> ccObservers;
		std::set<ECC_Observer *> eccObservers;

        // Config and global setup
		std::string username;
		std::string password;
		std::string authString;
		std::string secureCachePath;
		bool multiClusterMode;
		cc_int32_t sipccLoggingMask;

		AuthenticationStatusEnum::AuthenticationStatus authenticationStatus;

		std::string preferredDevice;
		std::string preferredLineDN;
		CC_ServicePtr phone;			// The generic handle, for simple operations.
		CC_SIPCCServicePtr softPhone;	// For setup operations not available on the generic API.

        // All known devices
		typedef std::map<std::string, PhoneDetailsImplPtr> PhoneDetailsMap;
		PhoneDetailsMap phoneDetailsMap;

		// store connection state
		ConnectionStatusEnum::ConnectionStatus connectionState;

	public: // Listeners for stacks controlled by CallControlManager
		// CC_Observers
		void onDeviceEvent  (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info);
		void onFeatureEvent (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info);
		void onLineEvent    (ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info);
		void onCallEvent    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info);

	private: //member functions

		// CC_Observers
		void notifyDeviceEventObservers  (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info);
		void notifyFeatureEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info);
		void notifyLineEventObservers    (ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info);
		void notifyCallEventObservers    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info);

		// ECC_Observers
		void notifyAvailablePhoneEvent (AvailablePhoneEventType::AvailablePhoneEvent event,
											const PhoneDetailsPtr phoneDetails);
		void notifyAuthenticationStatusChange (AuthenticationStatusEnum::AuthenticationStatus);
		void notifyConnectionStatusChange(ConnectionStatusEnum::ConnectionStatus status);
		void setConnectionState(ConnectionStatusEnum::ConnectionStatus status);
	};

}
