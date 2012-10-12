/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
