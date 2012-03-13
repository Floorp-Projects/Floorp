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

#ifndef _CCSIP_PLATFORM_H_
#define _CCSIP_PLATFORM_H_

#include "phone_platform_constants.h"

// SIP standard defines the max to be the path MTU if it is
// known, otherwise it should be 1500
// We are defining the max message size to be 2560, 1500 + 1060 bytes of
// extended buffer space (which is also the same as the PacketBuffer size
// #define PKTBUF_SIZ  3072 defined in phone.h
#define SIP_UDP_MESSAGE_SIZE   3072 /* Max size of a SIP message */

/* Constants */
#define MAX_SIP_URL_LENGTH          512
#define MAX_SIP_TAG_LENGTH          256
#define MAX_SIP_DISPLAYNAME_LENGTH  64
#define VIA_BRANCH_LENGTH           16

/*
 * MAX_SIP_DATE_LENGTH is used in ccsip_messaging for
 * the SIP Date header routine.
 * It has been defined here to be consistent with the rest
 * of the defines for the SIP message
 */
#define MAX_SIP_DATE_LENGTH         63  //Extended for foreign language
#define CCSIP_START_CSEQ            100
#define MAX_SIP_CALL_ID             128 /* Max size of a Call ID header string */
#define MAX_DIVERSION_HEADERS       25

/*********************************************************
 *
 *
 *  Phone constants
 *
 *
 *********************************************************/
#define MAX_REG_BACKUP         1    // Max number of backup registration CCBs
#define MIN_TEL_LINES          0    // Min number of lines
#define MAX_TEL_LINES          MAX_CALLS // Max number of calls
#define MAX_PHYSICAL_DIR_NUM   6    // Max number of physical directory numbers
#define MAX_UI_LINES_PER_DN    (MAX_LINES_PER_DN + 1) // Max number of lines per DN
#define MAX_CCBS               (MAX_TEL_LINES + MAX_REG_LINES + MAX_REG_BACKUP)
#define TEL_CCB_START          (MIN_TEL_LINES)
#define TEL_CCB_END            (TEL_CCB_START + MAX_TEL_LINES -1)
#define REG_CCB_START          (TEL_CCB_END + 1)
#define REG_CCB_END            (REG_CCB_START + MAX_REG_LINES -1)
#define REG_BACKUP_CCB         (REG_CCB_END + 1)
#define REG_FALLBACK_CCB_START (REG_BACKUP_CCB + 1)
#define REG_FALLBACK_CCB_END   (REG_FALLBACK_CCB_START +4)
#define REG_BACKUP_DN          1
#define REG_BACKUP_LINE        7
#define MAX_LINES_7940         2
#define INVALID_DN_LINE        (REG_FALLBACK_CCB_END + 1)
#define INIT_DN_LINE           0


void sip_platform_init(void);
int sip_platform_ui_restart(void);
extern void platform_print_sip_msg(const char *msg);

#endif
