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

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_errno.h"
#include "cpr_socket.h"
#include "cpr_in.h"
#include "ccsip_core.h"
#include "ccsip_task.h"
#include "sip_platform_task.h"
#include "ccsip_platform_udp.h"
#include "sip_common_transport.h"
#include "sip_common_regmgr.h"
#include "phone_debug.h"
#include "util_string.h"
#include "ccsip_platform_tcp.h"
#include "ccsip_platform_timers.h"
#include "text_strings.h"
#include "ccsip_register.h"
#include "phntask.h"
#include "plat_api.h"
#include "sip_socket_api.h"


/*
 * Externs
 */
extern cc_config_table_t CC_Config_Table[];
extern ccm_act_stdby_table_t CCM_Active_Standby_Table;
extern cpr_sockaddr_t *sip_set_sockaddr(cpr_sockaddr_storage *psock_storage, uint16_t family, 
                                 cpr_ip_addr_t ip_addr, uint16_t port, uint16_t *addr_len);
extern void ccsip_dump_recv_msg_info(sipMessage_t *pSIPMessage, 
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port);

#define MAX_CHUNKS 12
#define MAX_PAYLOAD_SIZE  (MAX_CHUNKS*CPR_MAX_MSG_SIZE)
/*
 * Globals
 */
static uint32_t sip_tcp_incomplete_msg = 0;
static uint32_t sip_tcp_fail_network_msg = 0;
int max_tcp_send_msg_q_size = 0;
int max_tcp_send_msg_q_connid = 0;
cpr_ip_addr_t max_tcp_send_msg_q_ipaddr = {0,{0}};
ushort max_tcp_send_msg_q_port = 0;

/*
 * The following routine that set the socket option has been
 * ported over from IOS. So not renaming.
 */
static ccsipRet_e
ccsipSocketSetNonblock (cpr_socket_t fd, int optval)
{
    const char  *fname = "ccsipSocketSetNonblock";

    if (cprSetSockNonBlock(fd)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set non-blocking socket mode %d\n",
                            fname, cpr_errno);
        return SIP_INTERNAL_ERR;
    }
    return SIP_SUCCESS;
}

/*
 * The following routine that set the socket option has been
 * ported over from IOS. So not renaming.
 */
static ccsipRet_e
ccsipSocketSetKeepAlive (cpr_socket_t fd, int optval)
{
    const char  *fname = "ccsipSocketSetKeepAlive";

    if (cprSetSockOpt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval,
                      sizeof(optval))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set KEEP ALIVE on a socket %d\n",
                                fname, cpr_errno);
        return SIP_INTERNAL_ERR;
    }

    return SIP_SUCCESS;
}

/*
 * The following routine that set the socket option has been
 * ported over from IOS. So not renaming.
 */
#ifdef NOT_AVAILABLE_WIN32
static ccsipRet_e
ccsipSocketSetTCPtos (cpr_socket_t fd, uint8_t optval)
{
    const char  *fname = "ccsipSocketSetTCPtos";

    if (cprSetSockOpt(fd, SOL_TCP, TCP_TOS, (void *)&optval,
                      sizeof(optval))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set TCP TOS on a socket %d\n",
                                fname, cpr_errno);
        return SIP_INTERNAL_ERR;
    }

    return SIP_SUCCESS;
}

/*
 * The following routine that set the socket option has been
 * ported over from IOS. So not renaming.
 */
static ccsipRet_e
ccsipSocketSetPushBit (cpr_socket_t fd, int optval)
{
    const char  *fname = "ccsipSocketSetPushBit";

    if (cprSetSockOpt(fd, SOL_TCP, TCP_ALWAYSPUSH, (void *)&optval,
                      sizeof(optval))) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set PUSH BIT on a socket %d\n",
                                fname, cpr_errno);
        return SIP_INTERNAL_ERR;
    }

    return SIP_SUCCESS;
}
#endif /* NOT_AVAILABLE_WIN32 */

static boolean ccsipIsSecureType(sipSPIConnId_t connid) 
{
    if (sip_tcp_conn_tab[connid].soc_type == SIP_SOC_TLS) {
        return TRUE;
    } 
    return FALSE;
}
/**
 *
 * sip_tcp_attach_socket
 *
 * Attach the socket to the select call
 *
 * Parameters:   s - the socket
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 * Remarks: No check is made to see if socket is already attached
 *
 */
int
sip_tcp_attach_socket (cpr_socket_t s)
{
    int i;

    /*
     * Attach socket to select call
     */
    for (i = 0; i < MAX_SIP_CONNECTIONS; i++) {
        if (sip_conn.read[i] == INVALID_SOCKET) {
            sip_conn.read[i] = s;
            FD_SET(s, &read_fds);
            nfds = MAX(nfds, (uint32_t)s);
            sip_conn.write[i] = s;
            FD_SET(s, &write_fds);
            break;
        }
    }

    /*
     * Are there already too many connections?
     */
    if (i == MAX_SIP_CONNECTIONS) {
        return SIP_ERROR;
    }
    return SIP_OK;
}

/**
 *
 * sip_tcp_detach_socket
 *
 * Attach the socket to the select call
 *
 * Parameters:   s - the socket
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 * Remarks: No check is made to see if socket is already attached
 *
 */
static int
sip_tcp_detach_socket (cpr_socket_t s)
{
    int i;
    const char *fname = "sip_tcp_detach_socket";

    if (s == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Invalid socket\n", fname);
        return SIP_ERROR;
    }
    /*
     * Attach socket to select call
     */
    for (i = 0; i < MAX_SIP_CONNECTIONS; i++) {
        if (sip_conn.read[i] == s) {
            sip_conn.read[i] = INVALID_SOCKET;
            FD_CLR(s, &read_fds);
            nfds = MAX(nfds, (uint32_t)s);
            sip_conn.write[i] = INVALID_SOCKET;
            FD_CLR(s, &write_fds);
            break;
        }
    }

    /*
     * Are there already too many connections?
     */
    if (i == MAX_SIP_CONNECTIONS) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Max TCP connections reached.\n", fname);
        return SIP_ERROR;
    }
    return SIP_OK;
}

/**
 *
 * sip_tcp_set_sock_options
 *
 * Attach the socket to the select call
 *
 * Parameters:   fd - the file descriptor
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 * Remarks: No check is made to see if socket is already attached
 *
 */
boolean
sip_tcp_set_sock_options (int fd)
{
    int optval;
    ccsipRet_e status = SIP_SUCCESS;

    optval = 1;

    /* Set non-blocking mode */
    status = ccsipSocketSetNonblock(fd, optval);
    if (status != SIP_SUCCESS) {
        return FALSE;
    }
	
    /* Set the keepalive option */
    status = ccsipSocketSetKeepAlive(fd, optval);
    if (status != SIP_SUCCESS) {
        return FALSE;
    }

    return TRUE;
}

/**
 *
 * sip_tcp_fd_to_connid
 *
 * returns the tcp conn table index for a particular socket
 *
 * Parameters:   fd - the file descriptor
 *
 * Return Value: sip_tcp_conn_tab index
 *
 */
int
sip_tcp_fd_to_connid (cpr_socket_t fd)
{
    int i;

    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (sip_tcp_conn_tab[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

/*
 * sip_tcp_get_free_conn_entry ()
 *
 * Description  : This procedure returns the first free entry from the TCP
 *                conn table.
 *
 * Input Params : None.
 *
 * Returns      : Index to the Connection table (if SUCCESSFUL)
 *                -1 in case of failure.
 */
int
sip_tcp_get_free_conn_entry (void)
{
    int i;
    const char *fname = "sip_tcp_get_free_conn_entry";

    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (sip_tcp_conn_tab[i].fd == -1) {
            /* Zero the connection table entry */
            memset((sip_tcp_conn_tab + i), 0, sizeof(sip_tcp_conn_t));
            sip_tcp_conn_tab[i].state = SOCK_IDLE;
            sip_tcp_conn_tab[i].dirtyFlag = FALSE;
            sip_tcp_conn_tab[i].error_cause = SOCKET_NO_ERROR;
            return i;
        }
    }

    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"TCP Connection table full\n", fname);

    return -1;
}

/*
 * sip_tcp_init_conn_table()
 * Description : Cleans up the entry associated with the connid
 *               from the sip_tcp_conn_tab
 *
 * Input : void
 *
 * Output : void
 *
 */
void
sip_tcp_init_conn_table (void)
{
    static boolean initial_call = TRUE;
    int idx;

    if (initial_call) {
        /*
         * Initialize the tcp conn table
         */
        for (idx = 0; idx < MAX_CONNECTIONS; ++idx) {
            sip_tcp_conn_tab[idx].fd = -1;
        }
        initial_call = FALSE;
    }
}

/*
 * sip_tcp_purge_entry()
 * Description : Cleans up the entry associated with the connid
 *               from the sip_tcp_conn_tab
 *
 * Input : connid
 *
 * Output : None
 *
 */
void
sip_tcp_purge_entry (sipSPIConnId_t connid)
{
    sip_tcp_conn_t *entry = sip_tcp_conn_tab + connid;
    const char *fname= "sip_tcp_purge_entry";
    boolean secure;

    if (!VALID_CONNID(connid)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Invalid TCP connection Id=%ld.\n",
                            fname, connid);
        return;
    }
    secure = ccsipIsSecureType(connid);
    
    (void) sip_tcp_detach_socket(entry->fd);
    (void) sipSocketClose(entry->fd, secure);
    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Socket fd: %d closed for connid %ld with "
                        "address: %i, remote port: %u\n",
                        DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), entry->fd, connid, entry->ipaddr, entry->port);
    
    entry->fd = -1;  /* Free the connection table entry in the BEGINNING ! */
    sipTcpFlushRetrySendQueue(entry);
    entry->ipaddr = ip_addr_invalid;
    entry->port = 0;
    entry->context = NULL;
    entry->dirtyFlag = FALSE;
    if (entry->prev_bytes) {
        cpr_free(entry->prev_msg);
    }
    return;
}


/*
 * sip_tcp_create_connection()
 * Description : This routine is called is response to a create connection
 * request from SIP_SPI to SIP_TCP.
 *
 * Input : spi_msg - Pointer to sipSPIMessage_t containing the create conn
 * parameters.
 *
 * Output : Nothing
 */
cpr_socket_t
sip_tcp_create_connection (sipSPIMessage_t *spi_msg)
{
    const char* fname = "sip_tcp_create_connection";
    int idx;
    cpr_socket_t new_fd;
    cpr_sockaddr_storage *local_addr_ptr;
    sipSPICreateConnection_t *create_msg;
    cpr_sockaddr_t local_addr;
    cpr_socklen_t local_addr_len = sizeof(cpr_sockaddr_t);
    int tos_dscp_val = 0; // set to default if there is no config. for dscp
#ifdef IPV6_STACK_ENABLED

    int ip_mode = CPR_IP_MODE_IPV4;
#endif
    uint16_t af_listen = AF_INET6;
    cpr_sockaddr_storage sock_addr;
    uint16_t       addr_len;
    cpr_sockaddr_storage local_sock_addr;
    cpr_ip_addr_t  local_ipaddr;

    sip_tcp_init_conn_table();
    create_msg = &(spi_msg->createConnMsg);
    CPR_IP_ADDR_INIT(local_ipaddr);

#ifdef IPV6_STACK_ENABLED

    config_get_value(CFGID_IP_ADDR_MODE, &ip_mode, sizeof(ip_mode));

    /*
     * Create a socket
     */
    if (ip_mode == CPR_IP_MODE_IPV6 ||
        ip_mode == CPR_IP_MODE_DUAL) {
        af_listen = AF_INET6;
    } else {
#endif
        af_listen = AF_INET;
#ifdef IPV6_STACK_ENABLED
    }
#endif

    /* Create New connection to the (addr,port) pair */
    new_fd = cprSocket(af_listen, SOCK_STREAM, 0 /* IPPROTO_TCP */);
    if (new_fd < 0) {
        /* Send create connection failed message to SIP_SPI */
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Socket creation failed %d.\n",
                            fname, cpr_errno);
        return INVALID_SOCKET;
    }
    idx = sip_tcp_get_free_conn_entry();
    if (idx == -1) {
        /* Send create connection failed message to SIP_SPI */
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No Free connection entry.\n",
                            fname);
        (void) sipSocketClose(new_fd, FALSE);
        return INVALID_SOCKET;
    }
	
    if (sip_tcp_set_sock_options(new_fd) != TRUE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Socket set option failed.\n",
                            fname);
    }

    sip_config_get_net_device_ipaddr(&local_ipaddr);
     
    memset(&local_sock_addr, 0, sizeof(local_sock_addr));

    (void) sip_set_sockaddr(&local_sock_addr, af_listen, local_ipaddr, 0, &addr_len);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"local_ipaddr.u.ip4=%x\n",
            DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), local_ipaddr.u.ip4);

    if (cprBind(new_fd, (cpr_sockaddr_t *)&local_sock_addr, addr_len)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"TCP bind failed with error %d\n", fname,
                              cpr_errno);
        (void) sipSocketClose(new_fd, FALSE);
        sip_tcp_conn_tab[idx].fd = INVALID_SOCKET;
        return INVALID_SOCKET;
    } 

    memset(&sock_addr, 0, sizeof(sock_addr));

    (void) sip_set_sockaddr(&sock_addr, af_listen, create_msg->addr, 
                            (uint16_t)(create_msg->port), &addr_len);

    sip_tcp_conn_tab[idx].fd = new_fd;
    sip_tcp_conn_tab[idx].ipaddr = create_msg->addr;
    sip_tcp_conn_tab[idx].port = create_msg->port;
    sip_tcp_conn_tab[idx].context = spi_msg->context;
    sip_tcp_conn_tab[idx].dirtyFlag = FALSE;
    sip_tcp_conn_tab[idx].addr = sock_addr;
    
    if (cprConnect(new_fd, (cpr_sockaddr_t *)&sock_addr, addr_len)
            == CPR_FAILURE) {
        if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
            char ipaddr_str[MAX_IPADDR_STR_LEN];

            ipaddr2dotted(ipaddr_str, &create_msg->addr);
            
            /* connect in progress. Include this socket in select */
            sip_tcp_conn_tab[idx].state = SOCK_CONNECT_PENDING;
            
            CCSIP_DEBUG_MESSAGE(SIP_F_PREFIX"socket connection in progress errno:%d"
                                "ipaddr: %s, port: %d\n",
                                fname, errno, ipaddr_str, create_msg->port);
        } else {
            char ipaddr_str[MAX_IPADDR_STR_LEN];

            ipaddr2dotted(ipaddr_str, &create_msg->addr);
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket connect failed errno: %d "
                              "ipaddr: %s, port: %d\n",
                              fname, errno, ipaddr_str, create_msg->port);
            sip_tcp_purge_entry(idx);
            return INVALID_SOCKET;
        }
    } else {
        /* Even for this non-blocking socket, the connection was
         * completed immediately. Is that a possibility ??
         * I am not sure. Just send a connectioncreated msg to SIP_SPI
         */
        sip_tcp_conn_tab[idx].state = SOCK_CONNECTED;
    }

    if (cprGetSockName(new_fd, &local_addr, &local_addr_len) != CPR_FAILURE) {
        local_addr_ptr = (cpr_sockaddr_storage *)&local_addr;

        if (local_addr_ptr->ss_family == AF_INET6) {

            create_msg->local_listener_port = ntohs(((cpr_sockaddr_in6_t *)local_addr_ptr)->sin6_port);

        } else {

            create_msg->local_listener_port = ntohs(((cpr_sockaddr_in_t *)local_addr_ptr)->sin_port);
        }

        (void) sip_tcp_attach_socket(new_fd);
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error getting local port info.\n",
                            fname);
        sip_tcp_purge_entry(idx);
        return INVALID_SOCKET;
    }

    // set IP tos/dscp value for SIP messaging
    config_get_value(CFGID_DSCP_FOR_CALL_CONTROL, (int *)&tos_dscp_val,
                     sizeof(tos_dscp_val));
    if (cprSetSockOpt(new_fd, SOL_IP, IP_TOS, (void *)&tos_dscp_val,
                      sizeof(tos_dscp_val)) == CPR_FAILURE) {
        // do NOT take hard action; just log the error and move on
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set IP TOS %d on TCP socket. cpr_errno = %d",
                          fname, tos_dscp_val, cpr_errno);
    }

    return (new_fd);
}

/*
 * sip_tcp_newmsg_to_spi()
 * Description : This routine is called to parse a tcp packet
 * and send it to the spi for further processing.
 *
 * Input : buf: pointer to the message
 *         nbytes: length of the message
 *         connID: connid over which the message was received
 *
 * Output : success / failure in processing
 */
static int
sip_tcp_newmsg_to_spi (char *buf, unsigned long nbytes, int connID)
{
    static const char *fname = "sip_tcp_newmsg_to_spi";
    sipMessage_t     *sip_msg;
    ccsipRet_e        val;
    cpr_sockaddr_storage   from;
    char             *disply_msg_buff = NULL;
    char            **display_msg_buff_p;
    boolean           error;
    cpr_ip_addr_t   ip_addr;

    CPR_IP_ADDR_INIT(ip_addr);

    /* Set up display msg. if debug msg. is enabled */
    if (SipDebugMessage) {
        display_msg_buff_p = &disply_msg_buff;
    } else {
        /* No display msg. is needed */
        display_msg_buff_p = NULL;
    }

    do {
        disply_msg_buff = NULL;
        error = FALSE;
        val = ccsip_process_network_message(&sip_msg, &buf, &nbytes,
                                            display_msg_buff_p);

        switch (val) {
        case SIP_SUCCESS:
            /*
             * Print the received TCP packet info
             */
            if (disply_msg_buff != NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"RCV: TCP message=\n", fname);
                platform_print_sip_msg(disply_msg_buff);
            }
            from = sip_tcp_conn_tab[connID].addr;

            util_extract_ip(&ip_addr, &from);
            ccsip_dump_recv_msg_info(sip_msg, &ip_addr, 0);
            /* Process SIP message */
            SIPTaskProcessTCPMessage(sip_msg, from);
            break;

        case SIP_MSG_INCOMPLETE_ERR:

            sip_tcp_conn_tab[connID].prev_msg = cpr_strdup(buf);
            if (sip_tcp_conn_tab[connID].prev_msg) {
                sip_tcp_conn_tab[connID].prev_bytes = nbytes;
            }
            sip_tcp_incomplete_msg++;
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIP Incomplete message.%d\n",fname,
                sip_tcp_incomplete_msg);
            error = TRUE;
            break;

        case SIP_MSG_PARSE_ERR:
            /*
             * Print the received TCP packet info
             */
            if (disply_msg_buff != NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"RCV: TCP message=\n", fname);
                platform_print_sip_msg(disply_msg_buff);
            }
            sip_tcp_fail_network_msg++;
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIP Message Parse error %d.\n", fname,
                sip_tcp_fail_network_msg);
            error = TRUE;
            break;

        case SIP_MSG_CREATE_ERR:
        default:
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"SIP Message create error.\n", fname);
            error = TRUE;
            break;
        }

        /* Free display msg. buffer if it is allocated */
        if (disply_msg_buff != NULL) {
            cpr_free(disply_msg_buff);
            disply_msg_buff = NULL;
        }

        if (error) {
            /* There was an error encountered, exit */
            return -1;
        }
    } while (nbytes > 0);
    return 0;
}

void
sip_tcp_createconnfailed_to_spi (cpr_ip_addr_t *ipaddr,
                                 uint16_t port,
                                 void *context,
                                 ccsipSockErrCodes_e errcode,
                                 int connid)
{
    static const char *fname = "sip_tcp_createconnfailed_to_spi";
    ccsipCCB_t *ccb = NULL;
    ti_config_table_t *ccm_active_table_entry = NULL,
                      *ccm_standby_table_entry = NULL,
                      *ccm_table_entry = NULL;
    ti_common_t *active_ti_common = NULL;
    ti_common_t *standby_ti_common = NULL;
    uint32_t retx_value;
    char            ip_addr_str[MAX_IPADDR_STR_LEN];


    /*
     * Use LINE1 for now as there is only one type of cc
     * supported, no mixed mode. Will have to change when
     * we add mixed mode cc support on the phone.
     */
    if (CC_Config_Table[LINE1].cc_type == CC_CCM) {

        /*
         * Check and see which tcp link went down, active /
         * standby and get the appropriate reg ccb.
         * Using ipaddr:port combination to find if the active
         * or the standby went down. To use fd we will have to
         * write a routine to return connid of the tcp conn table
         * for an ipaddr:port combination.
         */
        ccm_active_table_entry = CCM_Active_Standby_Table.active_ccm_entry;
        ccm_standby_table_entry = CCM_Active_Standby_Table.standby_ccm_entry;
        if (ccm_active_table_entry) {
            active_ti_common = &ccm_active_table_entry->ti_common;
        }
        if (ccm_standby_table_entry) {
            standby_ti_common = &ccm_standby_table_entry->ti_common;
        }

        ipaddr2dotted(ip_addr_str, ipaddr);
        if (active_ti_common && util_compare_ip(&(active_ti_common->addr), ipaddr) &&
            active_ti_common->port == port) {
            /*
             * Active link has gone down
             */
        	int last_cpr_err = cpr_errno;
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Active server going down due to "
                "%s. ip_addr:%s\n", DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname),
                last_cpr_err == CPR_ETIMEDOUT ? "ETIMEDOUT": 
                last_cpr_err == CPR_ECONNABORTED ? "ECONNABORTED": 
                last_cpr_err == CPR_ECONNRESET ? "CM_RESET_TCP": "CM_CLOSED_TCP",
                 ip_addr_str);

            ccb = sip_sm_get_ccb_by_index(REG_CCB_START);
            ccm_table_entry = ccm_active_table_entry;
            
        } else if (standby_ti_common && 
                    util_compare_ip(&(standby_ti_common->addr), ipaddr) &&
                   standby_ti_common->port == port) {
            /*
             * Standby link has gone down
             */
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Standby server going down "
                "ip_addr=%s\n",
                DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), 
                 ip_addr_str);

            ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
            ccm_table_entry = ccm_standby_table_entry;
        } else {
            /*
             * Neither of the links, just return.
             * This could happen with fall back ccm
             */
            ccsipCCB_t *ccb_of_fallback = NULL;

            // Find fallback ccb and set the socket handle INVALID
            if (sip_regmgr_find_fallback_ccb_by_addr_port(ipaddr, port,
                                                          &ccb_of_fallback)) {
                if (ccb_of_fallback && (ccb_of_fallback->cc_cfg_table_entry)) {
                    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Fallback server going "
                        " down ip_addr=%s\n",
                        DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname),
                         ip_addr_str);
                    sip_tcp_purge_entry(connid);
                    sipTransportSetServerHandleAndPort(INVALID_SOCKET, 0,
                        (ti_config_table_t *)ccb_of_fallback->cc_cfg_table_entry);
                }
            } else {
                sipTransportClearServerHandle(ipaddr, port, connid);
            }
            return;
        }
        /*
         * In this case we need to make sure any pending
         * calls are cleared as well... Need to make sure
         * we clear that, because we are setting the local port
         * to zero. So any one-time udp message that needs to get
         * sent will have the port as 0 in the contact header.
         */
        if (ccm_table_entry) {
            sip_tcp_purge_entry(connid);
            sipTransportSetServerHandleAndPort(INVALID_SOCKET, 0,
                        (ti_config_table_t *) (ccm_table_entry));
        } else {
            sipTransportClearServerHandle(ipaddr, port, connid);
        }
        /*
         * Set the retx_counter to the configured retx_value
         * so no more retries happen
         */
        if (ccb != NULL) {
        		config_get_value(CFGID_SIP_RETX, &retx_value, sizeof(retx_value));
	            ccb->retx_counter = retx_value + 1;
	            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"send a SIP_TMR_REG_RETRY"
	                "message so this cucm ip:%s can be put in fallback list \n", 
	                DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), ip_addr_str);
	            if (ccsip_register_send_msg(SIP_TMR_REG_RETRY, ccb->index)
	                    != SIP_REG_OK) {
	                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"REG send message failed.\n", fname);
	                ccsip_register_cleanup(ccb, TRUE);
	            }
        	         
        }
    }
}

/*
 * sip_tcp_read_socket()
 * Description : After we come out of select call, for every valid socket in
 * the connection table it
 *         1. Checks it for readability
 *         2. If it is readable, either accept the incoming connect (for master
 *            port) or receive data from network
 *         3. Processes the data received from the network.
 *
 * Input : Value of read mask after call to socket_select()
 *
 * Output : Nothing
 */
void
sip_tcp_read_socket (cpr_socket_t this_fd)
{
    int nbytes;
    char *sip_tcp_buf;
    char temp;
    int connid;
    const char *fname="sip_tcp_read_socket";
    boolean secure;

    connid = sip_tcp_fd_to_connid(this_fd);
    if (connid == -1) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Read failed for unknown socket %d.\n", 
                        fname, this_fd);
        return;
    }
    secure = ccsipIsSecureType(connid);

    if (sip_tcp_conn_tab[connid].state == SOCK_CONNECT_PENDING) {
        int bytes_read = 0;

        /* This socket is now readable, connection complete.
         * Inform SIP SPI
         * Do a dummy read, if it is successful, change state
         */
        bytes_read = sipSocketRecv(this_fd, &temp, 0, 0, secure);
        if ((bytes_read != -1) || (errno == EWOULDBLOCK)) {
            sip_tcp_conn_tab[connid].state = SOCK_CONNECTED;
        } else if (errno == ENOTCONN) {
            sip_tcp_conn_tab[connid].dirtyFlag = TRUE;
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket error=%d=\n", fname, errno);
            sip_tcp_createconnfailed_to_spi(&(sip_tcp_conn_tab[connid].ipaddr),
                                            sip_tcp_conn_tab[connid].port,
                                            sip_tcp_conn_tab[connid].context,
                                            SOCKET_CONNECT_ERROR, connid);
            return;
        }
    } else {
        unsigned long offset = sip_tcp_conn_tab[connid].prev_bytes;

        if (offset) {
            sip_tcp_buf = (char *) cpr_realloc(sip_tcp_conn_tab[connid].prev_msg,
                                               (offset + CPR_MAX_MSG_SIZE + 1));
            sip_tcp_conn_tab[connid].prev_bytes = 0;

            if (sip_tcp_buf == NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to realloc tcp_msg buffer memory.\n", 
                                    fname);
                cpr_free(sip_tcp_conn_tab[connid].prev_msg);
                sip_tcp_conn_tab[connid].prev_msg = NULL;
                return;
            }

            nbytes = sipSocketRecv(this_fd, &sip_tcp_buf[offset],
                             CPR_MAX_MSG_SIZE, 0, secure);

            /* Ensure that we dont have too much buffered which may
             * be due to some error conditions.
             */
            if ((nbytes + offset) > (MAX_PAYLOAD_SIZE + 1)) {

                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Total SIP message size of %d "
                                  "bytes exceeds maximum of %d bytes",
                                  fname, (nbytes + offset),
                                  (MAX_PAYLOAD_SIZE + 1));

                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Dropping SIP message %s", 
                                    fname, sip_tcp_buf);
                cpr_free(sip_tcp_buf);
                return;
            }
        } else {
            sip_tcp_buf = (char *) cpr_malloc(CPR_MAX_MSG_SIZE + 1);
            if (sip_tcp_buf == NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to malloc tcp_msg buffer memory.\n", 
                                    fname);
                return;
            }
            nbytes = sipSocketRecv(this_fd, sip_tcp_buf, CPR_MAX_MSG_SIZE, 0, secure);
        }

        if (nbytes > 0) {
            nbytes += offset;
            sip_tcp_buf[nbytes] = 0;
            (void) sip_tcp_newmsg_to_spi(sip_tcp_buf, nbytes, connid);
        } else if ((nbytes == 0) ||
                   ((nbytes == -1) && (errno != EWOULDBLOCK))) {
            /*
             * Remote connection closure or broken pipe - post a message
             * to sip transport and wait for connection close command.
             */
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"CUCM closed TCP connection.\n", 
                DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname));
            sip_tcp_conn_tab[connid].error_cause = SOCKET_REMOTE_CLOSURE;

            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket error=%d=\n", fname, errno);

            sip_tcp_createconnfailed_to_spi(&(sip_tcp_conn_tab[connid].ipaddr),
                                        sip_tcp_conn_tab[connid].port,
                                        sip_tcp_conn_tab[connid].context,
                                        SOCKET_CONNECT_ERROR, connid);
            // Clear proxy handle
            if (CC_Config_Table[LINE1].cc_type != CC_CCM) {
                sipTransportCSPSClearProxyHandle(&(sip_tcp_conn_tab[connid].ipaddr),
                                                 sip_tcp_conn_tab[connid].port,
                                                 this_fd);
                sip_tcp_purge_entry(connid);
            }
        }
        cpr_free(sip_tcp_buf);
    }
}

/*
 ** sip_tcp_find_msg
 *
 *  FILENAME: ip_phone\sip\ccsip_platform_tcp.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
sll_match_e
sip_tcp_find_msg (void *find_by_p, void *data_p)
{
    return (SLL_MATCH_FOUND);
}

/* Socket is not writable. Queue the data to be sent.
 *
 * Inputs:
 *   total_len  - total length of message
 *   connid     - connection id
 *   buf        - buffer of data to be written (this is the entire
 *                contents not just the remaining data. We do this
 *                so we can display the entire message when the last
 *                chunk is written.
 *  send_msg_display - boolean indicating whether the sent message
 *                      is to be displayed for debugging etc.
 */
static void
sipTcpQueueSendData (int total_len, int connid,
                     char *buf, void *context, boolean send_msg_display,
                     uint8_t ip_sig_tos)
{
    static const char *fname = "sipTcpQueueSendData";
    ccsipTCPSendData_t *sendData;
    sip_tcp_conn_t *entry;
    int send_msg_q_size = 0;

    entry = sip_tcp_conn_tab + connid;
    if (entry->sendQueue == NULL) {
        entry->sendQueue = sll_create(sip_tcp_find_msg);
        if (entry->sendQueue == NULL) {
            CCSIP_DEBUG_ERROR("%s Failed to create sendQueue to buffer data!\n", fname); 
            return;
        }
    }

    sendData = (ccsipTCPSendData_t *) cpr_malloc(sizeof(ccsipTCPSendData_t));
    if (sendData == NULL) {
        CCSIP_DEBUG_ERROR("%s Failed to allocate memory for sendData!\n", fname); 
        return;
    }
    memset(sendData, 0, sizeof(ccsipTCPSendData_t));

    sendData->data = (char *) cpr_malloc(total_len + 1);

    if (sendData->data) {
        sstrncpy(sendData->data, buf, total_len);
    } else {
        CCSIP_DEBUG_ERROR("%s Failed to allocate memory for sendData->data!\n", fname); 
        cpr_free(sendData);
        return;
    }

    sendData->bytesSent = 0;
    sendData->bytesLeft = (uint16_t) total_len;
    sendData->context = context;
    sendData->msg_display = send_msg_display;
    sendData->ip_sig_tos = ip_sig_tos;
    (void) sll_append(entry->sendQueue, sendData);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Data queued length %d\n", 
                          DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), total_len);

    if (send_msg_q_size > max_tcp_send_msg_q_size) {
        max_tcp_send_msg_q_size   = send_msg_q_size;
        max_tcp_send_msg_q_connid = connid;
        max_tcp_send_msg_q_ipaddr = entry->ipaddr;
        max_tcp_send_msg_q_port   = entry->port;
    }
}

/*
 * Free memory for any queued write data.
 */
void
sipTcpFlushRetrySendQueue (sip_tcp_conn_t *entry)
{
    ccsipTCPSendData_t *sendData = NULL;

    if (entry->sendQueue) {
        sendData = (ccsipTCPSendData_t *) sll_next(entry->sendQueue, sendData);
        while (sendData) {
            cpr_free(sendData->data);
            (void) sll_remove(entry->sendQueue, sendData);
            cpr_free(sendData);
            sendData = (ccsipTCPSendData_t *) sll_next(entry->sendQueue, NULL);
        }
        (void) sll_destroy(entry->sendQueue);
        entry->sendQueue = NULL;
    }
}

void
sip_tcp_resend (int connid)
{
    static const char *fname = "sip_tcp_resend";
    ccsipTCPSendData_t *qElem = NULL;
    sip_tcp_conn_t *entry;
    int bytes_sent;
    boolean secure;

    if (!VALID_CONNID(connid)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Resend failed for unknown socket %d.\n", 
                        fname, connid);
        return;
    }

    secure = ccsipIsSecureType(connid);

    entry = sip_tcp_conn_tab + connid;
    if (entry->sendQueue) {
        qElem = (ccsipTCPSendData_t *) sll_next(entry->sendQueue, NULL);
//        ccsipSocketSetIPtos(entry->fd, qElem->ip_sig_tos);
        while (qElem) {
            while (qElem->bytesLeft) {
                bytes_sent = sipSocketSend(entry->fd, &qElem->data[qElem->bytesSent],
                                     qElem->bytesLeft, 0, secure);
                if (bytes_sent > 0) {
                    qElem->bytesSent += bytes_sent;
                    qElem->bytesLeft -= bytes_sent;
                } else {
                    if (errno == EWOULDBLOCK) {
                        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Socket blocked requeue data\n",
                                              DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname));
                    } else {
                        entry->error_cause = SOCKET_SEND_ERROR;
                        sipTcpFlushRetrySendQueue(entry);
                        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket error=%d=\n", fname, errno);
                        sip_tcp_createconnfailed_to_spi(
                            &(sip_tcp_conn_tab[connid].ipaddr),
                            sip_tcp_conn_tab[connid].port,
                            sip_tcp_conn_tab[connid].context,
                            SOCKET_CONNECT_ERROR, connid);
                        CCSIP_DEBUG_ERROR("%s: Socket send error." 
                                          "Purge queued entry data.\n", fname);
                    }
                    return;
                }
            } /* while (qElem->bytesLeft) */

            cpr_free(qElem->data);
            (void) sll_remove(entry->sendQueue, qElem);
            cpr_free(qElem);
            CCSIP_DEBUG_REG_STATE("%s: sent out successfully, dequeue an entry.\n", fname); 

            qElem = (ccsipTCPSendData_t *) sll_next(entry->sendQueue, NULL);
        } /* while qElem */
    }
}


/*
 * sip_tcp_channel_send()
 * Description : This routine is called to send out a tcp message
 *
 *
 * Input : s: socket to send the message over
 *         buf: message to send
 *         len: length of the message
 *
 * Output : success / failure in processing
 */
int
sip_tcp_channel_send (cpr_socket_t s, char *buf, uint32_t len)
{
    static const char *fname = "sip_tcp_channel_send";
    int bytesSent = 0, totalBytesSent = 0;
    int connid = 0;
    sip_tcp_conn_t *entry;
    boolean secure;

    /* convert socket to connid */ 
    connid = sip_tcp_fd_to_connid(s);
    if (!VALID_CONNID(connid)) {
        CCSIP_DEBUG_ERROR("%s: Couldn't map socket to a valid connid!\n", fname);
        return SIP_TCP_SEND_ERROR; 
    }
    /* use connid we can get this socket's entry in sip_tcp_conn_tab[] */ 
    entry = sip_tcp_conn_tab + connid;

    /* secd requires that the socket should be in connected state
     * to send message. Return if the status is pending. 
     * Currently this gets control in fallback state. The retry timer
     * event in fallback state will resend the message.
     */
    if ((sip_tcp_conn_tab[connid].soc_type == SIP_SOC_TLS) &&
        (sip_tcp_conn_tab[connid].state == SOCK_CONNECT_PENDING)) {
        plat_soc_connect_status_e conn_status;
        conn_status = platSecSockIsConnected(s);
        if (conn_status == PLAT_SOCK_CONN_OK) {
            sip_tcp_conn_tab[connid].state = SOCK_CONNECTED;

        } else if (conn_status == PLAT_SOCK_CONN_WAITING) {

            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"tls socket waiting %d\n", DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), s);
            return SIP_TCP_SEND_OK;

        } else if (conn_status == PLAT_SOCK_CONN_FAILED) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket error=%d=\n", fname, errno);
            sip_tcp_createconnfailed_to_spi(&(sip_tcp_conn_tab[connid].ipaddr),
                                            sip_tcp_conn_tab[connid].port,
                                            sip_tcp_conn_tab[connid].context,
                                            SOCKET_CONNECT_ERROR, connid);   
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"TLS socket connect failed %d\n",
                              fname, s);
            return SIP_TCP_SEND_ERROR;
        }
    }

    /*
     * Check not exceeding max allowed payload size
     */
    if (len >= MAX_PAYLOAD_SIZE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_TCP_PAYLOAD_TOO_LARGE),
                          fname, len, CPR_MAX_MSG_SIZE);
        return SIP_TCP_SIZE_ERROR;
    }

    /*
     * Check to see if the send q is empty. If it is not, then
     * queue the current message in the sendqueue so that it
     * gets sent in order when the socket is ready.
     */
    if (entry->sendQueue && sll_count(entry->sendQueue)) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"%d Socket waiting on EWOULDBLOCK, "
                              " queueing data\n", DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), connid);
        sipTcpQueueSendData(len, connid, buf, NULL, TRUE, 0x0);
        return SIP_TCP_SEND_OK;
    }

    secure = ccsipIsSecureType(connid);

    while (len > 0) {
        bytesSent = sipSocketSend(s, (void *)buf, (size_t) len, 0, secure);
        if (bytesSent == SOCKET_ERROR) {
            if (cpr_errno == CPR_EWOULDBLOCK) {
                    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"%d Socket EWOULDBLOCK while "
                                          "sending, queueing data\n", DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname),
                                          connid);
                    sipTcpQueueSendData(len, connid, buf, NULL,
                                        TRUE, 0x0);
                    break;
            }
            if (cpr_errno != CPR_ENOTCONN) { 
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"socket error=%d=\n", fname, errno);
                sip_tcp_createconnfailed_to_spi(&(sip_tcp_conn_tab[connid].ipaddr),
                                                sip_tcp_conn_tab[connid].port,
                                                sip_tcp_conn_tab[connid].context,
                                                SOCKET_CONNECT_ERROR, connid);
            }
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "sipSocketSend", cpr_errno);
            return (cpr_errno== CPR_ENOTCONN? cpr_errno: SIP_TCP_SEND_ERROR);
        }
        len -= bytesSent;
        totalBytesSent += bytesSent;
        buf += bytesSent;
    }
    return SIP_TCP_SEND_OK;
}

/*
 * Free memory for any queued write data.
 */
void
sipTcpFreeSendQueue (int connid)
{
    static const char *fname = "sipTcpFreeSendQueue";
    sip_tcp_conn_t *entry;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Free TCP send queue for connid %d \n", 
                        DEB_F_PREFIX_ARGS(SIP_TCP_MSG, fname), connid);
    if (!VALID_CONNID(connid)) {
        return;
    }
    entry = sip_tcp_conn_tab + connid;
    sipTcpFlushRetrySendQueue(entry);
}

/*
 * sip_tcp_create_conn_using_blocking_socket()
 * Description : This routine is called to create blocking socket
 * request from SIP_SPI to SIP_TCP.
 *
 * Input : spi_msg - Pointer to sipSPIMessage_t containing the create conn
 * parameters.
 *
 * Output : socket fd
 */
cpr_socket_t
sip_tcp_create_conn_using_blocking_socket (sipSPIMessage_t *spi_msg) {
	
	cpr_socket_t server_conn_handle = INVALID_SOCKET;

	server_conn_handle = sip_tcp_create_connection(spi_msg);
	
	return server_conn_handle;
}
