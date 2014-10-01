/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CCAPP_TASK_H__
#define __CCAPP_TASK_H__
#include "sll_lite.h"

//Define app id for ccapp task
#define CCAPP_CCPROVIER     1
#define CCAPP_MSPROVIDER    2

typedef void(* appListener) (void *message, int type);
typedef struct {
    sll_lite_node_t node;
    int type;
    appListener *listener_p;
} listener_t;

extern void addCcappListener(appListener* listener, int type);
appListener *getCcappListener(int type);
cpr_status_e ccappTaskSendMsg (uint32_t cmd, void *msg, uint16_t len, uint32_t usrInfo);

#endif
