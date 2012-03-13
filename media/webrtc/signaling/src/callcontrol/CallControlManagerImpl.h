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

#include "CallControlManager.h"
#include "PhoneDetailsImpl.h"
#include "CC_SIPCCService.h"

#include "base/lock.h"


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

        // Local IP Address and DefaultGateway
        virtual void setLocalIpAddressAndGateway(const std::string& localIpAddress, const std::string& defaultGW);

        virtual AuthenticationStatusEnum::AuthenticationStatus getAuthenticationStatus();

        virtual bool registerUser( const std::string& deviceName, const std::string& user, const std::string& password, const std::string& domain );

        virtual bool startP2PMode(const std::string& user);

        virtual bool startROAPProxy( const std::string& deviceName, const std::string& user, const std::string& password, const std::string& domain );

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
		Lock m_lock;;
		std::set<CC_Observer *> ccObservers;
		std::set<ECC_Observer *> eccObservers;

        // Config and global setup
		std::string username;
		std::string password;
		std::string authString;
		std::string secureCachePath;
		bool multiClusterMode;
		cc_int32_t sipccLoggingMask;

        // Local IP Address
		std::string localIpAddress;
		std::string defaultGW;

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
		void onCallEvent    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info, char* sdp);

	private: //member functions

		// CC_Observers
		void notifyDeviceEventObservers  (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info);
		void notifyFeatureEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info);
		void notifyLineEventObservers    (ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info);
		void notifyCallEventObservers    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info, char* sdp);

		// ECC_Observers
		void notifyAvailablePhoneEvent (AvailablePhoneEventType::AvailablePhoneEvent event,
											const PhoneDetailsPtr phoneDetails);
		void notifyAuthenticationStatusChange (AuthenticationStatusEnum::AuthenticationStatus);
		void notifyConnectionStatusChange(ConnectionStatusEnum::ConnectionStatus status);
		void setConnectionState(ConnectionStatusEnum::ConnectionStatus status);
	};

}
