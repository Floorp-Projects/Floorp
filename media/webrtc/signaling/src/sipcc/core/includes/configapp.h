/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONFIGAPP_H
#define CONFIGAPP_H

extern void configapp_init();
extern void configapp_shutdown();
extern void configapp_process_msg(uint32_t cmd, void *msg);

#endif

