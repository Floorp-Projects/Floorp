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

#ifndef __SIP_COMMON_TRANSPORT_H__
#define __SIP_COMMON_TRANSPORT_H__

#include "cpr_types.h"
#include "cpr_socket.h"
#include "ccsip_pmh.h"
#include "sip_csps_transport.h"
#include "sip_ccm_transport.h"
#include "singly_link_list.h"


#define MAX_CONNECTIONS 5

/* Macro definition for chacking a valid connid (TCP/UDP) */
#define VALID_CONNID(connid) \
       (connid >= 0 && connid < MAX_CONNECTIONS)

typedef enum {
    NONE_CC = 0,
    ACTIVE_CC = 1,
    STANDBY_CC
} CC_POSITION;

/*
 * Define the types of connections
 */
/*
 * NOTE: Need to match the values in the config matrix
 */
typedef enum {
    CONN_NONE = 0,
    CONN_TCP,
    CONN_UDP,
    CONN_TLS,
    CONN_TCP_TMP,
    CONN_MAX_TYPES
} CONN_TYPE;

// Device Security Modes
typedef enum sec_level_t_ {
    NON_SECURE = 0,  // Normal, no security
    AUTHENTICATED,   // Use TLS, server will use NULL encryption
    ENCRYPTED,       // Use TLS, server will use AES encryption
    NOT_IN_CTL       // Not in CTL should not be seen by SIP.
} sec_level_t;

typedef enum conn_create_status_t_ {
    CONN_INVALID = -1,
    CONN_SUCCESS,
    CONN_FAILURE
} conn_create_status_t;

typedef struct ti_common_t_ {
    uint16_t     listen_port;
    char         addr_str[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t     addr;
    uint16_t     port;
    uint16_t     sec_port;
    CONN_TYPE    conn_type;
    CONN_TYPE    configured_conn_type;
    cpr_socket_t handle;
} ti_common_t;

typedef struct ti_ccm_t_ {
    CCM_ID  ccm_id;
    int32_t sec_level;
    int32_t is_valid;
} ti_ccm_t;

typedef struct ti_csps_t_ {
    char     bkup_pxy_addr_str[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t bkup_pxy_addr;
    uint16_t bkup_pxy_port;
    char     emer_pxy_addr_str[MAX_IPADDR_STR_LEN];
    uint16_t emer_pxy_port;
    char     outb_pxy_addr_str[MAX_IPADDR_STR_LEN];
    uint16_t outb_pxy_port;
} ti_csps_t;

typedef struct ti_config_table_t_ {
    CC_ID       cc_type;
    ti_common_t ti_common;
    union {
        ti_ccm_t   ti_ccm;
        ti_csps_t *ti_csps;
    } ti_specific;
} ti_config_table_t;

extern ti_config_table_t *CCM_Config_Table[MAX_REG_LINES + 1][MAX_CCM];
extern ti_config_table_t CCM_Dummy_Entry;
extern ti_config_table_t CSPS_Config_Table[MAX_REG_LINES];
typedef struct cc_config_table_t_ {
    CC_ID cc_type;
    void *cc_table_entry; // Needs to deferenced as
    // ti_config_table_t*
} cc_config_table_t;

typedef long sipSPIConnId_t;

typedef enum {
    SOCKET_NO_ERROR = -1,
    SOCKET_SEND_ERROR,
    SOCKET_RECV_ERROR,
    SOCKET_OPEN_ERROR,
    SOCKET_OPT_ERROR,
    SOCKET_CONNECT_ERROR,
    SOCKET_CONN_REFUSED_ERROR,
    SOCKET_BIND_ERROR,
    SOCKET_REMOTE_CLOSURE,
    SOCKET_ADMIN_CLOSURE,
    SIP_TCP_CONN_TABLE_FULL
} ccsipSockErrCodes_e;

/* All the possible state of the TCP/UDP sockets */
typedef enum {
    SOCK_IDLE,            /* not inuse */
    SOCK_LISTENING,       /* fd is listening on the well-known port */
    SOCK_ACCEPTED,        /* connection accepted          */
    SOCK_CONNECTED,       /* connection made              */
    SOCK_CONNECT_PENDING, /* connection is pending */
    SOCK_FAILED           /* failed, will be closed when all calls fail */
} sock_state_t;

typedef struct {
    cpr_ip_addr_t   addr;
    uint16_t        port;
    CONN_TYPE       transport;  /* Specifies UDP, Multicast UDP, TCP */
    uint8_t         ip_sig_tos;
    uint16_t        local_listener_port;
} sipSPICreateConnection_t;

typedef struct {
    void *context;
    sipSPICreateConnection_t createConnMsg;
} sipSPIMessage_t;


void sipTransportSetServerHandleAndPort(cpr_socket_t socket_handle,
                                        uint16_t listen_port,
                                        ti_config_table_t *ccm_table_entry);
int sip_dns_gethostbysrv(char *domain,
                         cpr_ip_addr_t *ipaddr_ptr,
                         uint16_t *port,
                         srv_handle_t *srv_order,
                         boolean retried_addr);
int sip_dns_gethostbysrvorname(char *hname,
                               cpr_ip_addr_t *ipaddr_ptr,
                               uint16_t *port);

conn_create_status_t sip_transport_setup_cc_conn(line_t dn, CCM_ID ccm_id);
int sip_transport_destroy_cc_conn(line_t dn, CCM_ID ccm_id);

int16_t SIPTaskGetProxyPortByDN(line_t dn);
uint32_t SIPTaskGetProxyAddressByDN(line_t dn);
cpr_socket_t SIPTaskGetProxyHandleByDN(line_t dn);

int sipTransportCreateSendMessage(ccsipCCB_t *ccb,
                                  sipMessage_t *pSIPMessage,
                                  sipMethod_t message_type,
                                  cpr_ip_addr_t *cc_remote_ipaddr,
                                  uint16_t cc_remote_port,
                                  boolean isRegister,
                                  boolean reTx,
                                  int timeout, void *scbp,
                                  int reldev_stored_msg);
#define sipTransportChannelCreateSend(a, b, m, c, d, e, f) \
sipTransportCreateSendMessage(a, b, m, c, d, FALSE, TRUE, e, NULL, f)

int sipTransportSendMessage(ccsipCCB_t *ccb,
                            char *pOutMessageBuf,
                            uint32_t nbytes,
                            sipMethod_t message_type,
                            cpr_ip_addr_t *cc_remote_ipaddr,
                            uint16_t cc_remote_port,
                            boolean isRegister,
                            boolean reTx,
                            int timeout,
                            void *scbp);
#define sipTransportChannelSend(a, b, c, m, d, e, f) \
sipTransportSendMessage(a, b, c, m, d, e, FALSE, TRUE, f, NULL)

int sipTransportGetServerAddrPort(char *domain,
                                  cpr_ip_addr_t *ipaddr_ptr,
                                  uint16_t *port,
                                  srv_handle_t *psrv_order,
                                  boolean retried_addr);
int      sipTransportGetPrimServerPort(line_t line);
int      sipTransportGetBkupServerPort(line_t line);
int      sipTransportGetEmerServerPort(line_t line);
int      sipTransportGetOutbProxyPort(line_t line);
cpr_ip_type     sipTransportGetPrimServerAddress(line_t line, char *buffer);
uint16_t  sipTransportGetBkupServerAddress(cpr_ip_addr_t *pip_addr, 
                                            line_t line, char *buffer);
void     sipTransportGetEmerServerAddress(line_t line, char *buffer);
void     sipTransportGetOutbProxyAddress(line_t line, char *buffer);
void sipTransportGetServerIPAddr(cpr_ip_addr_t *pip_addr, line_t line);
int      sipTransportInit(void);
uint16_t sipTransportGetServerAddress(cpr_ip_addr_t *pip_addr, line_t dn, line_t index);
short    sipTransportGetServerPort(line_t dn, line_t index);

int      sipTransportGetCCType(int line, void *cc_table_entry);
void     sip_regmgr_set_cc_info(line_t line, line_t dn_line,
                                CC_ID *cc_type, void *cc_table_entry);
uint16_t sipTransportGetListenPort(line_t line, ccsipCCB_t *ccb);
const char *sipTransportGetTransportType(line_t line, boolean upper_case,
                                         ccsipCCB_t *ccb);

void     sipTransportShutdown(void);
extern void ccsip_dump_send_msg_info(char *msg, sipMessage_t *pSIPMessage, 
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port);
void SIPTaskProcessTCPMessage(sipMessage_t *pSipMessage,
                              cpr_sockaddr_storage from);

void
sipTransportSetSIPServer();


#endif /* __SIP_COMMON_TRANSPORT_H__ */
