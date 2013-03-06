/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCC_SERVICE_H
#define _CC_SIPCC_SERVICE_H

#include "CC_Service.h"

extern "C" {
	// Callbacks from SIPCC.
    void configCtlFetchReq(int device_handle);
    char* platGetIPAddr();

    void CCAPI_DeviceListener_onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t hDevice, cc_deviceinfo_ref_t dev_info);
    void CCAPI_DeviceListener_onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t /* device_info */, cc_featureinfo_ref_t feature_info);
    void CCAPI_LineListener_onLineEvent(ccapi_line_event_e eventType, cc_lineid_t line, cc_lineinfo_ref_t info);
    void CCAPI_CallListener_onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info);
}

#include "VcmSIPCCBinding.h"
#include "CSFAudioControlWrapper.h"
#include "CSFVideoControlWrapper.h"
#include "CSFMediaProvider.h"

#include "base/lock.h"
#include "base/waitable_event.h"

#include <vector>
#include <set>
#include "mozilla/Mutex.h"

namespace CSF
{
    class PhoneConfig;
	DECLARE_NS_PTR(CC_SIPCCService);

	class CC_SIPCCService : public CC_Service, public StreamObserver, public MediaProviderObserver
    {
	    friend void ::configCtlFetchReq(int device_handle);
	    friend char* ::platGetIPAddr();

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
		virtual bool setSDPMode(bool mode);

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
        static void onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info);

	private: // Helper functions

        //These notify functions call through to CallControlManager
        void notifyDeviceEventObservers  (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info);
        void notifyFeatureEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info);
        void notifyCallEventObservers    (ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info);
        void notifyLineEventObservers    (ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info);

        void endAllActiveCalls();

        void applyLoggingMask(int newMask);
        void applyAudioVideoConfigSettings (PhoneConfig & phoneConfig);

        bool isValidMediaPortRange(int mediaStartPort, int mediaEndPort);
        bool isValidDSCPValue(int value);

    private: // Data Store
        // Singleton
        static CC_SIPCCService* _self;

	    std::string deviceName;
        cc_int32_t loggingMask;

        //IP Address Info
        std::string localAddress;
        std::string defaultGW;

	    // SIPCC lifecycle
        bool bCreated;
        bool bStarted;
        mozilla::Mutex m_lock;

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
