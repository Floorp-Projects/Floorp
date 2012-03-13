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
