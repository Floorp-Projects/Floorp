/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SIP_CSPS_TRANSPORT_H__
#define __SIP_CSPS_TRANSPORT_H__

#include "cpr_types.h"
#include "phone_types.h"
#include "phone_debug.h"
#include "cfgfile_utils.h"
#include "configmgr.h"
#include "ccsip_protocol.h"
#include "ccsip_pmh.h"
#include "ccsip_platform_timers.h"
#include "ccsip_platform_udp.h"
#include "ccsip_messaging.h"

/*
 * Defines for Primary, Secondary and Tertiary CC
 */
typedef enum {
    PRIMARY_CSPS = 0,
    MAX_CSPS
} CSPS_ID;

//extern csps_config_info_t CSPS_Config_Table;

cpr_socket_t sipTransportCSPSGetProxyHandleByDN(line_t dn);
short    sipTransportCSPSGetProxyPortByDN(line_t dn);
uint16_t sipTransportCSPSGetProxyAddressByDN(cpr_ip_addr_t *pip_addr,
                                             line_t dn);

uint16_t sip_config_get_proxy_port(line_t line);
uint16_t sip_config_get_backup_proxy_port(void);
void     sip_config_get_proxy_addr(line_t line, char *buffer, int buffer_len);
uint16_t sip_config_get_backup_proxy_addr(cpr_ip_addr_t *IPAddress,
                                           char *buffer, int buffer_len);

extern sipPlatformUITimer_t sipPlatformUISMTimers[];
extern ccsipGlobInfo_t gGlobInfo;

extern int dns_error_code; // DNS errror code global
void sipTransportCSPSClearProxyHandle(cpr_ip_addr_t *ipaddr, uint16_t port,
                                      cpr_socket_t this_fd);

#endif /* __SIP_CSPS_TRANSPORT_H__ */
