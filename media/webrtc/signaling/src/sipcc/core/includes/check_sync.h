/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CHECK_SYNC_H
#define CHECK_SYNC_H

#include "util_parse.h"

typedef enum {
    SYNCS_ANY,
    SYNCS_GOT_VERSION,
    SYNCS_GOT_VERSION_EQ,
    SYNCS_GOT_SYNC,
    SYNCS_GOT_SYNC_EQ,
    SYNCS_GOT_GENERIC,
    SYNCS_GOT_GENERIC_EQ
} sync_states;

typedef enum {
    SYNCF_NONE,
    SYNCF_MY,
    SYNCF_WILD
} SyncVersionFound_t;

typedef struct {
    char myVersion[MAX_LOAD_ID_STRING];
    char resetSync[MAX_SYNC_LEN];
    SyncVersionFound_t versionFound;
} VersionSync_t;

#define SYNCINFO_XML    "syncinfo.xml"
#define SYNC_BUF_SIZE   4096

extern int CheckSync;
extern int SyncInfoInProgress;
extern char *SyncBuffer;

int check_sync_get(void);
void add_sync_info(char *version, char *sync, VersionSync_t *versionSync);
int parse_sync_entry(char **parseptr, VersionSync_t *versionSync);
int parse_sync_info(char *parseptr, VersionSync_t *versionSync);
void process_sync_info(char *syncInfo);
short handle_sync_message(short cmd, void *pData);

#endif
