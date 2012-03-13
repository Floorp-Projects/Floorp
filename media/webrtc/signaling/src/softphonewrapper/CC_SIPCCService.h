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

#ifndef _CC_SIPCC_SERVICE_H
#define _CC_SIPCC_SERVICE_H

#include "CC_Service.h"

extern "C" {
	// Callbacks from SIPCC.
    void configCtlFetchReq(int device_handle);
    char* platGetIPAddr();
#ifndef _WIN32
    void platGetDefaultGW(char *addr);
#endif
  
    void CCAPI_DeviceListener_onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t hDevice, cc_deviceinfo_ref_t dev_info);
    void CCAPI_DeviceListener_onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t /* device_info */, cc_featureinfo_ref_t feature_info);
    void CCAPI_LineListener_onLineEvent(ccapi_line_event_e eventType, cc_lineid_t line, cc_lineinfo_ref_t info);
    void CCAPI_CallListener_onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info, char* sdp);
}

#include "VcmSIPCCBinding.h"
#include "CSFAudioControlWrapper.h"
#include "CSFVideoControlWrapper.h"
#include "CSFMediaProvider.h"

#include "base/lock.h"
#include "base/waitable_event.h"

#include <vector>
#include <set>

namespace CSF
{
    class PhoneConfig;
	DECLARE_PTR(CC_SIPCCService);

	class CC_SIPCCService : public CC_Service, public StreamObserver, public MediaProviderObserver
    {
	    friend void ::configCtlFetchReq(int device_handle);
	    friend char* ::platGetIPAddr();
#ifndef _WIN32
      friend void ::platGetDefaultGW(char *addr);
#endif
      
	public:
	    CC_SIPCCService();
	    virtual ~CC_SIPCCService();

	    /**
	     * Public API
	     */
	    virtual void addCCObserver ( CC_Observer * observer );
	    virtual void removeCCObserver ( CC_Observer * observer );

	    virtual bool init(const std::string& user, const std::string& password, const std::string& domain, const std::string& deviceName);
	    virtual void destroy();

	    virtual void setConfig(const std::string& xmlConfig);
	    virtual void setDeviceName(const std::string& deviceName);
	    virtual void setLoggingMask(int mask);
	    virtual void setLocalAddressAndGateway(const std::string& localAddress, const std::string& defaultGW);

	    virtual bool startService();
	    virtual void stop();

	    virtual bool isStarted();

	    virtual CC_DevicePtr getActiveDevice();
	    virtual std::vector<CC_DevicePtr> getDevices();

	    virtual AudioControlPtr getAudioControl();
	    virtual VideoControlPtr getVideoControl();
	    
	    // From the StreamObserver interface
	    virtual void registerStream(cc_call_handle_t call, int streamId, bool isVideo);
    	virtual void deregisterStream(cc_call_handle_t call, int streamId);
    	virtual void dtmfBurst(int digit, int direction, int duration);
    	virtual void sendIFrame(cc_call_handle_t call);

		virtual void onVideoModeChanged( bool enable );
		virtual void onKeyFrameRequested( int stream );
		virtual void onMediaLost( int callId );
		virtual void onMediaRestored( int callId );

		virtual bool setLocalVoipPort(int port);
		virtual bool setRemoteVoipPort(int port);
		virtual bool setP2PMode(bool mode);
		virtual bool setROAPProxyMode(bool mode);
		virtual bool setROAPClientMode(bool mode);

        /**
         * End of public API
         */

    public:
        // These are used by the C callback functions to raise events.
        // This layer:
        //   - converts the C handles to C++ objects
        //   - invokes the member notify* functions to propagate events upwards
        //   - invokes any other functions needed to perform "value added" event handling in ECC.
        static void onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t hDevice, cc_deviceinfo_ref_t dev_info);
        static void onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t /* device_info */, cc_featureinfo_ref_t feature_info);
        static void onLineEvent(ccapi_line_event_e eventType, cc_lineid_t line, cc_lineinfo_ref_t info);
        static void onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info, char* sdp);

	private: // Helper functions

        //These notify functions call through to CallControlManager
        void notifyDeviceEventObservers  (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info);
        void notifyFeatureEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info);
        void notifyCallEventObservers    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info, char* sdp);
        void notifyLineEventObservers    (ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info);

        bool waitUntilSIPCCFullyStarted();
        void signalToPhoneWhenInService (ccapi_device_event_e type, cc_deviceinfo_ref_t info);
        void endAllActiveCalls();

        void applyLoggingMask(int newMask);
        void applyAudioVideoConfigSettings (PhoneConfig & phoneConfig);

        bool isValidMediaPortRange(int mediaStartPort, int mediaEndPort);
        bool isValidDSCPValue(int value);

    private: // Data Store
        // Singleton
        static CC_SIPCCService* _self;

	    // Config
	    std::string xmlConfig;
	    std::string deviceName;
        cc_int32_t loggingMask;

        //IP Addres Info
        std::string localAddress;
        std::string defaultGW;

	    // SIPCC lifecycle
        bool bCreated;
        bool bStarted;
        Lock m_lock;
        base::WaitableEvent sippStartedEvent;

        // Media Lifecycle
        VcmSIPCCBinding vcmMediaBridge;

        // Observers
    	std::set<CC_Observer *> ccObservers;

		//AV Control Wrappers
		AudioControlWrapperPtr audioControlWrapper;
		VideoControlWrapperPtr videoControlWrapper;

		// Santization work
		bool bUseConfig;
		std::string sipUser;
		std::string sipPassword;
		std::string sipDomain;
    };
}

#endif
