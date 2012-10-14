/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_CALL_LISTENER_H_
#define _CC_CALL_LISTENER_H_

#include "cc_constants.h"

/*****************************************************************************************************
 * Call Control: provides the methods to process a call or calls.
 *****************************************************************************************************/

/****************************************Call Listener Methods****************************************
 * This section defines all call related methods that will invoke an application about a call.
  * All input parameters please see their definition in cc_constancts.h file.
 */

/**
 * Notification for the creation of a new call
 * @param call_handle call handle that is created.
 * @param call_state the call state
 * @param cc_cause cause that the call is created
 * @param cc_attr attr of the call created
 * @return void
 */
void CC_CallListener_callCreated(cc_call_handle_t call_handle,
		cc_call_state_t call_state,
		cc_cause_t cc_cause,
		cc_call_attr_t cc_attr);

/**
 * Update a call state, e.g. offhook.
 * Description: the first CallStateChanged after the CallCreated will have included
 * call_attr and call_instance. The consective CallStateChanged should ignore the
 * call_attr and call instance.
 * @param call_handle call handle
 * @param call_state call state
 * @param call_attr call attribute
 * @param call_cause cause that the call state changed
 * @param call_instance call instance id
 * @return void
 */
void CC_CallListener_callStateChanged(cc_call_handle_t call_handle,
		cc_call_state_t call_state,
		cc_call_attr_t call_attr,
		cc_cause_t call_cause,
		cc_call_instance_t call_instance);

/**
 * Inform the upper layer call control that the call attribute changed.
 * @param call_handle call handle
 * @param call_attr the call attr
 * @return void
 */
void CC_CallListener_callAttributeChanged(cc_call_handle_t call_handle,
		cc_call_attr_t call_attr);

/**
 * Inform the upper layer call control that a new call comes.
 * @param call_handle
 * @param call_security call security attribute
 * @return void
 */
void CC_CallListener_callSecurityChanged(cc_call_handle_t call_handle,
		cc_call_security_t call_security);

/**
 * Cancel a conference or Transfer feature. This is usually coming
 * from the CTI application to cancel the ongoing Xfer or Cnf calls.
 * @param call_handle call handle
 * @param other_call_handle the other call id that was cancelled
 * @return void
 */
void CC_CallListener_xferCnfCancelled(cc_call_handle_t call_handle,
		cc_call_handle_t other_call_handle);

/**
 * Update a call information about the call names or numbers.
 * @param call_handle call handle
 * @param clg_name calling party name
 * @param clg_number calling party number
 * @param cld_name called party name
 * @param cld_number called party number
 * @param alt_clg_number alternative calling party number
 * @param orig_name original name
 * @param orig_number original number
 * @param redir_name redirect name
 * @param redir_number redirect number
 * @param call_security call security
 * @param call_policy call policy
 * @param call_instance call instance
 * @param call_type
 * @return void
 */
void CC_CallListener_callInfoChanged(cc_call_handle_t call_handle,
		cc_string_t clg_name, cc_string_t clg_number,
		cc_string_t cld_name, cc_string_t cld_number,
		cc_string_t alt_clg_number,
		cc_string_t orig_name, cc_string_t orig_number,
		cc_string_t redir_name, cc_string_t redir_number,
		cc_call_security_t call_security,
		cc_call_policy_t call_policy,
		cc_call_instance_t call_instance,
		cc_call_type_t call_type);

/**
 * Update Global Call Identification for a call. GCID can be used to collapse
 * multiple call bubbles for the same call to a single call bubble. Please refer
 * to the collpased call bubble as specified by the roundtable UI Spec.
 * @param call_handle
 * @param gcid global call identification
 * @return void
 */
void CC_CallListener_callGCIDChanged(cc_call_handle_t call_handle, char* gcid);

/**
 * Update the placed call information. This API reports the dialed digits for the
 * placed call. The number specified by this API should be logged in the placed call information.
 * @param call_handle call_handle
 * @param cld_name called party name
 * @param cld_number called party number
 * @return void
 */
void CC_CallListener_callPlacedInfoChanged(cc_call_handle_t call_handle,
		cc_string_t cld_name,
		cc_string_t cld_number);

/**
 * Send call status to the upper layer for literally display purpose,
 * e.g., "HOLD" for a call was placed on held state.
 * @param call_handle call handle
 * @param status call status display string
 * @param timeout the time interval in seconds to display the status message on a call bubble if there is one.
 * @param prioroty the priority of display message.
 * @return void
 */
void CC_CallListener_callStatusChanged(cc_call_handle_t call_handle, cc_string_t status, int timeout, char priority);

/**
 * Inform the upper layer call control whether it is able to to delete the last dialed digit now.
 * @param call_handle call handle
 * @param state true or false whether it can delete last dialed digit.
 * @return void
 */
void CC_CallListener_backSpaceEnabled(cc_call_handle_t call_handle, cc_boolean state);

/**
 * Imform the upper layer that a call is locked remotely or not.
 * @param call_handle call handle
 * @param call_selection if the call is locked or not
 * @return void
 */
void CC_CallListener_callSelected(cc_call_handle_t call_handle,
		cc_call_selection_t call_selection);

/**
 * Inform the upper layer the log disposition
 * @param call_handle call handle
 * @param log_disp the log disposition
 * @return void
 */
void CC_CallListener_logDispositionUpdated(cc_call_handle_t call_handle,
		cc_log_disposition_t log_disp);

/**
 * Inform the upper layer that the last dialed digit was deleted.
 * @param call_handle call handle
 * @return void
 */
void CC_CallListener_lastDigitDeleted(cc_call_handle_t call_handle);

/**
 * Inform the upper layer that the call is in preservation state,
 * which means all features are disabled except to end the call.
 * e.g., "HOLD" for a call was placed on held state.
 * @param call_handle call handle
 * @return void
 */
void CC_CallListener_callPreserved(cc_call_handle_t call_handle);

/**
 * Inform the upper layer whether the video is available or not.
 * @param call_handle call handle
 * @param state video direction NONE => no video
 * @return void
 */
void CC_CallListener_videoAvailable(cc_call_handle_t call_handle, cc_int32_t state);

/**
 * Inform the upper layer that the remote has offered video capability.
 * @param call_handle call handle
 * @param direction the media direction
 * @return void
 */
void CC_CallListener_remoteVideoOffered(cc_call_handle_t call_handle, cc_sdp_direction_t direction);

/**************************END of Call Listener Methods***********************************/


#endif /* _CC_CALL_LISTENER_H_ */
