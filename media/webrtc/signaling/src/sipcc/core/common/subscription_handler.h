/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SUB_HANDLER_H__
#define __SUB_HANDLER_H__

boolean sub_hndlr_isAlertingBLFState(int inst);

boolean sub_hndlr_isInUseBLFState(int inst);

boolean sub_hndlr_isAvailable();

void sub_hndlr_start();

void sub_hndlr_stop();

void sub_hndlr_controlBLFButtons(boolean state);

void sub_hndlr_NotifyBLFStatus(int requestId, int status, int appId);

#endif

