/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIP_PLATFORM_TASK_H_
#define _SIP_PLATFORM_TASK_H_

#include "cpr_socket.h"

/*
 * Prototypes
 */
void sip_platform_task_loop(void *arg);
void sip_platform_task_set_listen_socket(cpr_socket_t s);
void sip_platform_task_set_read_socket(cpr_socket_t s);
void sip_platform_task_clr_read_socket(cpr_socket_t s);

void sip_platform_task_reset_listen_socket(cpr_socket_t s);

#endif
