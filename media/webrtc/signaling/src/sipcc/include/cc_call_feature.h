/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  @mainpage Portable SIP Stack API
 *
 *  @section intro_sec Introduction
 *  The portable SIP stack is used in multiple SIP endpoints. This document
 *  describes the API's provided by the portable SIP stack that third party
 *  vendors must implement to use the stack.
 *
 *
 *  @section sub_sec Functionality
 *  The SIP stack can be viewed as composed of different components each
 *  dealing with a specific functionality.
 *  functional modules in SIP stack
 *
 *  @subsection Management  Management
 *    This section covers the API that deals with managing the sip stack
 *    initialization, shutdown and state management.
 *    @li cc_types.h @par
 *      Type definitions used by SIP stack
 *    @li cc_constants.h @par
 *      Constant definitions used by call control
 *    @li cc_config.h @par
 *      This section covers API to set config data for the SIP stack
 *    @li cc_service.h @par
 *      Commands to initialize, restart, shutdown sipstack
 *    @li cc_service_listener.h @par
 *      Callbacks from SIP stack for SIP stack and registration status updates
 *
 *  @subsection cf Call Features
 *    Call Control features and calls related APIs
 *    @li cc_call_feature.h @par
 *      APIs to create/terminate calls and invoke call features on them
 *    @li cc_call_listener.h @par
 *      Callbacks for call states and call related data updates
 *    @li cc_info.h @par
 *      API to send a SIP INFO info Message in the context of a call
 *    @li cc_info_listener.h @par
 *      Callback on receipt of SIP INFO Message in the context of a call
 *
 *  @subsection df Device Features
 *    Device based features related APIs
 *    @li cc_device_feature.h @par
 *      APIs to invoke device specific features
 *    @li cc_device_listener.h @par
 *      Callbacks for device specific feature/data updates
 *
 *  @subsection blf BLF APIs
 *    This covers Busy Lamp Field subscription and state notifications
 *    @li cc_blf.h @par
 *      BLF subscription and unsubscription APIs
 *    @li cc_blf_listener.h @par
 *      BLF state change notification call backs
 *
 *  @subsection vcm  Media APIs
 *    Media related APIs
 *    @li vcm.h @par
 *      This file contains API that interface to the media layer on the platform.
 *      The following APIs need to be implemented to have the sip stack interact
 *      and issue commands to the media layer.
 *    @li ccsdp.h @par
 *      Contains helper methods to peek and set video SDP attributes as the SDP negotiation for m lines needs
 *      to be confirmed by the application after examining the SDP attributes. See vcmCheckAttribs and
 *      vcmPopulateAttribs methods in vcm.h
 *
 *  @subsection xml XML
 *    This section covers the XML Parser API that are invoked by the SIP stack to parse  and encode XML bodies
 *    @li xml_parser.h @par
 *      Methods to be implemented by platform for XML enoced and decode operations
 *    @li xml_parser_defines.h @par
 *      Type definitons and data structures used by the XML APIs
 *
 *  @subsection util Utilities
 *    Misc utilities
 *    @li sll_lite.h @par
 *      Simple linked list implementation bundled with SIP stack to be used in xml_parser apis
 *    @li dns_util.h @par
 *      DNS query methods to be implemented by the platform code
 *    @li plat_api.h  @par
 *      APIs that must be implemented by the platform for platform specific functionality
 *
 */

#ifndef _CC_CALL_FEATURE_H_
#define _CC_CALL_FEATURE_H_

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#include "timecard.h"
#endif

#include "cc_constants.h"

/***********************************Basic Call Feature Control Methods************************************
 * This section defines all the call related methods that an upper layer can use to control
 * a call in progress.
 */

/**
 * Used to create any outgoing call regular call. The incoming/reverting/consultation call will be
 * created by the stack. It creates a call place holder and initialize the memory for a call. An user needs
 * other methods to start the call, such as the method OriginateCall, etc
 * @param line line number that is invoked and is assigned
 * @return call handle wich includes the assigned line and call id
 */
cc_call_handle_t CC_createCall(cc_lineid_t line);

/**
 * Start the call that was created.
 * @param call_handle the call handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
 /*move it up...*/
cc_return_t CC_CallFeature_originateCall(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref);

/**
 * Terminate or end a normal call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_terminateCall(cc_call_handle_t call_handle);

/**
 * Answer an incoming or reverting call.
 * @param call_handle call handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_answerCall(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref);

/**
 * Send a keypress to a call, it could be a single digit.
 * @param call_handle call handle
 * @param cc_digit digit pressed
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_sendDigit(cc_call_handle_t call_handle, cc_digit_t cc_digit);

/**
 * Send a backspace action.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_backSpace(cc_call_handle_t call_handle);

/**
 * Send a dial digit string on an active call, e.g."9191234567".
 * @param call_handle call handle
 * @param video_pref the sdp direction
 * @param numbers dialed string
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_dial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const cc_string_t numbers);

/**
 * Initiate a speed dial.
 * @param call_handle call handle
 * @param speed_dial_number speed dial number to be dialed.
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_speedDial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const cc_string_t speed_dial_number);

/**
 * Initiate a BLF call pickup.
 * @param call_handle call handle
 * @param speed_dial_number speed dial number configured.
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_blfCallPickup(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const cc_string_t speed_dial_number);

/**
 * Redial the last dial numbers.
 * @param call_handle call handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 * @note: if there is no active dial made, this method should not be called.
 */
cc_return_t CC_CallFeature_redial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref);

/**
 * Update a video media capability for a call. To be used to escalate deescalate video on a specified call
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_updateCallMediaCapability(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref);

/**
 * Make a call forward all on the line identified by the call_handle.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_callForwardAll(cc_call_handle_t call_handle);

/**
 * Resume a held call.
 * @param call_handle call handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_resume(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref);

/**
 * End a consultation call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_endConsultativeCall(cc_call_handle_t call_handle);

/**
 * Initiate a conference. Steps to make a conference or transfer:
 * @li Step1 Create a call handle, e.g. chandle1.
 * @li Step2 Start the call on this call handle.
 * @li Step3 When the call is answered, invoke:
 *        CC_CallFeature_conference(chandle1, FALSE, CC_EMPTY_CALL_HANDLE) to start a conference operation.
 * @li Step4 Upon receiving the consultative call (cHandle2) created from pSipcc system,
 *    invoke:
 *        CC_CallFeature_dial(cHandle2)
 * 	  to dial the consultative call.
 * @li Step5 When the consultative call is in ringout or connected state, invoke:
 *        CC_CallFeature_conference(cHandle2, FALSE, CC_EMPTY_CALL_HANDLE) to
 *    finish the conference.
 * Note: @li in the step 4, a user could kill the consultative call and pickup a hold call (not the parent call that
 *    initiated the conference). In this scenario, a parent call handle should be supplied.
 *    For instance,
 *        CC_CallFeature_conference(cHandle2, FALSE, cHandle1)
 *       @li If it's a B2bConf, substitute the "FALSE" with "TRUE"
 *
 * @param call_handle the call handle for
 * 	@li the original connected call.
 * 	@li the consultative call or a held call besides the parent call initiated the transfer. This is used
 * 	   on the second time to finish the transfer.
 * @param is_local Local conference or not. If it's a local conference, it's a b2bconf.
 * @param parent_call_handle if supplied, it will be the targeted parent call handle, which initiated the conference.
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_conference(cc_call_handle_t call_handle, cc_boolean is_local,
		cc_call_handle_t parent_call_handle, cc_sdp_direction_t video_pref);

/**
 * Initiate a call transfer. Please refer to Conference feature.
 * @param call_handle the call handle for
 * 	@li the original connected call.
 * 	@li the consultative call or a held call besides the parent call initiated the transfer. This is used
 * 	   on the second time to finish the transfer.
 * @param parent_call_handle if supplied, it will be the parent call handle, which initiated the transfer.
 * @param video_pref video preference for the consult call as specified by direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_transfer(cc_call_handle_t call_handle, cc_call_handle_t parent_call_handle, cc_sdp_direction_t video_pref);


/**
 * Initiate a direct transfer
 * @param call_handle the call handle for the call to initialize a transfer
 * @param target_call_handle the call handle for the target transfer call.
 * @retrun SUCCESS or FAILURE. If the target call handle is empty, a FAILURE will be returned.
 */
cc_return_t CC_CallFeature_directTransfer(cc_call_handle_t call_handle, cc_call_handle_t target_call_handle);

/**
 * Initiate a join across line
 * @param call_handle the call handle for the call that initializes a join across line (conference).
 * @param target_call_handle the call handle for the call will be joined.
 */
cc_return_t CC_CallFeature_joinAcrossLine(cc_call_handle_t call_handle, cc_call_handle_t target_call_handle);

/**
 * Put a connected call on hold.
 * @param call_handle call handle
 * @param reason the reason to hold. The following values should be used.
 * 	  CC_HOLD_REASON_NONE,
 *    CC_HOLD_REASON_XFER, //Hold for transfer
 *    CC_HOLD_REASON_CONF, //Hold for conference
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_holdCall(cc_call_handle_t call_handle, cc_hold_reason_t reason);

/********************************End of basic call feature methods******************************************/

/*************************************Additional call feature methods***************************************
 *
 */
/** @todo if needed
 * Direct transfer a call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
//cc_return_t CC_CallFeature_directTransfer(cc_call_handle_t call_handle);

/**
 * Select or locked a call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_select(cc_call_handle_t call_handle);

/**
 * Cancel a call feature, e.g. when the consultative call is connected and the
 * user wishes not to make the conference, thie method can be invoked.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_cancelXfrerCnf(cc_call_handle_t call_handle);

/**
 * Indicate the mute state on the device
 * Used to send a mute state across over CAST to the PC
 */
void CC_CallFeature_mute(cc_boolean mute);

/**
 * Indicate the speaker state on the device
 * Used to send a speaker state across over CAST to the PC
 */
void CC_CallFeature_speaker(cc_boolean mute);

/**
 * Returns the call id of the connected call
 */

cc_call_handle_t CC_CallFeature_getConnectedCall();

#endif /* _CC_CALL_FEATURE_H_ */
