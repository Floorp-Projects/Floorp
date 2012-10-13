/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
