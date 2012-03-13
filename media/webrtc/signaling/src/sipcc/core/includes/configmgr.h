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
