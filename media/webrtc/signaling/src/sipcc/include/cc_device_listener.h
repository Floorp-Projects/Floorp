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
