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

#include <errno.h>
#include <string>

#include "CC_SIPCCDevice.h"
#include "CC_SIPCCDeviceInfo.h"
#include "CC_SIPCCFeatureInfo.h"
#include "CC_SIPCCLine.h"
#include "CC_SIPCCLineInfo.h"
#include "CC_SIPCCCallInfo.h"
#include "CallControlManagerImpl.h"
#include "CSFLogStream.h"
#include "csf_common.h"

extern "C"
{
#include "config_api.h"
}

static const char* logTag = "CallControlManager";

static std::string logDestination = "CallControl.log";

using namespace std;
using namespace CSFUnified;

namespace CSF
{


CallControlManagerImpl::CallControlManagerImpl()
: m_lock("CallControlManagerImpl"),
  multiClusterMode(false),
  sipccLoggingMask(0),
  authenticationStatus(AuthenticationStatusEnum::eNotAuthenticated),
  connectionState(ConnectionStatusEnum::eIdle)
{
    CSFLogInfoS(logTag, "CallControlManagerImpl()");
}

CallControlManagerImpl::~CallControlManagerImpl()
{
    CSFLogInfoS(logTag, "~CallControlManagerImpl()");
    destroy();
}

bool CallControlManagerImpl::destroy()
{
    CSFLogInfoS(logTag, "destroy()");
    bool retval = disconnect();
    if(retval == false)
	{
		return retval;
	}
	return retval;
}

// Observers
void CallControlManagerImpl::addCCObserver ( CC_Observer * observer )
{
	mozilla::MutexAutoLock lock(m_lock);
    if (observer == NULL)
    {
        CSFLogErrorS(logTag, "NULL value for \"observer\" passed to addCCObserver().");
        return;
    }

    ccObservers.insert(observer);
}

void CallControlManagerImpl::removeCCObserver ( CC_Observer * observer )
{
	mozilla::MutexAutoLock lock(m_lock);
    ccObservers.erase(observer);
}

void CallControlManagerImpl::addECCObserver ( ECC_Observer * observer )
{
	mozilla::MutexAutoLock lock(m_lock);
    if (observer == NULL)
    {
        CSFLogErrorS(logTag, "NULL value for \"observer\" passed to addECCObserver().");
        return;
    }

    eccObservers.insert(observer);
}

void CallControlManagerImpl::removeECCObserver ( ECC_Observer * observer )
{
	mozilla::MutexAutoLock lock(m_lock);
    eccObservers.erase(observer);
}

void CallControlManagerImpl::setMultiClusterMode(bool allowMultipleClusters)
{
    CSFLogInfoS(logTag, "setMultiClusterMode(" << allowMultipleClusters << ")");
    multiClusterMode = allowMultipleClusters;
}

void CallControlManagerImpl::setSIPCCLoggingMask(const cc_int32_t mask)
{
    CSFLogInfoS(logTag, "setSIPCCLoggingMask(" << mask << ")");
    sipccLoggingMask = mask;
}

void CallControlManagerImpl::setAuthenticationString(const std::string &authString)
{
    CSFLogInfoS(logTag, "setAuthenticationString()");
    this->authString = authString;
}

void CallControlManagerImpl::setSecureCachePath(const std::string &secureCachePath)
{
    CSFLogInfoS(logTag, "setSecureCachePath(" << secureCachePath << ")");
    this->secureCachePath = secureCachePath;
}

// Add local codecs
void CallControlManagerImpl::setAudioCodecs(int codecMask)
{
  CSFLogDebug(logTag, "setAudioCodecs %X", codecMask); 

  VcmSIPCCBinding::setAudioCodecs(codecMask);
}

void CallControlManagerImpl::setVideoCodecs(int codecMask)
{
  CSFLogDebug(logTag, "setVideoCodecs %X", codecMask); 

  VcmSIPCCBinding::setVideoCodecs(codecMask);
}

AuthenticationStatusEnum::AuthenticationStatus CallControlManagerImpl::getAuthenticationStatus()
{
    return authenticationStatus;
}

bool CallControlManagerImpl::registerUser( const std::string& deviceName, const std::string& user, const std::string& password, const std::string& domain )
{
	setConnectionState(ConnectionStatusEnum::eRegistering);

    CSFLogInfoS(logTag, "registerUser(" << user << ", " << domain << " )");
    if(phone != NULL)
    {
    	setConnectionState(ConnectionStatusEnum::eReady);

        CSFLogErrorS(logTag, "registerUser() failed - already connected!");
        return false;
    }

    softPhone = CC_SIPCCServicePtr(new CC_SIPCCService());
    phone = softPhone;
    phone->init(user, password, domain, deviceName);
    softPhone->setLoggingMask(sipccLoggingMask);
    phone->addCCObserver(this);

    phone->setP2PMode(false);

    bool bStarted = phone->startService();
    if (!bStarted) {
        setConnectionState(ConnectionStatusEnum::eFailed);
    } else {
        setConnectionState(ConnectionStatusEnum::eReady);
    }

    return bStarted;
}

bool CallControlManagerImpl::startP2PMode(const std::string& user)
{
	setConnectionState(ConnectionStatusEnum::eRegistering);

    CSFLogInfoS(logTag, "startP2PMode(" << user << " )");
    if(phone != NULL)
    {
    	setConnectionState(ConnectionStatusEnum::eReady);

        CSFLogErrorS(logTag, "startP2PMode() failed - already started in p2p mode!");
        return false;
    }

    softPhone = CC_SIPCCServicePtr(new CC_SIPCCService());
    phone = softPhone;
    phone->init(user, "", "127.0.0.1", "sipdevice");
    softPhone->setLoggingMask(sipccLoggingMask);
    phone->addCCObserver(this);

    phone->setP2PMode(true);

    bool bStarted = phone->startService();
    if (!bStarted) {
        setConnectionState(ConnectionStatusEnum::eFailed);
    } else {
        setConnectionState(ConnectionStatusEnum::eReady);
    }

    return bStarted;
}

bool CallControlManagerImpl::startSDPMode()
{
    CSFLogInfoS(logTag, "startSDPMode");
    if(phone != NULL)
    {
        CSFLogError(logTag, "%s failed - already started in SDP mode!",__FUNCTION__);
        return false;
    }

    softPhone = CC_SIPCCServicePtr(new CC_SIPCCService());
    phone = softPhone;
    phone->init("JSEP", "", "127.0.0.1", "sipdevice");
    softPhone->setLoggingMask(sipccLoggingMask);
    phone->addCCObserver(this);
    phone->setSDPMode(true);
 
    return phone->startService();
}

bool CallControlManagerImpl::disconnect()
{
    CSFLogInfoS(logTag, "disconnect()");
    if(phone == NULL)
        return true;

    connectionState = ConnectionStatusEnum::eIdle;
    phone->removeCCObserver(this);
    phone->stop();
    phone->destroy();
    phone.reset();
    softPhone.reset();

    return true;
}

std::string CallControlManagerImpl::getPreferredDeviceName()
{
    return preferredDevice;
}

std::string CallControlManagerImpl::getPreferredLineDN()
{
    return preferredLineDN;
}

ConnectionStatusEnum::ConnectionStatus CallControlManagerImpl::getConnectionStatus()
{
    return connectionState;
}

std::string CallControlManagerImpl::getCurrentServer()
{
    return "";
}

// Currently controlled device
CC_DevicePtr CallControlManagerImpl::getActiveDevice()
{
    if(phone != NULL)
        return phone->getActiveDevice();

    return CC_DevicePtr();
}

// All known devices
PhoneDetailsVtrPtr CallControlManagerImpl::getAvailablePhoneDetails()
{
  PhoneDetailsVtrPtr result = PhoneDetailsVtrPtr(new PhoneDetailsVtr());
  for(PhoneDetailsMap::iterator it = phoneDetailsMap.begin(); it != phoneDetailsMap.end(); it++)
  {
    PhoneDetailsPtr details = it->second;
    result->push_back(details);
  }
  return result;
}

PhoneDetailsPtr CallControlManagerImpl::getAvailablePhoneDetails(const std::string& deviceName)
{
    PhoneDetailsMap::iterator it = phoneDetailsMap.find(deviceName);
    if(it != phoneDetailsMap.end())
    {
        return it->second;
    }
    return PhoneDetailsPtr();
}
// Media setup
VideoControlPtr CallControlManagerImpl::getVideoControl()
{
    if(phone != NULL)
        return phone->getVideoControl();

    return VideoControlPtr();
}

AudioControlPtr CallControlManagerImpl::getAudioControl()
{
    if(phone != NULL)
        return phone->getAudioControl();

    return AudioControlPtr();
}

bool CallControlManagerImpl::setProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key, std::string& value)
{
  unsigned long strtoul_result;
  char *strtoul_end;

  CSFLogInfoS(logTag, "setProperty(" << value << " )");

  if (key == ConfigPropertyKeysEnum::eLocalVoipPort) {
    errno = 0;
    strtoul_result = strtoul(value.c_str(), &strtoul_end, 10);

    if (errno || value.c_str() == strtoul_end || strtoul_result > USHRT_MAX) {
      return false;
    }

    CCAPI_Config_set_local_voip_port((int) strtoul_result);
  } else if (key == ConfigPropertyKeysEnum::eRemoteVoipPort) {
    errno = 0;
    strtoul_result = strtoul(value.c_str(), &strtoul_end, 10);

    if (errno || value.c_str() == strtoul_end || strtoul_result > USHRT_MAX) {
      return false;
    }

    CCAPI_Config_set_remote_voip_port((int) strtoul_result);
  } else if (key == ConfigPropertyKeysEnum::eTransport) {
    if (value == "tcp")
      CCAPI_Config_set_transport_udp(false);
    else
      CCAPI_Config_set_transport_udp(true);
  }

  return true;
}

std::string CallControlManagerImpl::getProperty(ConfigPropertyKeysEnum::ConfigPropertyKeys key)
{
  std::string retValue = "NONESET";
  char tmpString[11];

  CSFLogInfoS(logTag, "getProperty()");

  if (key == ConfigPropertyKeysEnum::eLocalVoipPort) {
    csf_sprintf(tmpString, sizeof(tmpString), "%u", CCAPI_Config_get_local_voip_port());
    retValue = tmpString;
  } else if (key == ConfigPropertyKeysEnum::eRemoteVoipPort) {
    csf_sprintf(tmpString, sizeof(tmpString), "%u", CCAPI_Config_get_remote_voip_port());
    retValue = tmpString;
  } else if (key == ConfigPropertyKeysEnum::eVersion) {
    const char* version = CCAPI_Config_get_version();
    retValue = version;
  }

  return retValue;
}
/*
  There are a number of factors that determine PhoneAvailabilityType::PhoneAvailability. The supported states for this enum are:
  { eUnknown, eAvailable, eUnAvailable, eNotAllowed }. eUnknown is the default value, which is set when there is no information
  available that would otherwise determine the availability value. The factors that can influence PhoneAvailability are:
  phone mode, and for a given device (described by DeviceInfo) the model, and the name of the device. For phone control mode, the
  device registration and whether CUCM says the device is CTI controllable (or not) is a factor.

  For Phone Control mode the state machine is:

     is blacklisted model name? -> Yes -> NOT_ALLOWED
        (see Note1 below)
              ||
              \/
              No
              ||
              \/
       is CTI Controllable?
     (determined from CUCM) -> No -> NOT_ALLOWED
              ||
              \/
              Yes
              ||
              \/
  Can we tell if it's registered? -> No -> ?????? TODO: Seems to depends on other factors (look at suggestedAvailability parameter
              ||                                        in DeviceSubProviderImpl.addOrUpdateDevice() in CSF1G Java code.
              \/
              Yes
              ||
              \/
         is Registered?
     (determined from CUCM) -> No -> NOT_AVAILABLE
              ||
              \/
              Yes
              ||
              \/
           AVAILABLE

  ========

  For Softphone mode the state machine is:

        is device excluded?
   (based on "ExcludedDevices"   -> Yes -> NOT_ALLOWED
         config settings
        (see Note2 below))
              ||
              \/
              No
              ||
              \/
          isSoftphone?



     Note1: model name has to match completely, ie it's not a sub-string match, but we are ignoring case. So, if the blacklist
            contains a string "Cisco Unified Personal Communicator" then the model has to match this completely (but can be a
            different case) to be a match. In CSF1G the blacklist is hard-wired to:
                { "Cisco Unified Personal Communicator",
                  "Cisco Unified Client Services Framework",
                  "Client Services Framework",
                  "Client Services Core" }

     Note2: The "ExcludedDevices" is a comma-separated list of device name prefixes (not model name). Unlike the above, this is
            a sub-string match, but only a "starts with" sub-string match, not anywhere in the string. If the device name
            is a complete match then this is also excluded, ie doesn't have to be a sub-string. For example, if the
            ExcludeDevices list contains { "ECP", "UPC" } then assuming we're in softphone mode, then any device whose
            name starts with the strings ECP or UPC, or whose complete name is either of these will be deemed to be excluded
            and will be marked as NOT_ALLOWED straightaway. In Phone Control mode the "ExcludedDevices" list i not taken into
            account at all in the determination of availability.

     Note3: isSoftphone() function

  The config service provides a list of "blacklisted" device name prefixes, that is, if the name of the device starts with a
  sub-string that matches an entry in the blacklist, then it is straightaway removed from the list? marked as NOT_ALLOWED.
 */

// CC_Observers
void CallControlManagerImpl::onDeviceEvent(ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info)
{
    notifyDeviceEventObservers(deviceEvent, devicePtr, info);
}
void CallControlManagerImpl::onFeatureEvent(ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info)
{
    notifyFeatureEventObservers(deviceEvent, devicePtr, info);
}
void CallControlManagerImpl::onLineEvent(ccapi_line_event_e lineEvent,     CC_LinePtr linePtr, CC_LineInfoPtr info)
{
    notifyLineEventObservers(lineEvent, linePtr, info);
}
void CallControlManagerImpl::onCallEvent(ccapi_call_event_e callEvent,     CC_CallPtr callPtr, CC_CallInfoPtr info)
{
    notifyCallEventObservers(callEvent, callPtr, info);
}


void CallControlManagerImpl::notifyDeviceEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_DeviceInfoPtr info)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<CC_Observer*>::const_iterator it = ccObservers.begin();
    for ( ; it != ccObservers.end(); it++ )
    {
        (*it)->onDeviceEvent(deviceEvent, devicePtr, info);
    }
}

void CallControlManagerImpl::notifyFeatureEventObservers (ccapi_device_event_e deviceEvent, CC_DevicePtr devicePtr, CC_FeatureInfoPtr info)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<CC_Observer*>::const_iterator it = ccObservers.begin();
    for ( ; it != ccObservers.end(); it++ )
    {
        (*it)->onFeatureEvent(deviceEvent, devicePtr, info);
    }
}

void CallControlManagerImpl::notifyLineEventObservers (ccapi_line_event_e lineEvent, CC_LinePtr linePtr, CC_LineInfoPtr info)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<CC_Observer*>::const_iterator it = ccObservers.begin();
    for ( ; it != ccObservers.end(); it++ )
    {
        (*it)->onLineEvent(lineEvent, linePtr, info);
    }
}

void CallControlManagerImpl::notifyCallEventObservers (ccapi_call_event_e callEvent, CC_CallPtr callPtr, CC_CallInfoPtr info)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<CC_Observer*>::const_iterator it = ccObservers.begin();
    for ( ; it != ccObservers.end(); it++ )
    {
        (*it)->onCallEvent(callEvent, callPtr, info);
    }
}

void CallControlManagerImpl::notifyAvailablePhoneEvent (AvailablePhoneEventType::AvailablePhoneEvent event,
        const PhoneDetailsPtr availablePhoneDetails)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<ECC_Observer*>::const_iterator it = eccObservers.begin();
    for ( ; it != eccObservers.end(); it++ )
    {
        (*it)->onAvailablePhoneEvent(event, availablePhoneDetails);
    }
}

void CallControlManagerImpl::notifyAuthenticationStatusChange (AuthenticationStatusEnum::AuthenticationStatus status)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<ECC_Observer*>::const_iterator it = eccObservers.begin();
    for ( ; it != eccObservers.end(); it++ )
    {
        (*it)->onAuthenticationStatusChange(status);
    }
}

void CallControlManagerImpl::notifyConnectionStatusChange(ConnectionStatusEnum::ConnectionStatus status)
{
	mozilla::MutexAutoLock lock(m_lock);
    set<ECC_Observer*>::const_iterator it = eccObservers.begin();
    for ( ; it != eccObservers.end(); it++ )
    {
        (*it)->onConnectionStatusChange(status);
    }
}

void CallControlManagerImpl::setConnectionState(ConnectionStatusEnum::ConnectionStatus status)
{
	connectionState = status;
	notifyConnectionStatusChange(status);
}
}
