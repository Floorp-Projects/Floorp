/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_socket.h"
#include "cpr_memory.h"
#include "cpr_ipc.h"
#include "cpr_in.h"
#include "cpr_errno.h"
#include "cpr_string.h"
#include "util_string.h"
#include "text_strings.h"
#include "ccsip_core.h"
#include "ccsip_task.h"
#include "ccsip_platform_udp.h"
#include "phone.h"
#include "phone_debug.h"
#include "sip_common_transport.h"
#include "sip_platform_task.h"
#include "sip_socket_api.h"

// FIXME include does not exist on windows
//#include <net/if.h>

static uint16_t af_family_listen = AF_INET6;
static uint16_t af_family_connect = AF_INET6;

/*
 *  To set the appropriate socket strcutre based on the family. The called
 *  of the function is responsible for allocating the memory.
 *
 *  @param psock_storage  pointer to cpar_sockaddr_storage
 *         family         network family AF_INET or AF_INET6
 *         ip_addr        ip address
 *         port
 *         addr_len       legth returned based on family
 *
 *
 *  @return  pointer to cpr_sockaddr, which is also psock_storage
 *
 *  @pre     none
 *
 */

cpr_sockaddr_t *sip_set_sockaddr (cpr_sockaddr_storage *psock_storage, uint16_t family,
                                 cpr_ip_addr_t ip_addr, uint16_t port, uint16_t *addr_len)
{
    static const char fname[] = "sip_set_sockaddr";

    cpr_sockaddr_in6_t *pin6_addr;
    cpr_sockaddr_in_t *pin_addr;
    cpr_sockaddr_t *psock_addr;
    uint32_t   tmp_ip;
    int i,j;
    unsigned char tmp;

    switch (family) {
    case AF_INET6:

        pin6_addr = (cpr_sockaddr_in6_t *)psock_storage;
        memset(pin6_addr, 0, sizeof(cpr_sockaddr_in6_t));
        pin6_addr->sin6_family      = family;
        pin6_addr->sin6_port        = htons(port);

        if (ip_addr.type == CPR_IP_ADDR_IPV4) {

            if (ip_addr.u.ip4 != INADDR_ANY) {
                pin6_addr->sin6_addr.addr.base16[5] = 0xffff;
            }
            tmp_ip = ntohl(ip_addr.u.ip4);
            memcpy((void *)&(pin6_addr->sin6_addr.addr.base16[6]), (void *)&tmp_ip, 4);

        } else {

            for (i=0, j=15; i<16; i++, j--) {
                tmp  = pin6_addr->sin6_addr.addr.base8[j];
                pin6_addr->sin6_addr.addr.base8[j] = ip_addr.u.ip6.addr.base8[i];
                ip_addr.u.ip6.addr.base8[i] = tmp;
            }

        }

        *addr_len = sizeof(cpr_sockaddr_in6_t);
        return(psock_addr=(cpr_sockaddr_t *)pin6_addr);

    case AF_INET:
        if (ip_addr.type == CPR_IP_ADDR_IPV6) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Setting ipv6 address in AF_INET",fname);
            break;
        }
        pin_addr = (cpr_sockaddr_in_t *)psock_storage;
        memset(pin_addr, 0, sizeof(cpr_sockaddr_in_t));
        pin_addr->sin_family      = family;
        pin_addr->sin_addr.s_addr = htonl(ip_addr.u.ip4);
        pin_addr->sin_port        = htons(port);

        *addr_len = sizeof(cpr_sockaddr_in_t);
        return(psock_addr=(cpr_sockaddr_t *)pin_addr);

    default:
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set sockaddr.",fname);
        break;
    }

    return(NULL);
}

int
sip_platform_udp_channel_listen (cpr_ip_mode_e ip_mode, cpr_socket_t *s,
                                 cpr_ip_addr_t *local_ipaddr,
                                 uint16_t local_port)
{
    static const char fname[] = "sip_platform_udp_channel_listen";
    cpr_sockaddr_storage sock_addr;
    uint16_t       addr_len;

    /*
     * If socket passed is is not INVALID_SOCKET close it first
     */

    if (*s != INVALID_SOCKET) {
        if (sipSocketClose(*s, FALSE) != CPR_SUCCESS) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "sipSocketClose", cpr_errno);
        }
        sip_platform_task_reset_listen_socket(*s);
    }

    /*
     * Create a socket
     */
    if (ip_mode == CPR_IP_MODE_IPV6 ||
        ip_mode == CPR_IP_MODE_DUAL) {
        af_family_listen = AF_INET6;
    } else {
        af_family_listen = AF_INET;
    }

    *s = cprSocket(af_family_listen, SOCK_DGRAM, 0);
    if (*s == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                          fname, "cprSocket unable to open socket", cpr_errno);
        if (ip_mode == CPR_IP_MODE_DUAL) {

            af_family_listen = AF_INET;
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"Socket open failed for IPv6 using IPv4 address.",
                             DEB_F_PREFIX_ARGS(SIP_SDP, fname));

            *s = cprSocket(af_family_listen, SOCK_DGRAM, 0);
            if (*s == INVALID_SOCKET) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprSocket unable to open socket for IPv4",
                                        cpr_errno);
                return SIP_ERROR;
            }
        }
    }

    (void) sip_set_sockaddr(&sock_addr, af_family_listen, *local_ipaddr,
                            local_port, &addr_len);

    if (cprBind(*s,  (cpr_sockaddr_t *)&sock_addr, addr_len) == CPR_FAILURE) {
        (void) sipSocketClose(*s, FALSE);
        *s = INVALID_SOCKET;
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                          fname, "cprBind", cpr_errno);
        return SIP_ERROR;
    }
    sip_platform_task_set_listen_socket(*s);

    return SIP_OK;
}

int
sip_platform_udp_channel_create (cpr_ip_mode_e ip_mode, cpr_socket_t *s,
                                 cpr_ip_addr_t *remote_ipaddr,
                                 uint16_t remote_port,
                                 uint32_t local_udp_port)
{
    static const char *fname = "sip_platform_udp_channel_create";
    cpr_sockaddr_storage sock_addr;
    uint16_t       addr_len;
    cpr_sockaddr_storage local_sock_addr;
    cpr_ip_addr_t local_signaladdr;

    int tos_dscp_val = 0; // set to default if there is no config. for dscp

    CPR_IP_ADDR_INIT(local_signaladdr);

    if (*s != INVALID_SOCKET) {
        (void) sipSocketClose(*s, FALSE);
    }

    if (ip_mode == CPR_IP_MODE_IPV6 ||
        ip_mode == CPR_IP_MODE_DUAL) {
        af_family_connect = AF_INET6;
    } else {
        af_family_connect = AF_INET;
    }
    /*
     * Create socket
     */
    *s = cprSocket(af_family_connect, SOCK_DGRAM, 0);
    if (*s == INVALID_SOCKET) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                          fname, "cprSocket unable to open socket",
                          cpr_errno);
        /* Try opening ipv4 socket */
        if (ip_mode == CPR_IP_MODE_DUAL) {

            CCSIP_DEBUG_TASK("%s: cprSocket Open failed for IPv6 trying IPv4",
                            fname);
            af_family_connect = AF_INET;
            *s = cprSocket(af_family_connect, SOCK_DGRAM, 0);
            if (*s == INVALID_SOCKET) {

                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprSocket unable to open AF_INET socket",
                             cpr_errno);
                return SIP_ERROR;
            }
        }
    }

    sip_config_get_net_device_ipaddr(&local_signaladdr);
    memset(&local_sock_addr, 0, sizeof(local_sock_addr));

    (void) sip_set_sockaddr(&local_sock_addr, af_family_connect, local_signaladdr, 0, &addr_len);
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"local_signaladdr.u.ip4=%x",
        DEB_F_PREFIX_ARGS(SIP_SDP, fname), local_signaladdr.u.ip4);

    if(cprBind(*s, (cpr_sockaddr_t *)&local_sock_addr, addr_len)){
       CCSIP_DEBUG_ERROR(SIP_F_PREFIX"UDP bind failed with errno %d", fname, cpr_errno);
       (void) sipSocketClose(*s, FALSE);
       *s = INVALID_SOCKET;
       return SIP_ERROR;
    }

    /*
     * Connect to remote address
     */
    (void) sip_set_sockaddr(&sock_addr, af_family_connect, *remote_ipaddr,
                            remote_port, &addr_len);

 /*   if (cprConnect(*s, (cpr_sockaddr_t *)&sock_addr, addr_len) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                          fname, "cprConnect", cpr_errno);
        (void) sipSocketClose(*s, FALSE);
        *s = INVALID_SOCKET;
          return SIP_ERROR;
    }
*/
    // set IP tos/dscp value for SIP messaging
    config_get_value(CFGID_DSCP_FOR_CALL_CONTROL, (int *)&tos_dscp_val,
                     sizeof(tos_dscp_val));

    if (cprSetSockOpt(*s, SOL_IP, IP_TOS, (void *)&tos_dscp_val,
                      sizeof(tos_dscp_val)) == CPR_FAILURE) {
        // do NOT take hard action; just log the error and move on
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to set IP TOS %d on UDP socket. "
                          "cpr_errno = %d\n", fname, tos_dscp_val, cpr_errno);
    }
    return SIP_OK;
}


int
sip_platform_udp_channel_destroy (cpr_socket_t s)
{
    static const char fname[] = "sip_platform_udp_channel_destroy";

    if (s != INVALID_SOCKET) {
        if (sipSocketClose(s, FALSE) == CPR_FAILURE) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "sipSocketClose", cpr_errno);
            return SIP_ERROR;
        }
    }
    return SIP_OK;
}

int
sip_platform_udp_channel_read (cpr_socket_t s,
                               cprBuffer_t buf,
                               uint16_t *len,
                               cpr_sockaddr_t *soc_addr,
                               cpr_socklen_t *soc_addr_len)
{
    static const char *fname = "sip_platform_udp_channel_read";
    int bytes_read;
    // NOT USED: cpr_sockaddr_in_t *addr = (cpr_sockaddr_in_t *)soc_addr;

    bytes_read = cprRecvFrom(s, buf, CPR_MAX_MSG_SIZE, 0, soc_addr,
                             soc_addr_len);

    switch (bytes_read) {
    case SOCKET_ERROR:
        /*
         * If no data is available to read (CPR_EWOULDBLOCK),
         * for non-blocking socket, it is not an error.
         */
        cpr_free(buf);
        *len = 0;
        if (cpr_errno != CPR_EWOULDBLOCK) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"fd[%d]", fname, s);
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprRecvFrom", cpr_errno);
            return SIP_ERROR;
        }
        /*
         * Will continue reading when data arrives at socket
         */
        break;
    case 0:
        /*
         * Return value 0 is OK.  This does NOT mean the connection
         * has closed by the peer, as with TCP sockets.  With UDP
         * sockets, there is no such thing as closing a connection.
         */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"No data on fd %d", DEB_F_PREFIX_ARGS(SIP_SDP, fname), s);
        cpr_free(buf);
        *len = 0;
        break;
    default:
        /* PKT has been read */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Recvd on fd %d", DEB_F_PREFIX_ARGS(SIP_SDP, fname), s);
        *len = (uint16_t) bytes_read;
        break;
    }

    return SIP_OK;
}

int
sip_platform_udp_channel_send (cpr_socket_t s, char *buf, uint16_t len)
{
    static const char *fname = "sip_platform_udp_channel_send";
    ssize_t bytesSent;

    /*
     * Check not exceeding max allowed payload size
     */
    if (len >= PKTBUF_SIZ) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_UDP_PAYLOAD_TOO_LARGE),
                          fname, len, PKTBUF_SIZ);
        return SIP_ERROR;
    }

    while (len > 0) {
        bytesSent = sipSocketSend(s, (void *)buf, (size_t)len, 0, FALSE);
        if (bytesSent == SOCKET_ERROR) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprSend", cpr_errno);
            return SIP_ERROR;
        }

        len -= bytesSent;
        buf += bytesSent;
    }

    return SIP_OK;
}

/**
 *
 * sip_platform_udp_read_socket
 *
 * Read from the socket to extract received message
 *
 * Parameters:   s - the socket
 *
 * Return Value: None
 *
 */
void
sip_platform_udp_read_socket (cpr_socket_t s)
{
    cprBuffer_t buf;
    uint16_t len = 0;
    cpr_sockaddr_storage from;
    cpr_socklen_t from_len;
    const char *fname = "sip_platform_udp_read_socket";

    if (af_family_listen == AF_INET6) {
        from_len = sizeof(cpr_sockaddr_in6_t);
    } else {
        from_len = sizeof(cpr_sockaddr_in_t);
    }

    buf = SIPTaskGetBuffer(CPR_MAX_MSG_SIZE);
    if (buf) {
        if ((sip_platform_udp_channel_read(s, buf, &len,
                (cpr_sockaddr_t *)&from, &from_len) == SIP_OK) &&
            (len != 0)) {
            (void) SIPTaskProcessUDPMessage(buf, len, from);
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No buffers available to read UDP socket.",
                            fname);
    }
}

int
sip_platform_udp_channel_sendto (cpr_socket_t s, char *buf, uint32_t len,
                                 cpr_ip_addr_t *dst_ipaddr, uint16_t dst_port)
{
    static const char *fname = "sip_platform_udp_channel_sendto";
    ssize_t bytesSent;
    cpr_sockaddr_storage sock_addr;
    uint16_t       addr_len;
    cpr_ip_addr_t  dest_ip_addr;

    /*
     * Connect to remote address
     */
    dest_ip_addr = *dst_ipaddr;
    (void) sip_set_sockaddr(&sock_addr, af_family_connect, dest_ip_addr,
                            dst_port, &addr_len);


    /*
     * Check not exceeding max allowed payload size
     */
    if (len >= PKTBUF_SIZ) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_UDP_PAYLOAD_TOO_LARGE),
                          fname, len, PKTBUF_SIZ);
        return SIP_ERROR;
    }

    while (len > 0) {
        bytesSent = cprSendTo(s, (void *)buf, (size_t)len, 0,
                              (cpr_sockaddr_t *)&sock_addr, addr_len);

        if ((bytesSent == SOCKET_ERROR) && (cpr_errno == CPR_ECONNREFUSED)) {
            /*
             * Will get socket error ECONNREFUSED after an ICMP message
             * resend the message
             */
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"UDP send to error %d", DEB_F_PREFIX_ARGS(SIP_SOCK, fname), cpr_errno);
            bytesSent = cprSendTo(s, (void *)buf, (size_t)len, 0,
                                  (cpr_sockaddr_t *)&sock_addr, addr_len);
        }
        if (bytesSent == SOCKET_ERROR) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_SYSTEMCALL_FAILED),
                              fname, "cprSendTo", cpr_errno);
            return SIP_ERROR;
        }

        len -= bytesSent;
        buf += bytesSent;
    }

    return SIP_OK;
}
