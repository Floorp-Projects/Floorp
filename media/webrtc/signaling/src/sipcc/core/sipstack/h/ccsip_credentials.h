/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_CREDENTIALS_H_
#define _CCSIP_CREDENTIALS_H_

#include "cpr_types.h"
#include "prot_configmgr.h"

#define CRED_MAX_ID_LEN AUTH_NAME_SIZE
#define CRED_MAX_PW_LEN 32

#define CRED_USER       0x00000001
#define CRED_LINE       0x00000002


typedef struct _credentials {
    char id[CRED_MAX_ID_LEN];
    char pw[CRED_MAX_PW_LEN];
} credentials_t;

#endif
