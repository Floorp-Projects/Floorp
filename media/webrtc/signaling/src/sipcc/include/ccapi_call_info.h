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

#ifndef _CCAPI_CALL_INFO_H_
#define _CCAPI_CALL_INFO_H_

#include "ccapi_types.h"
#include "peer_connection_types.h"

/**
 * get Line on which this call is
 * @param [in] handle - call info handle
 * @return cc_line_id_t - line ID
 */
cc_lineid_t CCAPI_CallInfo_getLine(cc_callinfo_ref_t handle);

/**
 * get Call state
 * @param [in] handle - call info handle
 * @return call state
 */
cc_call_state_t CCAPI_CallInfo_getCallState(cc_callinfo_ref_t handle);

/**
 * get call attributes
 * @param [in] handle - call info handle
 * @return call attributes
 */
cc_call_attr_t CCAPI_CallInfo_getCallAttr(cc_callinfo_ref_t handle);

/**
 * get Call Type
 * @param [in] handle - call info handle
 * @return call type
 */
cc_call_type_t CCAPI_CallInfo_getCallType(cc_callinfo_ref_t handle);

/**
 * get Called party name 
 * @param [in] handle - call info handle
 * @return called party name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getCalledPartyName(cc_callinfo_ref_t handle);

/**
 * get Called party number
 * @param [in] handle - call info handle
 * @return called party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getCalledPartyNumber(cc_callinfo_ref_t handle);

/**
 * get Calling party name
 * @param [in] handle - call info handle
 * @return calling party name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getCallingPartyName(cc_callinfo_ref_t handle);

/**
 * get Calling party number
 * @param [in] handle - call info handle
 * @return calling party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getCallingPartyNumber(cc_callinfo_ref_t handle);

/**
 * get Calling party number
 * @param [in] handle - call info handle
 * @return calling party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getAlternateNumber(cc_callinfo_ref_t handle);

/**
 * get Original Called party name
 * @param [in] handle - call info handle
 * @return original called party name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getOriginalCalledPartyName(cc_callinfo_ref_t handle);

/**
 * get Original Called party number
 * @param [in] handle - call info handle
 * @return original called party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getOriginalCalledPartyNumber(cc_callinfo_ref_t handle);

/**
 * get last redirecting party name
 * @param [in] handle - call info handle
 * @return last redirecting party name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getLastRedirectingPartyName(cc_callinfo_ref_t handle);

/**
 * get past redirecting party number
 * @param [in] handle - call info handle
 * @return last redirecting party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getLastRedirectingPartyNumber(cc_callinfo_ref_t handle);

/**
 * get placed call party name
 * @param [in] handle - call info handle
 * @return placed party name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getPlacedCallPartyName(cc_callinfo_ref_t handle);

/**
 * get placed call party number
 * @param [in] handle - call info handle
 * @return placed party number
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getPlacedCallPartyNumber(cc_callinfo_ref_t handle);


/**
 * get call instance number
 * @param [in] handle - call info handle
 * @return 
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_int32_t CCAPI_CallInfo_getCallInstance(cc_callinfo_ref_t handle);

/**
 * get call status prompt
 * @param [in] handle - call info handle
 * @return call status
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getStatus(cc_callinfo_ref_t handle);

/**
 * get call security   // TODO XLS has callagent security and endtoend security on call?
 * @param [in] handle - call info handle
 * @return call security status
 */
cc_call_security_t CCAPI_CallInfo_getSecurity(cc_callinfo_ref_t handle);

/**
 * get Call Selection Status
 * @param [in] handle - call info handle
 * @return cc_boolean - TRUE => selected
 */
cc_int32_t CCAPI_CallInfo_getSelectionStatus(cc_callinfo_ref_t handle);

/**
 * get GCID
 * @param [in] handle - call info handle
 * @return GCID
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_CallInfo_getGCID(cc_callinfo_ref_t handle);

/**
 * get ringer loop count
 * @param handle - call handle
 * @return once Vs continuous
 */
cc_boolean CCAPI_CallInfo_getIsRingOnce(cc_callinfo_ref_t handle);

/**
 * get ringer state.
 * @param handle - call handle
 * @return ringer state.
 */
cc_boolean CCAPI_CallInfo_getRingerState(cc_callinfo_ref_t handle);

/**
 * get ringer mode
 * @param handle - call handle
 * @return ringer mode
 */
int CCAPI_CallInfo_getRingerMode(cc_callinfo_ref_t handle);


/**
 * get onhook reason
 * @param [in] handle - call info handle
 * @return onhook reason
 */
cc_int32_t  CCAPI_CallInfo_getOnhookReason(cc_callinfo_ref_t handle);

/**
 * is Conference Call?
 * @param [in] handle - call info handle
 * @return boolean - is Conference
 */
cc_boolean  CCAPI_CallInfo_getIsConference(cc_callinfo_ref_t handle);

/**
 * getStream Statistics
 * @param [in] handle - call info handle
 * @param [in,out] stats - Array to get the stats
 * @param [in,out] count - in len of stats arraysize of stats / out stats copied
 * @return cc_return_t - CC_SUCCESS or CC_FAILURE
 */
cc_return_t  CCAPI_CallInfo_getStreamStatistics(cc_callinfo_ref_t handle, cc_int32_t stats[], cc_int32_t *count);


/**
 * has capability - is the feature allowed
 * @param [in] handle - call info handle
 * @param [in] feat_id - feature id
 * @return boolean - is Allowed
 */
cc_boolean  CCAPI_CallInfo_hasCapability(cc_callinfo_ref_t handle, cc_int32_t feat_id);

/**
 * get Allowed Feature set
 * @param [in] handle - call info handle
 * @param [in,out] feat_set - array of len CC_CALL_CAP_MAX 
 * @return cc_return_t - CC_SUCCESS or CC_FAILURE
 */
cc_return_t  CCAPI_CallInfo_getCapabilitySet(cc_callinfo_ref_t handle, cc_int32_t feat_set[]);

/**
 * Call selection status
 * @param [in] handle - call info handle
 * @return cc_boolean - selection status
 */
cc_boolean  CCAPI_CallInfo_isCallSelected(cc_callinfo_ref_t handle);

/**
 * INFO Package for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - Info package header
 */
cc_string_t  CCAPI_CallInfo_getINFOPack(cc_callinfo_ref_t handle);

/**
 * INFO type for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - content-type  header
 */
cc_string_t  CCAPI_CallInfo_getINFOType(cc_callinfo_ref_t handle);

/**
 * INFO body for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - INFO body
 */
cc_string_t  CCAPI_CallInfo_getINFOBody(cc_callinfo_ref_t handle);

/**
 * Get the call log reference
 * @param [in] handle - call info handle
 * @return cc_string_t - INFO body
 * NOTE: Memory associated with the call log is tied to the cc_callinfo_ref_t handle 
 * this would be freed when the callinfo ref is freed.
 */
cc_calllog_ref_t  CCAPI_CallInfo_getCallLogRef(cc_callinfo_ref_t handle);

/**
 * returns the negotiated video direction for this call
 * @param [in] handle - call handle
 * @return cc_sdp_direction_t - video direction
 */
cc_sdp_direction_t  CCAPI_CallInfo_getVideoDirection(cc_callinfo_ref_t handle);

/**
 * Returns the Audio mute state for this call
 * @return boolean true=muted false=not muted
 */        
cc_boolean CCAPI_CallInfo_isAudioMuted(cc_callinfo_ref_t handle);

/**
 * Returns the Video  mute state for this call
 * @return boolean true=muted false=not muted
 */        
cc_boolean CCAPI_CallInfo_isVideoMuted(cc_callinfo_ref_t handle);

/**
 * get SDP string CreateOffer and CreateAnswer callback
 * @param [in] handle - call info handle
 * @return sdp
 */
cc_string_t CCAPI_CallInfo_getSDP(cc_callinfo_ref_t handle);

/**
 * get status code from internal JSEP functions
 * @param [in] handle - call info handle
 * @return status code
 */
cc_int32_t  CCAPI_CallInfo_getStatusCode(cc_callinfo_ref_t handle);

/**
 * get media stream table
 * @param [in] handle - call info handle
 * @return media track table
 */
MediaStreamTable* CCAPI_CallInfo_getMediaStreams(cc_callinfo_ref_t handle);

#endif /* _CCAPIAPI_CALL_INFO_H_ */
