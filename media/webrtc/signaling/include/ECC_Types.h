/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

/*
  These #defines are for use with CallControlManager::createSoftphone(..., const cc_int32_t mask) parameter as follows:

  CallControlManager::createSoftphone(..., GSM_DEBUG_BIT | FIM_DEBUG_BIT | LSM_DEBUG_BIT );

  This turns on debugging for the three areas (and disables logging in all other areas) of pSIPCC logging specified.

*/

#define SIP_DEBUG_MSG_BIT         (1 <<  0) // Bit 0
#define SIP_DEBUG_STATE_BIT       (1 <<  1) // Bit 1
#define SIP_DEBUG_TASK_BIT        (1 <<  2) // Bit 2
#define SIP_DEBUG_REG_STATE_BIT   (1 <<  3) // Bit 3
#define GSM_DEBUG_BIT             (1 <<  4) // Bit 4
#define FIM_DEBUG_BIT             (1 <<  5) // Bit 5
#define LSM_DEBUG_BIT             (1 <<  6) // Bit 6
#define FSM_DEBUG_SM_BIT          (1 <<  7) // Bit 7
#define CSM_DEBUG_SM_BIT          (1 <<  8) // Bit 8
#define CC_DEBUG_BIT              (1 <<  9) // Bit 9
#define CC_DEBUG_MSG_BIT          (1 << 10) // Bit 10
#define AUTH_DEBUG_BIT            (1 << 11) // Bit 11
#define CONFIG_DEBUG_BIT          (1 << 12) // Bit 12
#define DPINT_DEBUG_BIT           (1 << 13) // Bit 13
#define KPML_DEBUG_BIT            (1 << 15) // Bit 15
#define VCM_DEBUG_BIT             (1 << 17) // Bit 17
#define CC_APP_DEBUG_BIT          (1 << 18) // Bit 18
#define CC_LOG_DEBUG_BIT          (1 << 19) // Bit 19
#define TNP_DEBUG_BIT             (1 << 20) // Bit 20

namespace CSFUnified
{
    DECLARE_PTR(DeviceInfo)
}

namespace CSF
{
	namespace AuthenticationFailureCodeType
	{
		typedef enum {
			eNoError,
			eNoServersConfigured,
			eNoCredentialsConfigured,
			eCouldNotConnect,
			eServerCertificateRejected,
			eCredentialsRejected,
			eResponseEmpty,
			eResponseInvalid
		} AuthenticationFailureCode;
		std::string ECC_API toString(AuthenticationFailureCode value);
	}

	namespace AuthenticationStatusEnum
	{
		typedef enum {
			eNotAuthenticated,
			eInProgress,
			eAuthenticated,
			eFailed
		} AuthenticationStatus;
		std::string ECC_API toString(AuthenticationStatus value);
	}

	namespace DeviceRetrievalFailureCodeType
	{
		typedef enum {
			eNoError,
			eNoServersConfigured,
			eNoDeviceNameConfigured,
			eCouldNotConnect,
			eFileNotFound,
			eFileEmpty,
			eFileInvalid
		} DeviceRetrievalFailureCode;
		std::string ECC_API toString(DeviceRetrievalFailureCode value);
	}

    namespace ConnectionStatusEnum
    {
    	typedef enum {
    		eIdle,
    		eNone,
    		eFetchingDeviceConfig,
    		eRegistering,
    		eReady,
    		eConnectedButNoDeviceReady,
    		eRetrying,
    		eFailed
    	} ConnectionStatus;
		std::string ECC_API toString(ConnectionStatus value);
    }

	namespace ServiceStateType {
		typedef enum
		{
			eUnknown,
			eInService,
			eOutOfService
		} ServiceState;
		std::string ECC_API toString(ServiceState value);
	}

	namespace AvailablePhoneEventType
	{
		typedef enum {
			eFound,		// New Phone device discovered and added to the Available list.
			eUpdated,	// Change to an existing Phone details record in the Available list.
			eLost		// Phone device removed from the Available list.
		} AvailablePhoneEvent;
		std::string ECC_API toString(AvailablePhoneEvent value);
	}

	namespace ConfigPropertyKeysEnum
	{
		typedef enum {
			eLocalVoipPort,
			eRemoteVoipPort,
			eVersion,
			eTransport
		} ConfigPropertyKeys;
	}

	typedef enum
	{
		VideoEnableMode_DISABLED,
		VideoEnableMode_SENDONLY,
		VideoEnableMode_RECVONLY,
		VideoEnableMode_SENDRECV

	} VideoEnableMode;

	typedef enum				// scaling from codec size to render window size
	{
		RenderScaling_NONE,		// 1:1 actual pixels, crop and/or fill
		RenderScaling_TO_FIT,	// scale to fit, without preserving aspect ratio
		RenderScaling_BORDER,	// scale to fit, fill without cropping
		RenderScaling_CROP		// scale to fit, crop without filling

	} RenderScaling;

    typedef void *VideoWindowHandle;
	typedef void* ExternalRendererHandle;
	typedef unsigned int VideoFormat;
}

