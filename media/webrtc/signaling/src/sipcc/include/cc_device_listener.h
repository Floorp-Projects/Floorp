/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_DEVICE_LISTENER_H_
#define _CC_DEVICE_LISTENER_H_
#include "cc_constants.h"

/***********************************Device feature listener methods**************************************/
/**
 * Inform an application about the call forward status
 * @param line line number
 * @param is_fwd if the call forward set is a success or not.
 * @param is_local is cfwd a local configured or supportted by cucm
 * @param cfa_num call farward all number
 * @return void
 */
void CC_DeviceListener_callForwardAllStateChanged(cc_lineid_t line, cc_boolean is_fwd, cc_boolean is_local, cc_string_t cfa_num);

/**
 * Informa an application that a specifical line has message waiting.
 * @param line line number
 * @param state has message waiting or not
 * @param message_type type of message waiting, e.g. voice message, text message, etc.
 * @param new_count the count of new message waiting.
 * @param old_count the count of old message waiting.
 * @param high_priority_new_count the count of high priority message waiting.
 * @param high_priority_old_count the count of old high priority message waiting.
 * @return void
 */
void CC_DeviceListener_mwiStateUpdated(cc_lineid_t line, cc_boolean state,
		cc_message_type_t message_type,
		int new_count,
		int old_count,
		int high_priority_new_count,
		int high_priority_old_count);

/**
 * Update the handset lamp light
 * @param lamp_state lamp state
 * @return void
 */
void CC_DeviceListener_mwiLampUpdated(cc_lamp_state_t lamp_state);

/**
 * Inform application if the maximum number of call for the particular line is reached or not.
 * @param line the line is reported
 * @param state true or false
 * @return void
 */
void CC_DeviceListener_mncReached(cc_lineid_t line, cc_boolean state);

/**
 * Call Status/prompt notifications
 * @param time display duration
 * @param display_progress
 * @param priority see definition for notification priority in cc_constants.h
 * @param prompt phrase to display
 * @return void
 */
void CC_DeviceListener_displayNotify(int time, cc_boolean display_progress, char priority, cc_string_t prompt);

/**
 * Update speed dial label
 * @param line the line number
 * @param button_no the button number
 * @param speed_dial_number the speed dial number
 * @param label the label to display for the line if line visual presentation is available.
 * @return void
 */
void CC_DeviceListener_labelNSpeedUpdated(cc_lineid_t line,
		int button_no,
		cc_string_t speed_dial_number,
		cc_string_t label);

#endif /* _CC_DEVICE_LISTENER_H_ */
