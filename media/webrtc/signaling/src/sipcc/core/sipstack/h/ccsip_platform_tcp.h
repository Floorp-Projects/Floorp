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

#ifndef __CCSIP_PLATFORM_TCP__H__
#define __CCSIP_PLATFORM_TCP__H__

/* The maximum number of connections allowed */
#define MAX_SIP_CONNECTIONS (64 - 2)

#define SIP_TCP_SEND_OK 0
#define SIP_TCP_SIZE_ERROR 1
#define SIP_TCP_SEND_ERROR 2
#define SIP_SOC_TCP 0
#define SIP_SOC_TLS 1

#define SIP_TCP_NOT_CONNECTED 0
#define SIP_TCP_CONNECTED 1
	 
typedef struct _sendData
{
    struct _sendData *next;
    char             *data;
    uint16_t          bytesLeft;
    uint16_t          bytesSent;
    void             *context;
    boolean           msg_display;
    uint8_t           ip_sig_tos;
} ccsipTCPSendData_t;

typedef struct
{
    uint16 connectionId;
} sipTCPTimerContext_t;

typedef struct
{
    cpr_socket_t      fd;           /* Socket file descriptor */
    cpr_sockaddr_storage addr;
    cpr_ip_addr_t     ipaddr;       /* Remote IP address */
    uint16_t          port;         /* Remote port # */
    sock_state_t      state;
    char             *prev_msg;
    uint32_t          prev_bytes;
    void             *context;      /* Connection Manager's context */
    /* queue for partial socket writes */
    sll_handle_t      sendQueue;
    boolean           dirtyFlag;
    boolean           pend_closure; /* Indicates to close the connection
                                     * entry on completion of partial
                                     * socket message writes
                                     */
    int               error_cause;
    int               soc_type;
} sip_tcp_conn_t;

typedef struct
{
    cpr_socket_t read[MAX_SIP_CONNECTIONS];
    cpr_socket_t write[MAX_SIP_CONNECTIONS];
} sip_connection_t;


extern sip_connection_t sip_conn;
extern uint32_t nfds;
extern fd_set read_fds;
extern fd_set write_fds;
extern sip_tcp_conn_t sip_tcp_conn_tab[MAX_CONNECTIONS];

/*
 *
 * Function declarations
 *
 */
int sip_tcp_fd_to_connid(cpr_socket_t fd);
cpr_socket_t sip_tcp_create_connection(sipSPIMessage_t *spi_msg);
void sip_tcp_read_socket(cpr_socket_t this_fd);
int sip_tcp_channel_send(cpr_socket_t s,
                         char *buf,
                         uint32_t len);
void sip_tcp_createconnfailed_to_spi(cpr_ip_addr_t *ipaddr,
                                     uint16_t port,
                                     void *context,
                                     ccsipSockErrCodes_e errcode,
                                     int connid);
void sip_tcp_purge_entry(sipSPIConnId_t connid);
void sipTcpFlushRetrySendQueue(sip_tcp_conn_t *entry);
void sip_tcp_resend(int connid);
extern int sip_tcp_get_free_conn_entry(void);
extern int sip_tcp_attach_socket(cpr_socket_t s);
extern void sip_tcp_init_conn_table(void);
extern boolean sip_tcp_set_sock_options(int fd);
void sipTransportClearServerHandle(cpr_ip_addr_t *ipaddr,
                                   uint16_t port,
                                   int connid);
void sipTcpFreeSendQueue(int connid);
extern cpr_socket_t sip_tcp_create_conn_using_blocking_socket (sipSPIMessage_t *spi_msg);

#endif /* __CCSIP_PLATFORM_TCP__H__ */
