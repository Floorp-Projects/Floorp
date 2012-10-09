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

/**
 * Management Events
 */
 typedef enum {
 EV_CC_CREATE=0,
 EV_CC_START,
 EV_CC_CONFIG_RECEIVED ,
 EV_CC_DO_SOFT_RESET ,
 EV_CC_INSERVICE,
 EV_CC_OOS_FAILOVER,
 EV_CC_OOS_FALLBACK,
 EV_CC_OOS_REG_ALL_FAILED,
 EV_CC_OOS_SHUTDOWN_ACK,
 EV_CC_RE_REGISTER,
 EV_CC_STOP,
 EV_CC_DESTROY,
 EV_CC_IP_VALID,
 EV_CC_IP_INVALID
} mgmt_event_t;


/**
 * Management states
 */
typedef enum {
MGMT_STATE_CREATED=0,
MGMT_STATE_IDLE,
MGMT_STATE_REGISTERING,
MGMT_STATE_REGISTERED,
MGMT_STATE_OOS,
MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK,
MGMT_STATE_WAITING_FOR_CONFIG_FILE,
MGMT_STATE_OOS_AWAIT_UN_REG_ACK,
MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK,
MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK
} mgmt_state_t;

extern void registration_processEvent(int event);
cc_boolean is_phone_registered();
