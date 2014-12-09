/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _TNP_BLF_H
#define _TNP_BLF_H

#ifdef __cplusplus
extern "C" {
#endif
#include "sessionConstants.h"

void blf_notification(int request_id, int status, int app_id);

#ifdef __cplusplus
}
#endif

#endif
