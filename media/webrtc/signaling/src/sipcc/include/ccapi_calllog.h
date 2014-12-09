/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_stdio.h"
#include "ccapi_call.h"
#include "sessionHash.h"
#include "CCProvider.h"
#include "phone_debug.h"


/**
 * get the placed missed rcvd disposition for the call
 * @param [in] handle - reference to call log
 * @return cc_log_disposition_t - log disposition
 */
cc_log_disposition_t CCAPI_CallLog_getCallDisposition(cc_calllog_ref_t handle);

/**
 * get call start Time as seconds since epoch
 * @param [in] handle - reference to call log
 * @return  cc_uint32_t - start time as number of seconds since epoch
 */
cc_uint32_t CCAPI_CallLog_getStartTime(cc_calllog_ref_t handle);

/**
 * get call duration in seconds
 * @param [in] handle - reference to call log
 * @return cc_uint32_t - call duration in seconds
 */
cc_uint32_t CCAPI_CallLog_getCallDuration(cc_calllog_ref_t handle);

/**
 * get first leg remote party name
 * @param [in] handle - reference to call log
 * @return cc_string_t - remote party name for first leg
 */
cc_string_t CCAPI_CallLog_getFirstLegRemotePartyName(cc_calllog_ref_t handle);

/**
 * get last leg remote party name
 * @param [in] handle - reference to call log
 * @return cc_string_t - remote party name for last leg
 */
cc_string_t CCAPI_CallLog_getLastLegRemotePartyName(cc_calllog_ref_t handle);

/**
 * get first leg remote party number
 * @param [in] handle - reference to call log
 * @return cc_string_t - remote party number for first leg
 */
cc_string_t CCAPI_CallLog_getFirstLegRemotePartyNumber(cc_calllog_ref_t handle);

/**
 * get last leg remote party number
 * @param [in] handle - reference to call log
 * @return cc_string_t - remote party number for last leg
 */
cc_string_t CCAPI_CallLog_getLastLegRemotePartyNumber(cc_calllog_ref_t handle);

/**
 * get first leg local party name
 * @param [in] handle - reference to call log
 * @return cc_string_t - local party name for first leg
 */
cc_string_t CCAPI_CallLog_getLocalPartyName(cc_calllog_ref_t handle);

/**
 * get first leg local party number
 * @param [in] handle - reference to call log
 * @return cc_string_t - local party number for first leg
 */
cc_string_t CCAPI_CallLog_getLocalPartyNumber(cc_calllog_ref_t handle);

/**
 * get first leg alt party number
 * @param [in] handle - reference to call log
 * @return cc_string_t - alt party number for first leg
 */
cc_string_t CCAPI_CallLog_getFirstLegAltPartyNumber(cc_calllog_ref_t handle);

/**
 * get last leg alt party number
 * @param [in] handle - reference to call log
 * @return cc_string_t - alt party number for last leg
 */
cc_string_t CCAPI_CallLog_getLastLegAltPartyNumber(cc_calllog_ref_t handle);
