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
