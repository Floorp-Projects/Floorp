/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MISC_APP_TASK_H
#define MISC_APP_TASK_H

extern cprMsgQueue_t s_misc_msg_queue;

extern cpr_status_e MiscAppTaskSendMsg(uint32_t cmd, cprBuffer_t buf,
                                       uint16_t len);
extern void MiscAppTask(void *arg);

#endif /* MISC_APP_TASK_H */
