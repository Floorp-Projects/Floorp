/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_CALL_H_
#define _CCAPI_CALL_H_

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#include "timecard.h"
#endif

#include "ccapi_types.h"

/**
 * Get reference to call info
 * @param [in] handle - call handle
 * @return cc_call_info_snap_t
 * NOTE: The info returned by this method must be released using CCAPI_Call_releaseCallInfo()
 */
cc_callinfo_ref_t CCAPI_Call_getCallInfo(cc_call_handle_t handle);

/**
 * Release the resources held by this call info snapshot
 * @param [in] ref - refrence to the call info to be freed
 * @return void
 */
void CCAPI_Call_releaseCallInfo(cc_callinfo_ref_t ref);

/**
 *  Retain the call info reference
 *  @param [in] ref - reference to the call info to be retained
 *  @return void
 *  NOTE: Application that has retained callInfo must call CCAPI_Call_releaseCallInfo()
 *  to free memory associated with this Info.
 */
void CCAPI_Call_retainCallInfo(cc_callinfo_ref_t ref);

/**
 * get the line associated with this call
 * @param [in] handle - call handle
 * @return cc_lineid_t
 */
cc_lineid_t CCAPI_Call_getLine(cc_call_handle_t call_handle);

/**
 * Originate call - API to go offhook and dial specified digits on a given call
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on call
 * @param [in] digits - digits to be dialed. can be empty then this API simply goes offhook
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_originateCall(cc_call_handle_t handle, cc_sdp_direction_t video_pref, cc_string_t digits);


/**
 * Send digits on the call - can be invoked either to dial additional digits or send DTMF
 * @param [in] handle - call handle
 * @param [in] digit - digit to be dialed
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_sendDigit(cc_call_handle_t handle, cc_digit_t digit);

/**
 * Send Backspace - Delete last digit dialed.
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_backspace(cc_call_handle_t handle);

/**
 * Answer Call
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_answerCall(cc_call_handle_t handle, cc_sdp_direction_t video_pref);

/**
 * Redial
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_redial(cc_call_handle_t handle, cc_sdp_direction_t video_pref);

/**
 * Initiate Call Forward All
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_initiateCallForwardAll(cc_call_handle_t handle);
/**
 * Hold
 * @param [in] handle - call handle
 * @param [in] reason - hold reason
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_hold(cc_call_handle_t handle, cc_hold_reason_t reason);

/**
 * Resume
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_resume(cc_call_handle_t handle, cc_sdp_direction_t video_pref) ;

/**
 * end Consult leg - used to end consult leg when the user picks active calls list for xfer/conf
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_endConsultativeCall(cc_call_handle_t handle);

/**
 * end Call
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_endCall(cc_call_handle_t handle);

/**
 * Initiate a conference
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on consult call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_conferenceStart(cc_call_handle_t handle, cc_sdp_direction_t video_pref);

/**
 * complete conference
 * @param [in] handle - call handle
 * @param [in] phandle - call handle of the other leg
 * @param [in] video_pref - video direction desired on consult call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_conferenceComplete(cc_call_handle_t handle, cc_call_handle_t phandle,
                          cc_sdp_direction_t video_pref);

/**
 * start transfer
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on consult call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_transferStart(cc_call_handle_t handle, cc_sdp_direction_t video_pref);

/**
 * complete transfer
 * @param [in] handle - call handle
 * @param [in] phandle - call handle of the other leg
 * @param [in] video_pref - video direction desired on consult call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_transferComplete(cc_call_handle_t handle, cc_call_handle_t phandle,
                          cc_sdp_direction_t video_pref);

/**
 * cancel conference or transfer
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_cancelTransferOrConferenceFeature(cc_call_handle_t handle);

/**
 * direct Transfer
 * @param [in] handle - call handle
 * @param [in] target - call handle for transfer target call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_directTransfer(cc_call_handle_t handle, cc_call_handle_t target);

/**
 * Join Across line
 * @param [in] handle - call handle
 * @param [in] target - join target
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_joinAcrossLine(cc_call_handle_t handle, cc_call_handle_t target);

/**
 * BLF Call Pickup
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction preference
 * @param [in] speed - speedDial Number
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_blfCallPickup(cc_call_handle_t handle, cc_sdp_direction_t video_pref, cc_string_t speed);

/**
 * Select a call
 * @param [in] handle - call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_select(cc_call_handle_t handle);

/**
 * Update Video Media Cap for the call
 * @param [in] handle - call handle
 * @param [in] video_pref - video direction desired on call
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_updateVideoMediaCap(cc_call_handle_t handle, cc_sdp_direction_t video_pref);

/**
 * send INFO method for the call
 * @param [in] handle - call handle
 * @param [in] infopackage - Info-Package header value
 * @param [in] infotype - Content-Type header val
 * @param [in] infobody - Body of the INFO message
 * @return SUCCESS or FAILURE
 */
cc_return_t CCAPI_Call_sendInfo(cc_call_handle_t handle, cc_string_t infopackage, cc_string_t infotype, cc_string_t infobody);

/**
 * API to mute/unmute audio
 * @param [in] val - TRUE=> mute FALSE => unmute
 * @return SUCCESS or FAILURE
 * NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume.
 * This API doesn't perform the mute operation but simply caches the mute state of the session.
 */
cc_return_t CCAPI_Call_setAudioMute(cc_call_handle_t handle, cc_boolean val);

/**
 * API to mute/unmute Video
 * @param [in] val - TRUE=> mute FALSE => unmute
 * @return SUCCESS or FAILURE
 * NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume
 * This API doesn't perform the mute operation but simply caches the mute state of the session.
 */
cc_return_t CCAPI_Call_setVideoMute(cc_call_handle_t handle, cc_boolean val);


#endif // _CCAPI_CALL_H_
