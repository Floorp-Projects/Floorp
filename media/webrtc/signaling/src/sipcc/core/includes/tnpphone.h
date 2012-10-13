/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TNPPHONE_H
#define TNPPHONE_H

#include "phone.h"


#ifdef SIP_OS_WINDOWS


#ifndef MAX_FILE_NAME
#define MAX_FILE_NAME 256
#endif

/* Defined in net_config.h */
typedef struct
{
    uint8_t  fName[MAX_FILE_NAME];   // Name of file being transfered
    uint32_t rcvLen;
    uint32_t maxSize;           // The maximum size of the file to be read
    uint8_t *fDest;             // The destination address for transfer
    uint32_t fHost;             // Foreign IP address
    uint16_t rtCode;
    uint8_t  taskId;
    irx_lst_blk *pTaskList;
} TftpOpen;                     // TFTP Request

#define CFG_PROGRAMMED   0x12300000L
#define CFG_PROGRAM_TFTP 0x12200000L
#define CFG_DHCP_DISABLE 0x00000001L
#define CFG_DHCP_TIMEOUT 0x00000002L
#define CFG_TFTP_TIMEOUT 0x00000004L
#define CFG_TFTP_NTFOUND 0x00000008L
#define CFG_TFTP_ACCESS  0x00000010L
#define CFG_TFTP_ERR     0x00000020L
#define CFG_DNS_ERROR    0x00000040L
#define CFG_DNS_TIMEOUT  0x00000080L
#define CFG_DNS_IP       0x00000100L
#define CFG_VER_ERR      0x00000200L
#define CFG_CKSUM_ERR    0x00000400L
//#define CFG_TFTP_FILE    0x00000800L
#define CFG_DFLT_GWY     0x00001000L
#define CFG_DUP_IP       0x00002000L
#define CFG_PATCHED      0x00004000L
#define CFG_LINK_DOWN    0x00008000L
#define CFG_FIXED_TFTP   0x00010000L
#define CFG_PROG_ERR     0x00020000L
#define CFG_BOOTP        0x00040000L
#define CFG_DHCP_RELEASE 0x00080000L


#ifndef ERROR
#define ERROR   (-1)
#endif

#ifndef PHONE_TICK_TO
#define PHONE_TICK_TO 10
#endif


#endif
#endif
