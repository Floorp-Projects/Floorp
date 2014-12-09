/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_MACROS_H_
#define _CCSIP_MACROS_H_


#define EVENT_ACTION_SM(x) \
        (gSIPHandlerTable[x])

#define UPDATE_FLAGS(x, y) \
        (x = (x == STATUS_SUCCESS) ? y : x)

#define GET_SIP_MESSAGE() sippmh_message_create()

#define REG_CHECK_EVENT_SANITY(x, y) \
        ((x - SIP_REG_STATE_BASE >= 0) && (x <= SIP_REG_STATE_END) && \
         (y - SIPSPI_REG_EV_BASE >=0)  && (y <= SIPSPI_REG_EV_END))

#define REG_EVENT_ACTION(x, y) \
        (gSIPRegSMTable[x - SIP_REG_STATE_BASE][y - SIPSPI_REG_EV_BASE])

#endif
