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

#ifndef SNTP_H
#define SNTP_H

#include <cpr_types.h>
#include <cpr_stdio.h>
#include <cpr_string.h>
#include <cpr_socket.h>

#include <time2.h>
// Begin system specific includes
//#include <irx.h>
#include <dns_utils.h>
#include <phone_debug.h>
#include <logger.h>
#include <logmsg.h>
#include <phone.h>
#include <text_strings.h>
// End system specific includes

#define MAX_IPADDR_STR_LEN 48

#define NTP_UTC_OFFSET     (2208963600)
#define PORT_NTP           (123)
#define MULTICAST_ADDR_NTP (0xE0000101L)
#define BROADCAST_ADDR     (0xFFFFFFFF)
#define TIMEPORT           (37)

typedef enum { unicast, multicast, anycast, directed_broadcast } SNTP_Mode;

typedef struct {
    unsigned int precision:8;
    unsigned int poll:8;
    unsigned int stratum:8;
    unsigned int mode:3;
    unsigned int versionNumber:3;
    unsigned int leapIndicator:2;
} NTPHeader_s;

typedef struct {
    union {
        NTPHeader_s   header;
        unsigned long rawheader;
    } u;
    unsigned long rootDelay;
    unsigned long rootDispersion;
    unsigned long referenceIdentifier;
    unsigned long referenceTimestamp[2];
    unsigned long originateTimestamp[2];
    unsigned long receiveTimestamp[2];
    unsigned long transmitTimestamp[2];
    // Removed the following fields since they are version 4
    // specific.
    //unsigned long   keyIdentifier;
    //unsigned long   messageDigest[4];
} NTPStruct_s;

typedef struct {
    Socket        *server_socket;             // Socket to SNTP server
    Socket        *lsocket;                   // Local listening socket
    Socket        *bsocket;                   // Broadcast listening socket
    Socket        *msocket;                   // Multicast listening socket
    char          address[MAX_IPADDR_STR_LEN];
    unsigned long sntp_server_addr;
    SNTP_Mode     mode;

    unsigned long destinationTimestamp[2];
    long          roundtripDelay[2];
    long          timeOffset[2];
    long          time_zone;
} SNTP_State;

typedef union {
    unsigned long NTPDataBuffer[17];
    NTPStruct_s   NTPData;
} NTPPacket_u;

void SNTPInit(void);
void SNTPStop(void);
int SNTPSend(void);
void SNTPCallback(irx_tmr_buf *pTmrBlk);
int SNTPRecv(SysHdr *pSm);
long NTPSemanticCheck(const NTPStruct_s *ntpdata);
void SNTPSecondUpdate(void);
void printNTPStruct(const NTPStruct_s *ntpdata, const SNTP_State *state);
long sntp_get_rand_seed(void);
void SNTPDebugInit(void);

#endif
