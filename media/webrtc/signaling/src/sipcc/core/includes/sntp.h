/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
