/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
