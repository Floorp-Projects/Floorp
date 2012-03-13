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

#ifdef _WIN32
#include <windows.h> //plat_api.h seems to need some of the types defined in Windows.h (e.g. boolean)
#endif

#include "CC_CallTypes.h"
#include "CC_SIPCCService.h"
#include "NullDeleter.h"
#include "CC_SIPCCDevice.h"
#include "CC_SIPCCDeviceInfo.h"
#include "CC_SIPCCFeatureInfo.h"
#include "CC_SIPCCCallServerInfo.h"
#include "CC_SIPCCCall.h"
#include "CC_SIPCCCallInfo.h"
#include "CC_SIPCCLine.h"
#include "CC_SIPCCLineInfo.h"
#include "CSFMediaProvider.h"
#include "CSFAudioTermination.h"
#include "CSFVideoTermination.h"

#include "base/platform_thread.h"
#include "base/time.h"

extern "C" {
#include "ccapi_device.h"
}
#include "debug-psipcc-types.h"
#include "VcmSIPCCBinding.h"

#include "csf_common.h"

#include "CSFLogStream.h"
static const char* logTag = "CC_SIPCCService";

using namespace std;

#define MAX_SUPPORTED_NUM_CALLS        100
#define MAX_SUPPORTED_NUM_LINES        100
#define MAX_SUPPORTED_NUM_FEATURES     100
#define MAX_SUPPORTED_NUM_CALL_SERVERS 100

extern "C"
{
#include "cpr_types.h"
#include "ccapi_device.h"
#include "ccapi_device_info.h"
#include "ccapi_call.h"

#include "cpr_stdio.h"
#include "config_api.h"
#include "ccapi_service.h"
#include "plat_api.h"

// Possibly these should be turned off for linux too
#ifdef WIN32
boolean cpr_memory_mgmt_pre_init(size_t size)
{
    return TRUE;
}

void debugCprMem(cc_debug_cpr_mem_options_e category, cc_debug_flag_e flag)
{
}

void debugClearCprMem(cc_debug_clear_cpr_options_e category)
{
}

void debugShowCprMem(cc_debug_show_cpr_options_e category)
{
}
#endif

/**
 * configCtlFetchReq
 * 
 * This function tells the config manager to fetch the CTL file 
 * and then fetch the config  from the CUCM. It is expected that 
 * this will result in processing of
 * the config file after the config managers response is received.
 *
 * The response received for this request is asynchronous and 
 * should be handled via event provided by config manager.
 * The CCAPI_Config_reponse api needs to be called for the 
 * handling of the response to the fetch request
 *
 */
void configCtlFetchReq(int device_handle)
{
    CSFLogDebugS(logTag, "In configCtlFetchReq");

    CSF::CC_SIPCCService * pPhone = CSF::CC_SIPCCService::_self;

    if (pPhone == NULL)
    {
        CSFLogErrorS( logTag, "CC_SIPCCService::_self is NULL.");
    }
    else
    {
    	if (pPhone->bUseConfig == true)
    		CCAPI_Config_response(device_handle, pPhone->deviceName.c_str(), pPhone->xmlConfig.c_str(), true);
    	else
    		CCAPI_Start_response(device_handle, pPhone->deviceName.c_str(), pPhone->sipUser.c_str(),
    							 	 pPhone->sipPassword.c_str(), pPhone->sipDomain.c_str());
    }
}

/**
 * configFetchReq
 * 
 * This function tells the config manager to fetch the latest config
 * from the CUCM. It is expected that this will result in processing of
 * the config file after the config managers response is received.
 *
 * The response received for this request is asynchronous and 
 * should be handled via event provided by config manager.
 * The CCAPI_Config_reponse api needs to be called for the 
 * handling of the response to the fetch request
 *
 * There are cases where only the config file is needed, for eg: during
 * Apply Config, for those situation we use this API
 *
 */
void configFetchReq(int device_handle)
{
    CSFLogDebugS( logTag, "In configFetchReq");

    configCtlFetchReq(device_handle);
}

/**
 * configParserError
 *
 * Notify the config manager that the config file has an error
 * and a new config file needs to be downloaded.
 *
 * The error could be XML format error or minimum config not being
 * present in the config file.  It is expected that 
 * this will result in processing of
 * the config file after the config managers response is received.
 *
 */
void configParserError(void)
{
    CSFLogErrorS( logTag, "In configParserError");
}

/**
 * Inform application that pSipcc stack has receive a NOTIFY message of event
 * type "service-control" and action=apply-config. The arguments passed to this
 * function contains the parameter values received in the message.
 *
 * @param config_version [in] latest version stamp of phone configuration.
 * @param dial_plan_version [in] latest version stamp of the dial plan.
 * @param fcp_version [in] latest version stamp of feature policy control.
 * @param cucm_result  [in] action taken by the cucm for the changes applied by the
 * user to the phone configuration. cucm result could be
 *  @li "no_change" - the new phone configuration changes do not impact Unified CM,
 *  @li "config_applied" - The Unified CM Administration applied the
 *  configuration changes dynamically without requiring phone to re-register,
 *  @li "reregister_needed" - The Unified CM Administration applied the
 *  configuration changes and required the phone to re-register to make them
 *  effective.
 * @param load_id [in] - firmware image name that phone should be running with
 * @param inactive_load_id [in] - firmware image name that phone should be in inactive partition.
 * @param load_server [in] - address of load server to download the  firmware
 * image load_id.
 * @param log_server [in] - log server address where error report need to be
 * sent. Error report, for example, could be related to image download failure
 * or phone configuration file download failure.
 * @param ppid [in] whether peer to peer upgrade is available
 * @return void
 */
void configApplyConfigNotify(cc_string_t config_version,
		cc_string_t dial_plan_version,
		cc_string_t fcp_version,
		cc_string_t cucm_result,
		cc_string_t load_id,
		cc_string_t inactive_load_id,
		cc_string_t load_server,
		cc_string_t log_server,
		cc_boolean ppid)
{
    CSFLogDebugS( logTag, "In configApplyConfigNotify");
}

char * platGetIPAddr ()
{
    CSFLogDebugS( logTag, "In platGetIPAddr()");

    CSF::CC_SIPCCService * pPhone = CSF::CC_SIPCCService::_self;

    if (pPhone == NULL)
    {
        CSFLogErrorS( logTag, "In platGetIPAddr(). CC_SIPCCService::_self is NULL.");
        return (char *) "";
    }

	return (char*) pPhone->localAddress.c_str();
}

/**
 * Called by SIPCC to get the default gateway. Determining the default gateway is platform specific.
 * In ECC we rely on the "NetworkMonitor" interface to provide this, ie the job of determining the
 * default gateway is not done in ECC iteself.
 *
 * @param *addr the pointer to the string holding default gw address (dhcp.xxxx.gateway)
 *        addr points to a buffer of size MAX_SIP_URL_LENGTH(512) bytes.
 * @return void
 */
#ifndef _WIN32
void platGetDefaultGW(char *addr)
{
    CSFLogDebugS( logTag, "In platGetDefaultGW()");

    if (addr == NULL)
    {
        CSFLogErrorS( logTag, "In platGetDefaultGW(). Passed NULL value for addr.");
        return;
    }

    CSF::CC_SIPCCService * pPhone = CSF::CC_SIPCCService::_self;

    if (pPhone == NULL)
    {
        CSFLogErrorS( logTag, "In platGetDefaultGW(). CC_SIPCCService::_self is NULL.");
        return;
    }

    csf_strcpy(addr, 512, pPhone->defaultGW.c_str());
}
#endif

void ccmedia_flash_once_timer_callback (void)
{
}

/**
 * dialPlanFetchReq
 * 
 * This function tells the get file request service to fetch the latest dial
 * plan from the CUCM.
 *
 * @param device_handle [in] handle of the device, the response is for
 * @param dialPlanFileName [in] the name of dialplan file to retrieve 
 *
 */
cc_boolean dialPlanFetchReq(int device_handle, char* dialPlanFileName)
{
    return 0;
}

/**
 * fcpFetchReq
 * 
 * This function tells the get file request service to fetch the latest fcp
 * file from the CUCM.
 *
 * @param device_handle [in] handle of the device, the response is for
 * @param dialPlanFileName [in] the name of fcp file to retrieve 
 *
 */
cc_boolean fcpFetchReq(int device_handle, char* fcpFileName)
{
    return 0;
}

extern cc_int32_t SipDebugMessage;
extern cc_int32_t SipDebugState;
extern cc_int32_t SipDebugTask;
extern cc_int32_t SipDebugRegState;
extern cc_int32_t GSMDebug;
extern cc_int32_t FIMDebug;
extern cc_int32_t LSMDebug;
extern cc_int32_t FSMDebugSM;
extern int32_t CSMDebugSM;
extern cc_int32_t CCDebug;
extern cc_int32_t CCDebugMsg;
extern cc_int32_t AuthDebug;
extern cc_int32_t ConfigDebug;
extern cc_int32_t DpintDebug;
extern cc_int32_t KpmlDebug;
extern cc_int32_t VCMDebug;
extern cc_int32_t g_CCAppDebug;
extern cc_int32_t g_CCLogDebug;
extern cc_int32_t TNPDebug;

static cc_int32_t * _maskedLoggingEntriesArray[19] = {
  &SipDebugMessage, //13
  &SipDebugState, //12
  &SipDebugTask, //11
  &SipDebugRegState, //14
  &GSMDebug, //7
  &FIMDebug, //4
  &LSMDebug, //8
  &FSMDebugSM, //5
  &CSMDebugSM, //??
  &CCDebug, //2
  &CCDebugMsg, //3
  &AuthDebug, //6
  &ConfigDebug, //0
  &DpintDebug, //18
  &KpmlDebug, //19
  &VCMDebug, //??
  &g_CCAppDebug, //0
  &g_CCLogDebug, //??
  &TNPDebug, //1
};

//Following are not covered above. :
//g_cacDebug,9
//g_dcsmDebug, 10
//SipDebugTrx, 14
//TMRDebug, 15
//SipDebugDM, 16
//g_DEFDebug, 17
//g_blfDebug, 21
//g_configappDebug, 23
//CCEVENTDebug, 24
//PLATDebug, 25

//If you add more to this array then you'll need to manually update "HAS_21_BITS" below to be "HAS_22_BITS"
//of whatever the new number of bits defined ends up as. Then modify the code that currently uses HAS_21_BITS
//below to use your new #define.
#define HAS_21_BITS 0x1FFFFF

static int _maxBitValueMaskedLoggingEntries = csf_countof(_maskedLoggingEntriesArray);//Should be 21


} //end extern C

extern "C" void CCAPI_DeviceListener_onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t hDevice, cc_deviceinfo_ref_t dev_info)
{
    //CSFLogDebugS( logTag, "In CCAPI_DeviceListener_onDeviceEvent");
    CSF::CC_SIPCCService::onDeviceEvent(type, hDevice, dev_info);
}

extern "C" void CCAPI_DeviceListener_onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t dev_info, cc_featureinfo_ref_t feature_info)
{
    //CSFLogDebugS( logTag, "In CCAPI_DeviceListener_onFeatureEvent");
    CSF::CC_SIPCCService::onFeatureEvent(type, dev_info, feature_info);
}

extern "C" void CCAPI_LineListener_onLineEvent(ccapi_line_event_e type, cc_lineid_t line, cc_lineinfo_ref_t info)
{
    //CSFLogDebugS( logTag, "In CCAPI_LineListener_onLineEvent");
    CSF::CC_SIPCCService::onLineEvent(type, line, info);
}

extern "C" void CCAPI_CallListener_onCallEvent(ccapi_call_event_e type, cc_call_handle_t handle, cc_callinfo_ref_t info, char* sdp)
{
    //CSFLogDebugS( logTag, "In CCAPI_CallListener_onCallEvent");
	CSF::CC_SIPCCService::onCallEvent(type, handle, info, sdp);
}



namespace CSF
{

CC_SIPCCService* CC_SIPCCService::_self = NULL;

CC_SIPCCService::CC_SIPCCService()
: loggingMask(0),
  bCreated(false),
  bStarted(false),  
  sippStartedEvent(false, false),
  vcmMediaBridge(MediaProvider::create()),
  bUseConfig(false)
{
	// Only one instance allowed!
    assert(_self == NULL);
    _self = this;
    vcmMediaBridge.setStreamObserver(this);
    vcmMediaBridge.setMediaProviderObserver(this);
}

CC_SIPCCService::~CC_SIPCCService()
{
	destroy();

    _self = NULL;
}

bool CC_SIPCCService::init(const std::string& user, const std::string& password, const std::string& domain, const std::string& device)
{
    sipUser = user;
    sipPassword = password;
    sipDomain = domain;
    deviceName = device;

    if (!(bCreated = (CCAPI_Service_create() == CC_SUCCESS)))
    {
        CSFLogErrorS( logTag, "Call to CCAPI_Service_create() failed.");
        return false;
    }
    return true;
}

void CC_SIPCCService::destroy()
{
	stop();

    if (bCreated)
    {
        if (CCAPI_Service_destroy() == CC_FAILURE)
        {
            CSFLogErrorS( logTag, "Call to CCAPI_Service_destroy() failed.");
        }

        bCreated = false;
    }

    xmlConfig = "";
	deviceName = "";
	loggingMask = 0;

    CC_SIPCCDevice::reset();
    CC_SIPCCDeviceInfo::reset();
    CC_SIPCCFeatureInfo::reset();
    CC_SIPCCCallServerInfo::reset();
    CC_SIPCCLine::reset();
    CC_SIPCCLineInfo::reset();
    CC_SIPCCCall::reset();
    CC_SIPCCCallInfo::reset();

	if(audioControlWrapper != NULL)
	{
		audioControlWrapper->setAudioControl(NULL);
	}
	if(videoControlWrapper != NULL)
	{
		videoControlWrapper->setVideoControl(NULL);
	}
}

void CC_SIPCCService::setConfig(const std::string& xmlConfig)
{
	this->xmlConfig = xmlConfig;
}

void CC_SIPCCService::setDeviceName(const std::string& deviceName)
{
	this->deviceName = deviceName;
}

void CC_SIPCCService::setLoggingMask(int mask)
{
	this->loggingMask = mask;
}

void CC_SIPCCService::setLocalAddressAndGateway(const std::string& localAddress,
                                                const std::string& defaultGW)
{
	this->localAddress = localAddress;
    this->defaultGW = defaultGW;

    CCAPI_Device_IP_Update(CCAPI_Device_getDeviceID(), localAddress.c_str(), "", 0,
                           localAddress.c_str(), "", 0);

	AudioTermination* audio = VcmSIPCCBinding::getAudioTermination();
	if(audio != NULL)
	{
		audio->setLocalIP(localAddress.c_str());
	}
	VideoTermination* video = VcmSIPCCBinding::getVideoTermination();
	if(video != NULL)
	{
		video->setLocalIP(localAddress.c_str());
	}
}

/*
 * New function to start sip stack without device file download.
 */
bool CC_SIPCCService::startService()
{
	AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
	VideoTermination * pVideo = VcmSIPCCBinding::getVideoTermination();

	if(pAudio != NULL)
	{
		pAudio->setMediaPorts(16384, 32766);
		pAudio->setDSCPValue(184);
    	pAudio->setVADEnabled(false);
	}

    if (pVideo != NULL)
    {
		pVideo->setDSCPValue(136);
    }

    bUseConfig = false;
    if (!(bStarted = (CCAPI_Service_start() == CC_SUCCESS)))
    {
        CSFLogErrorS( logTag, "Call to CCAPI_Service_start() failed.");
        return false;
    }

    CC_DevicePtr devicePtr = CC_SIPCCDevice::createDevice ();
    if (devicePtr == NULL)
    {
    	CSFLogWarnS( logTag, "stopping because createDevice failed");
    	stop();
        return false;
    }
    CSFLogDebugS( logTag, "About to imposeLoggingMask");
    applyLoggingMask(loggingMask);

    return waitUntilSIPCCFullyStarted();
}


void CC_SIPCCService::stop()
{
    if (bStarted)
    {
    	// But first, tear down all existing calls.
    	endAllActiveCalls();

        if (CCAPI_Service_stop() == CC_FAILURE)
        {
            CSFLogErrorS( logTag, "Call to CCAPI_Service_stop() failed.");
        }

        bStarted = false;
    }
}

bool CC_SIPCCService::isStarted()
{
	return bStarted;
}

CC_DevicePtr CC_SIPCCService::getActiveDevice()
{
    return CC_SIPCCDevice::wrap(CCAPI_Device_getDeviceID());
}

vector<CC_DevicePtr> CC_SIPCCService::getDevices()
{
	vector<CC_DevicePtr> devices;

	CC_SIPCCDevicePtr pDevice = CC_SIPCCDevice::wrap(CCAPI_Device_getDeviceID());
	if(pDevice != NULL)
	{
        devices.push_back(pDevice);
    }

    return devices;
}

AudioControlPtr CC_SIPCCService::getAudioControl ()
{
	if(audioControlWrapper != NULL)
	{
		return audioControlWrapper;
	}
	else
	{
		audioControlWrapper = AudioControlWrapperPtr(new AudioControlWrapper(VcmSIPCCBinding::getAudioControl()));
		return audioControlWrapper;
	}
}

VideoControlPtr CC_SIPCCService::getVideoControl ()
{
	if(videoControlWrapper != NULL)
	{
		return videoControlWrapper;
	}
	else
	{
		videoControlWrapper = VideoControlWrapperPtr(new VideoControlWrapper(VcmSIPCCBinding::getVideoControl()));
		return videoControlWrapper;
	}
}


// Private Helpers

bool CC_SIPCCService::waitUntilSIPCCFullyStarted ()
{
    bool bStarted = false;

    CSFLogDebugS( logTag, "Waiting for SIPCC Registration to complete...");

    if (sippStartedEvent.TimedWait(base::TimeDelta::FromMilliseconds(20000)))
    {
    	CSFLogDebugS( logTag, "SIPCC Service has entered CC_STATE_INS State.");
        bStarted = true;
    }
    else
    {
    	CSFLogErrorS( logTag, "Timed out waiting for SIPCC Service to start.");
    }

    return bStarted;
}

void CC_SIPCCService::applyLoggingMask (int newMask)
{
    if (newMask >> _maxBitValueMaskedLoggingEntries > 0)
    {
    	CSFLogWarn( logTag, "Value of 0x%x specified for mask includes at least one bit value that exceeds the maximum supported bitfield value. "
                    "Ignoring unsupported bits.", newMask);
    }
    
    CSFLogDebugS( logTag, "Applying a sipcc log mask = " << newMask);

    loggingMask = newMask & (HAS_21_BITS);

    for (int i=0; i<_maxBitValueMaskedLoggingEntries; i++)
    {
        *(_maskedLoggingEntriesArray[i]) = (loggingMask >> i) & 0x1;
    }
}

void CC_SIPCCService::signalToPhoneWhenInService (ccapi_device_event_e type, cc_deviceinfo_ref_t info)
{
    if (type == CCAPI_DEVICE_EV_STATE)
    {
        cc_service_state_t serviceState = CCAPI_DeviceInfo_getServiceState(info);

        if (serviceState == CC_STATE_INS)
        {
        	sippStartedEvent.Signal();
        }
        else if  (serviceState == CC_STATE_OOS)
        {
        	CSFLogDebugS( logTag, "CC_STATE_OOS while starting up - ignoring ");
        }
    }
}

void CC_SIPCCService::endAllActiveCalls()
{
	CC_DevicePtr device = getActiveDevice();
	if(device != NULL)
	{
		CC_DeviceInfoPtr deviceInfo = device->getDeviceInfo();
		vector<CC_CallPtr> calls = deviceInfo->getCalls();
		CSFLogInfo( logTag, "endAllActiveCalls(): %d calls to be ended.", calls.size());
		for(vector<CC_CallPtr>::iterator it = calls.begin(); it != calls.end(); it++)
		{
			// For each active call, if it can be ended, do so.
			CC_CallPtr call = *it;
			CC_CallInfoPtr callInfo = call->getCallInfo();
			if(callInfo->hasCapability(CC_CallCapabilityEnum::canEndCall))
			{
				CSFLogDebugS( logTag, "endAllActiveCalls(): ending call " <<
						callInfo->getCallingPartyNumber() << " -> " << callInfo->getCalledPartyNumber() <<
						" [" << call_state_getname(callInfo->getCallState()) << "]");
				call->endCall();
			}
			else if(callInfo->hasCapability(CC_CallCapabilityEnum::canResume) && callInfo->getCallState() != REMHOLD)
			{
				CSFLogDebugS( logTag, "endAllActiveCalls(): resume then ending call " <<
						callInfo->getCallingPartyNumber() << " -> " << callInfo->getCalledPartyNumber() <<
						" [" << call_state_getname(callInfo->getCallState()) << "]");
				call->muteAudio();
				call->resume(callInfo->getVideoDirection());
				call->endCall();
			}
		}

		if(!calls.empty())
		{
			// If we had any calls, allow a short time for the SIP messaging to go out
			PlatformThread::Sleep(500);
		}
    }
}


// C++ Event Handlers
void CC_SIPCCService::onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t handle, cc_deviceinfo_ref_t info)
{
    if (_self == NULL)
    {
        CSFLogErrorS( logTag, "CC_SIPCCService::_self is NULL. Unable to notify observers of device event.");
        return;
    }

    CC_SIPCCDevicePtr devicePtr = CC_SIPCCDevice::wrap(handle);
    if (devicePtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify device observers for device handle (%u), as failed to create CC_DevicePtr", handle);
        return;
    }

    CC_SIPCCDeviceInfoPtr infoPtr = CC_SIPCCDeviceInfo::wrap(info);
    if (infoPtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify call observers for device handle (%u), as failed to create CC_DeviceInfoPtr", handle);
        return;
    }

    CSFLogInfoS( logTag, "onDeviceEvent(" << device_event_getname(type) << ", " << devicePtr->toString() <<
    		", [" << infoPtr->getDeviceName() << "] )");
    _self->notifyDeviceEventObservers(type, devicePtr, infoPtr);

    _self->signalToPhoneWhenInService(type, info);
}

void CC_SIPCCService::onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t /* device_info */, cc_featureinfo_ref_t feature_info)
{

    if (_self == NULL)
     {
         CSFLogErrorS( logTag, "CC_SIPCCService::_self is NULL. Unable to notify observers of device event.");
         return;
     }

     cc_device_handle_t hDevice = CCAPI_Device_getDeviceID();
     CC_DevicePtr devicePtr = CC_SIPCCDevice::wrap(hDevice);
     if (devicePtr == NULL)
     {
         CSFLogError( logTag, "Unable to notify device observers for device handle (%u), as failed to create CC_DevicePtr", hDevice);
         return;
     }

     CC_FeatureInfoPtr infoPtr = CC_SIPCCFeatureInfo::wrap(feature_info);
     if (infoPtr  == NULL)
     {
         CSFLogError( logTag, "Unable to notify call observers for feature info handle (%u), as failed to create CC_FeatureInfoPtr", feature_info);
         return;
     }

     CSFLogInfoS( logTag, "onFeatureEvent(" << device_event_getname(type) << ", " << devicePtr->toString() <<
    		 ", [" << infoPtr->getDisplayName() << "] )");
     _self->notifyFeatureEventObservers(type, devicePtr, infoPtr);
}

void CC_SIPCCService::onLineEvent(ccapi_line_event_e eventType, cc_lineid_t line, cc_lineinfo_ref_t info)
{
    if (_self == NULL)
    {
        CSFLogErrorS( logTag, "CC_SIPCCService::_self is NULL. Unable to notify observers of line event.");
        return;
    }

    CC_LinePtr linePtr = CC_SIPCCLine::wrap(line);
    if (linePtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify line observers for line lineId (%u), as failed to create CC_LinePtr", line);
        return;
    }

    CC_LineInfoPtr infoPtr = CC_SIPCCLineInfo::wrap(info);
    if (infoPtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify line observers for line lineId (%u), as failed to create CC_LineInfoPtr", line);
        return;
    }

    CSFLogInfoS( logTag, "onLineEvent(" << line_event_getname(eventType) << ", " << linePtr->toString() <<
    		", [" << infoPtr->getNumber() << "|" << (infoPtr->getRegState() ? "INS" : "OOS") << "] )");
    _self->notifyLineEventObservers(eventType, linePtr, infoPtr);
}

void CC_SIPCCService::onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info, char* sdp)
{
    if (_self == NULL)
    {
        CSFLogErrorS( logTag, "CC_SIPCCService::_self is NULL. Unable to notify observers of call event.");
        return;
    }

    CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(handle);
    if (callPtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify call observers for call handle (%u), as failed to create CC_CallPtr", handle);
        return;
    }

    CC_SIPCCCallInfoPtr infoPtr = CC_SIPCCCallInfo::wrap(info);
    if (infoPtr == NULL)
    {
        CSFLogError( logTag, "Unable to notify call observers for call handle (%u), as failed to create CC_CallInfoPtr", handle);
        return;
    }

    infoPtr->setMediaData(callPtr->getMediaData());

	set<CSF::CC_CallCapabilityEnum::CC_CallCapability> capSet = infoPtr->getCapabilitySet();
    CSFLogInfoS( logTag, "onCallEvent(" << call_event_getname(eventType) << ", " << callPtr->toString() <<
    		", [" << call_state_getname(infoPtr->getCallState()) << "|" << CC_CallCapabilityEnum::toString(capSet) << "] )");
    _self->notifyCallEventObservers(eventType, callPtr, infoPtr, sdp);
}

void CC_SIPCCService::addCCObserver ( CC_Observer * observer )
{
	AutoLock lock(m_lock);
    if (observer == NULL)
    {
        CSFLogErrorS( logTag, "NULL value for \"observer\" passed to addCCObserver().");
        return;
    }

    ccObservers.insert(observer);
}

void CC_SIPCCService::removeCCObserver ( CC_Observer * observer )
{
	AutoLock lock(m_lock);
    ccObservers.erase(observer);
}

//Notify Observers
void CC_SIPCCService::notifyDeviceEventObservers (ccapi_device_event_e eventType, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info)
{
	AutoLock lock(m_lock);
	set<CC_Observer*>::const_iterator it = ccObservers.begin();
	for ( ; it != ccObservers.end(); it++ )
    {
	    (*it)->onDeviceEvent(eventType, devicePtr, info);
    }
}

void CC_SIPCCService::notifyFeatureEventObservers (ccapi_device_event_e eventType, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info)
{
	AutoLock lock(m_lock);
	set<CC_Observer*>::const_iterator it = ccObservers.begin();
	for ( ; it != ccObservers.end(); it++ )
    {
	    (*it)->onFeatureEvent(eventType, devicePtr, info);
    }
}

void CC_SIPCCService::notifyLineEventObservers (ccapi_line_event_e eventType, CC_LinePtr linePtr, CC_LineInfoPtr info)
{
	AutoLock lock(m_lock);
	set<CC_Observer*>::const_iterator it = ccObservers.begin();
	for ( ; it != ccObservers.end(); it++ )
    {
	    (*it)->onLineEvent(eventType, linePtr, info);
    }
}

void CC_SIPCCService::notifyCallEventObservers (ccapi_call_event_e eventType, CC_CallPtr callPtr, CC_CallInfoPtr info, char* sdp)
{
	AutoLock lock(m_lock);
	set<CC_Observer*>::const_iterator it = ccObservers.begin();
	for ( ; it != ccObservers.end(); it++ )
    {
	    (*it)->onCallEvent(eventType, callPtr, info, sdp);
    }
}

// This is called when the SIP stack has caused a new stream to be allocated. This function will
// find the call associated with that stream so that the call can store the streamId.
void CC_SIPCCService::registerStream(cc_call_handle_t call, int streamId, bool isVideo)
{
    CSFLogDebugS( logTag, "registerStream for call: " << call << " strId=" << streamId << " video=" << isVideo);
	// get the object corresponding to the handle
    CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(call);
    if (callPtr != NULL)
    {
    	callPtr->addStream(streamId, isVideo);
    }
    else
    {
        CSFLogErrorS( logTag, "registerStream(), No call found for allocated Stream:" << streamId << ", " << isVideo);
    }
}

void CC_SIPCCService::deregisterStream(cc_call_handle_t call, int streamId)
{
	// get the object corresponding to the handle
    CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(call);
    if (callPtr != NULL)
    {
    	callPtr->removeStream(streamId);
    }
    else
    {
        CSFLogErrorS( logTag, "deregisterStream(), No call found for deallocated Stream:" << streamId);
    }
}

void CC_SIPCCService::dtmfBurst(int digit, int direction, int duration)
{
	// We haven't a clue what stream to use.  Search for a call which has an audio stream.
	vector<CC_SIPCCCallPtr> calls;
	{
		// First build a list of all the calls.
		cc_deviceinfo_ref_t deviceInfoRef = CCAPI_Device_getDeviceInfo(CCAPI_Device_getDeviceID());
		cc_call_handle_t handles[MAX_SUPPORTED_NUM_CALLS] = {};
		cc_uint16_t numHandles = csf_countof(handles);

		CCAPI_DeviceInfo_getCalls(deviceInfoRef, handles, &numHandles);

		for (int i=0; i<numHandles; i++)
		{
			CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(handles[i]);
			calls.push_back(callPtr);
		}
		CCAPI_Device_releaseDeviceInfo(deviceInfoRef);
	}

	// Using the list of all calls, search for those containing an audio stream.
	// Send a DTMF digit on the first one we find.

	AudioTermination * pAudio = VcmSIPCCBinding::getAudioTermination();
	bool bSent = false;
	for(vector<CC_SIPCCCallPtr>::iterator it = calls.begin(); it != calls.end() && !bSent; it++)
    {
    	CC_SIPCCCallMediaDataPtr pMediaData = (*it)->getMediaData();

    	AutoLock lock(pMediaData->streamMapMutex);
		for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
	    {
			if (entry->second.isVideo == false)
			{
				// first is the streamId
				if (pAudio->sendDtmf(entry->first, digit))
				{
					// We have sent a digit, done.
					bSent = true;
					break;
				}
				else
				{
					CSFLogWarnS( logTag, "dtmfBurst:sendDtmf returned fail");
				}
			}
	    }
    }
}

void CC_SIPCCService::sendIFrame(cc_call_handle_t call_handle)
{
	CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(call_handle);
    CC_SIPCCCallMediaDataPtr pMediaData=callPtr->getMediaData();
    if (pMediaData != NULL )
    {
        for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
        {
     		if (entry->second.isVideo == true)
            {
                VcmSIPCCBinding::getVideoTermination()->sendIFrame( entry->first );
            }
        }
    }
}

bool CC_SIPCCService::isValidMediaPortRange(int mediaStartPort, int mediaEndPort)
{
    if(mediaStartPort < 1024  ||
        mediaStartPort > 65535  ||
    	mediaEndPort < 1024 ||
    	mediaEndPort > 65535 ||
    	mediaEndPort - mediaStartPort < 3)
    {
    		return false;
    }
    else
    {
    	return true;
    }
}

bool CC_SIPCCService::isValidDSCPValue(int value)
{
	//value is 8-bit value, but the two least significant bits are unused,
	//leaving an effective 6 bits of information.  Valid values are therefore
	//all multiples of 4 that lie between 0 and 252 inclusive.
	if(value >= 0 &&
		value <= 252 &&
		value % 4 == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CC_SIPCCService::onVideoModeChanged( bool enable )
{
}

void CC_SIPCCService::onKeyFrameRequested( int stream )
// This is called when the Video Provider indicates that it needs to send a request for new key frame to the sender
{
    CSFLogDebugS(logTag, "onKeyFrameRequested for stream ");
	// We haven't a clue what stream to use.  Search for a call which has an audio stream.
	vector<CC_SIPCCCallPtr> calls;
	{
		// First build a list of all the calls.
		cc_deviceinfo_ref_t deviceInfoRef = CCAPI_Device_getDeviceInfo(CCAPI_Device_getDeviceID());
		cc_call_handle_t handles[MAX_SUPPORTED_NUM_CALLS] = {};
		cc_uint16_t numHandles = csf_countof(handles);

		CCAPI_DeviceInfo_getCalls(deviceInfoRef, handles, &numHandles);

		for (int i=0; i<numHandles; i++)
		{
			CC_SIPCCCallPtr callPtr = CC_SIPCCCall::wrap(handles[i]);
			//CC_SIPCCCallPtr callPtr = wrapCall(handles[i]);
			calls.push_back(callPtr);
		}
		CCAPI_Device_releaseDeviceInfo(deviceInfoRef);
	}

	// Using the list of all calls, search for those containing an video stream.
	// Send the send info SIP message when we find the one with the correct stream id.

	bool bSent = false;
	for(vector<CC_SIPCCCallPtr>::iterator it = calls.begin(); it != calls.end() && !bSent; it++)
    {
    	CC_SIPCCCallMediaDataPtr pMediaData = (*it)->getMediaData();

    	AutoLock lock(pMediaData->streamMapMutex);
		for (StreamMapType::iterator entry =  pMediaData->streamMap.begin(); entry !=  pMediaData->streamMap.end(); entry++)
	    {
			if ((entry->first==stream) && (entry->second.isVideo == true))
			{
                CSFLogDebugS(logTag, "Send SIP message to originator for stream id" << stream);
				if ((*it)->sendInfo ( "","application/media_control+xml", "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
						"<media_control>\n"
						"\n"
						"  <vc_primitive>\n"
						"    <to_encoder>\n"
						"      <picture_fast_update/>\n"
						"    </to_encoder>\n"
						"  </vc_primitive>\n"
						"\n"
						"</media_control>\n"))
				{
					CSFLogWarnS(logTag, "sendinfo returned true");
					bSent = true;
					break;
				}
				else
				{
					CSFLogWarnS(logTag, "sendinfo returned false");
				}
			}
	    }
    }
}

void CC_SIPCCService::onMediaLost( int callId )
{
}

void CC_SIPCCService::onMediaRestored( int callId )
{
}

bool CC_SIPCCService::setLocalVoipPort(int port) {
	return CCAPI_Config_set_local_voip_port(port);
}

bool CC_SIPCCService::setRemoteVoipPort(int port) {
	return CCAPI_Config_set_remote_voip_port(port);
}

bool CC_SIPCCService::setP2PMode(bool mode)  {
	return CCAPI_Config_set_p2p_mode(mode);
}

bool CC_SIPCCService::setROAPProxyMode(bool mode)  {
	return CCAPI_Config_set_roap_proxy_mode(mode);
}

bool CC_SIPCCService::setROAPClientMode(bool mode)  {
	return CCAPI_Config_set_roap_client_mode(mode);
}

} // End of namespace CSF
