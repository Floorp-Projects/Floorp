/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CONFIGMGR_H_
#define _CONFIGMGR_H_

/*
 * #defines for maximum length of the image names
 * The MAX_LOAD_ID_LENGTH must be larger (or the same as)
 * MAX_OLD_LOAD_ID_LENGTH or strange things may
 * happen because they are used as indexes into
 * arrays.
 */
#include "cpr_types.h"
#define MAX_LOAD_ID_LENGTH     60
#define MAX_LOAD_ID_STRING     MAX_LOAD_ID_LENGTH + 1
#define MAX_OLD_LOAD_ID_LENGTH  8
#define MAX_OLD_LOAD_ID_STRING  MAX_OLD_LOAD_ID_LENGTH + 1
#define MAX_URL_LENGTH 128

#define MAX_SYNC_LEN       33
#define MAX_PHONE_LABEL    32

/* Start of section for new "protocol-inspecific" calls */

#define MAX_CONFIG_STRING_NAME  64
#define DEFAULT_LINE 1

#define YESSTR    "YES"
#define NOSTR     "NO"
#define IPOFZEROS "0.0.0.0"

enum ACTIONATTR {
    AA_IGNORE   = 0,
    AA_COMMIT   = 1,
    AA_RELOAD   = 1 << 1,
    AA_REGISTER = 1 << 2,
    AA_FORCE    = 1 << 3,
    AA_RESET    = 1 << 4,
    AA_SETTINGS = 1 << 5,
    AA_BU_REG   = 1 << 6
};

/*********************************************************
 *
 *  External Function Prototypes
 *
 *********************************************************/
void config_get_string(int id, char *buffer, int buffer_len);
void config_set_string(int id, char *buffer);
void config_get_value(int id, void *buffer, int length);
void config_set_value(int id, void *buffer, int length);

void config_get_line_string(int id, char *buffer, int line, int buffer_len);
void config_set_line_string(int id, char *buffer, int line);
void config_get_line_value(int id, void *buffer, int length, int line);
void config_set_line_value(int id, void *buffer, int length, int line);

void config_init(void);

#endif /* _CONFIGMGR_H_ */
