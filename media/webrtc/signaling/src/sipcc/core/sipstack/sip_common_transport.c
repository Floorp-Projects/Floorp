/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_in.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_errno.h"
#include "cpr_socket.h"
#include "cpr_locks.h"
#include "phone_debug.h"
#include "dialplan.h"
#include "config.h"
#include "configmgr.h"
#include "ccsip_platform_udp.h"
#include "ccsip_platform_timers.h"
#include "ccsip_register.h"
#include "sip_platform_task.h"
#include "ccsip_task.h"
#include "ccsip_messaging.h"
#include "ccsip_reldev.h"
#include "sip_common_transport.h"
#include "sip_csps_transport.h"
#include "sip_ccm_transport.h"
#include "sip_common_regmgr.h"
#include "util_string.h"
#include "dns_utils.h"
#include "debug.h"
#include "phntask.h"
#include "ccsip_subsmanager.h"
#include "ccsip_publish.h"
#include "ccsip_platform_tcp.h"
#include "ccsip_platform_tls.h"
#include "platform_api.h"
#include "sessionTypes.h"

uint16_t ccm_config_id_addr_str[MAX_CCM] = {
    CFGID_CCM1_ADDRESS,
    CFGID_CCM2_ADDRESS,
    CFGID_CCM3_ADDRESS
};

#ifdef IPV6_STACK_ENABLED

uint16_t ccm_config_id_ipv6_addr_str[MAX_CCM] = {
    CFGID_CCM1_IPV6_ADDRESS,
    CFGID_CCM2_IPV6_ADDRESS,
    CFGID_CCM3_IPV6_ADDRESS
};

#endif

uint16_t ccm_config_id_port[MAX_CCM] = {
    CFGID_CCM1_SIP_PORT,
    CFGID_CCM2_SIP_PORT,
    CFGID_CCM3_SIP_PORT
};

uint16_t ccm_config_id_sec_level[MAX_CCM] = {
    CFGID_CCM1_SEC_LEVEL,
    CFGID_CCM2_SEC_LEVEL,
    CFGID_CCM3_SEC_LEVEL
};

uint16_t ccm_config_id_is_valid[MAX_CCM] = {
    CFGID_CCM1_IS_VALID,
    CFGID_CCM2_IS_VALID,
    CFGID_CCM3_IS_VALID
};

ti_config_table_t CCM_Device_Specific_Config_Table[MAX_CCM];
ti_config_table_t CCM_Dummy_Entry;
ti_config_table_t CSPS_Config_Table[MAX_REG_LINES];
ti_config_table_t *CCM_Config_Table[MAX_REG_LINES + 1][MAX_CCM];
int phone_local_tcp_port[UNUSED_PARAM];
ti_csps_t CSPS_Device_Specific_Config_Table;

sip_tcp_conn_t sip_tcp_conn_tab[MAX_CONNECTIONS];

char sent_string[] = "Sent:";
char rcvd_string[] = "Rcvd:";
char cseq_string[] = " Cseq:";
char callid_string[] = " CallId:";

static cpr_socket_t listen_socket = INVALID_SOCKET;
extern cc_config_table_t CC_Config_Table[];
extern sipCallHistory_t gCallHistory[];
extern ccm_act_stdby_table_t CCM_Active_Standby_Table;

//extern uint16_t ccm_config_id_addr_str[MAX_CCM];
//extern uint16_t ccm_config_id_port[MAX_CCM];
extern void platAddCallControlClassifiers(
            unsigned long myIPAddr, unsigned short myPort,
            unsigned long cucm1IPAddr, unsigned short cucm1Port,
            unsigned long cucm2IPAddr, unsigned short cucm2Port,
            unsigned long cucm3IPAddr, unsigned short cucm3Port,
            unsigned char  protocol);
extern void platform_get_ipv4_address(cpr_ip_addr_t *ip_addr);

#define SIP_IPPROTO_UDP 17
#define SIP_IPPROTO_TCP 6

/*
 *  Function: ccsip_add_wlan_classifiers()
 *
 *  Parameters: None
 *
 *  Description: Add classifiers required for wlan
 *
 *  Returns: None
 *
 */
void ccsip_add_wlan_classifiers ()
{
    cpr_ip_addr_t    my_addr;
    uint32_t    local_port = 0;
    uint32_t  transport_prot = 0;

    platform_get_ipv4_address (&my_addr);

    config_get_value(CFGID_VOIP_CONTROL_PORT, &local_port,
                         sizeof(local_port));
    config_get_value(CFGID_TRANSPORT_LAYER_PROT, &transport_prot,
                         sizeof(transport_prot));

    platAddCallControlClassifiers(my_addr.u.ip4, local_port ,
            ntohl(CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common.addr.u.ip4),
            CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common.port,
            ntohl(CCM_Device_Specific_Config_Table[SECONDARY_CCM].ti_common.addr.u.ip4),
            CCM_Device_Specific_Config_Table[SECONDARY_CCM].ti_common.port,
            ntohl(CCM_Device_Specific_Config_Table[TERTIARY_CCM].ti_common.addr.u.ip4),
            CCM_Device_Specific_Config_Table[TERTIARY_CCM].ti_common.port,
            (transport_prot==CONN_UDP)? SIP_IPPROTO_UDP:SIP_IPPROTO_TCP);
}

void ccsip_remove_wlan_classifiers ()
{
    platRemoveCallControlClassifiers();

}

/*
 ** sipTransportGetServerHandle
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: This function gets the server handle for a particular
 *               line id.
 *
 *  RETURNS: The server handle
 *
 */
static cpr_socket_t
sipTransportGetServerHandle (line_t dn, line_t ndx)
{

    ti_config_table_t *ccm_table_ptr = NULL;
    ti_common_t ti_common;
    static const char *fname = "sipTransportGetServerHandle";

    /*
     * Checking for dn < 1, since we dereference the Config_Tables using
     * dn-1
     */
    if (((int)dn < 1) || ((int)dn > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn);
        return (INVALID_SOCKET);
    }

    if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (ndx == REG_BACKUP_CCB) {
            /*
             * This is the reg backup ccb, so return the
             * standby ccm handle
             */
            ccm_table_ptr = CCM_Active_Standby_Table.standby_ccm_entry;
        } else if (ndx > REG_BACKUP_CCB) {
            ccsipCCB_t *ccb = NULL;

            ccb = sip_sm_get_ccb_by_index(ndx);
            if (ccb != NULL) {
                ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
            } else {
                return (INVALID_SOCKET);
            }
        } else {
            ccm_table_ptr = CCM_Active_Standby_Table.active_ccm_entry;
        }
        if (ccm_table_ptr) {
            ti_common = ccm_table_ptr->ti_common;
            return (ti_common.handle);
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        return (sipTransportCSPSGetProxyHandleByDN(dn));
    }
    return (INVALID_SOCKET);
}

/*
 ** sipTransportGetServerHandleWithAddr
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: IP addr
 *
 *  DESCRIPTION: This function gets the server handle
 *
 *  RETURNS: The server handle
 *
 */
static cpr_socket_t
sipTransportGetServerHandleWithAddr (cpr_ip_addr_t *remote_ip_addr)
{

    ti_config_table_t *ccm_table_ptr = NULL;

    ccm_table_ptr = CCM_Active_Standby_Table.active_ccm_entry;

    if (ccm_table_ptr && util_compare_ip(remote_ip_addr, &(ccm_table_ptr->ti_common.addr))) {
        return (ccm_table_ptr->ti_common.handle);
    }
    ccm_table_ptr = CCM_Active_Standby_Table.standby_ccm_entry;
    if (ccm_table_ptr && util_compare_ip(remote_ip_addr, &(ccm_table_ptr->ti_common.addr))) {
        return (ccm_table_ptr->ti_common.handle);
    }
    return (INVALID_SOCKET);
}

/*
 ** sipTransportGetServerAddress
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: This function gets the server address for a particular
 *               line id.
 *
 *  RETURNS: 0 if successful
 *
 */
uint16_t
sipTransportGetServerAddress (cpr_ip_addr_t *pip_addr, line_t dn, line_t ndx)
{
    static const char *fname = "sipTransportGetServerAddress";
    *pip_addr = ip_addr_invalid;

    /*
     * Checking for dn < 1, since we dereference the Config_Tables using
     * dn-1
     */
    if (((int)dn < 1) || ((int)dn > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn);
        return (0);
    }

    if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (ndx == REG_BACKUP_CCB) {
            /*
             * This is the reg backup ccb, so return the
             * standby ccm addr
             */
            if (CCM_Active_Standby_Table.standby_ccm_entry) {
                *pip_addr = CCM_Active_Standby_Table.standby_ccm_entry->
                        ti_common.addr;
            }
            return (0);
        } else if (ndx > REG_BACKUP_CCB) {
            ccsipCCB_t *ccb = NULL;

            ccb = sip_sm_get_ccb_by_index(ndx);
            if (ccb != NULL) {
                ti_config_table_t *ccm_table_ptr = NULL;
                ti_common_t ti_common;

                ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
                if (ccm_table_ptr) {
                    ti_common = ccm_table_ptr->ti_common;
                    *pip_addr = ti_common.addr;
                }
            }
            return (0);
        } else {
            if (CCM_Active_Standby_Table.active_ccm_entry) {
                *pip_addr = CCM_Active_Standby_Table.active_ccm_entry->
                        ti_common.addr;
            }
            return (0);
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        return (sipTransportCSPSGetProxyAddressByDN(pip_addr, dn));
    }
}

/*
 ** sipTransportGetServerPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: This function gets the server port for a particular
 *               line id.
 *
 *  RETURNS: Server port as a short
 *
 */
short
sipTransportGetServerPort (line_t dn, line_t ndx)
{
    static const char *fname = "sipTransportGetServerPort";
    /*
     * Checking for dn < 1, since we dereference the Config_Tables using
     * dn-1
     */
    if (((int)dn < 1) || ((int)dn > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn);
        return (0);
    }

    if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (ndx == REG_BACKUP_CCB) {
            /*
             * This is the reg backup ccb, so return the
             * standby ccm port
             */
            if (CCM_Active_Standby_Table.standby_ccm_entry) {
                return ((short) CCM_Active_Standby_Table.standby_ccm_entry->
                        ti_common.port);
            }
        } else if (ndx > REG_BACKUP_CCB) {
            ccsipCCB_t *ccb = NULL;

            ccb = sip_sm_get_ccb_by_index(ndx);
            if (ccb != NULL) {
                ti_config_table_t *ccm_table_ptr = NULL;
                ti_common_t ti_common;

                ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
                if (ccm_table_ptr) {
                    ti_common = ccm_table_ptr->ti_common;
                    return (ti_common.port);
                }
            }
            return (0);
        } else {
            if (CCM_Active_Standby_Table.active_ccm_entry) {
                return ((short) CCM_Active_Standby_Table.active_ccm_entry->
                        ti_common.port);
            }
            return (0);
        }
    }

    /*
     * Assume CSPS for now.
     */
    return ((short) sipTransportCSPSGetProxyPortByDN(dn));
}

conn_create_status_t
sip_transport_setup_cc_conn (line_t dn, CCM_ID ccm_id)
{
    static const char *fname = "sip_transport_setup_cc_conn";
    int             dnsErrorCode;
    cpr_ip_addr_t   server_ipaddr;
    uint16_t        server_port = 0, listener_port = 0;
    cpr_socket_t    server_conn_handle = INVALID_SOCKET;
    conn_create_status_t status = CONN_INVALID;
    uint32_t        type;
    ti_common_t    *ti_common;
    int            ip_mode = CPR_IP_MODE_IPV4;
	uint32_t		s_port;

    /*
     * Checking for dn < 1, since we dereference the Config_Tables using
     * dn-1
     */
    if (((int)dn < 1) || ((int)dn > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn);
        return (status);
    }

    if (ccm_id >= MAX_CCM) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccm id <%d> out of bounds.\n",
                          fname, ccm_id);
        return (status);
    }
    CPR_IP_ADDR_INIT(server_ipaddr);

#ifdef IPV6_STACK_ENABLED

    config_get_value(CFGID_IP_ADDR_MODE,
                     &ip_mode, sizeof(ip_mode));
#endif

    if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        ti_ccm_t *ti_ccm;

        ti_ccm = &CCM_Config_Table[dn - 1][ccm_id]->ti_specific.ti_ccm;
        if (!ti_ccm->is_valid) {
            /*
             * dont even attempt to create a connection if the
             * platform has deemed the ccm info invalid
             */
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Admin has not configured a valid cucm for cucm index=%s=%d.\n",
                              fname, CCM_ID_PRINT(ccm_id), ccm_id);
            return (status);
        }
        dnsErrorCode = dnsGetHostByName(CCM_Config_Table[dn - 1][ccm_id]->
                                         ti_common.addr_str, &server_ipaddr,
                                         100, 1);
        if (dnsErrorCode != DNS_OK) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              "sip_transport_setup_cc_conn",
                                "dnsGetHostByName() returned error:%s",
                                CCM_Config_Table[dn - 1][ccm_id]->ti_common.addr_str);
            return status;
        }

        util_ntohl(&server_ipaddr, &server_ipaddr);


        config_get_value(CFGID_VOIP_CONTROL_PORT, &s_port, sizeof(s_port));
        server_port =  (uint16_t) s_port;

        if (CCM_Config_Table[dn - 1][ccm_id]->ti_common.conn_type == CONN_UDP) {
            type = SOCK_DGRAM;
            listener_port = CCM_Config_Table[dn - 1][ccm_id]->ti_common.listen_port;
        } else {
            type = SOCK_STREAM;
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        sipTransportGetServerIPAddr(&server_ipaddr, dn);
        server_port   = (uint16_t) sipTransportGetPrimServerPort(dn);
        if (CSPS_Config_Table[dn - 1].ti_common.conn_type == CONN_UDP) {
            type = SOCK_DGRAM;
            listener_port = CSPS_Config_Table[dn - 1].ti_common.listen_port;
        } else {
            type = SOCK_STREAM;
        }
    }
    if (util_check_if_ip_valid(&server_ipaddr) && server_port != 0) {
        char server_ipaddr_str[MAX_IPADDR_STR_LEN];
        int  ret_status = SIP_ERROR;

        ipaddr2dotted(server_ipaddr_str, &server_ipaddr);
        if (type == SOCK_DGRAM) {
            ret_status = sip_platform_udp_channel_create(ip_mode, &server_conn_handle,
                                                         &server_ipaddr,
                                                         server_port, 0);
            if (ret_status == SIP_OK) {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"DN <%d>: CC UDP socket opened: "
                                 "<%s>:<%d>, handle=<%d>\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), dn,
                                 server_ipaddr_str, server_port,
                                 server_conn_handle);
                status = CONN_SUCCESS;
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"DN <%d>:"
                                  "udp channel error"
                                  "server addr=%s, server port=%d) failed.\n",
                                  fname, dn, server_ipaddr_str, server_port);
                server_conn_handle = INVALID_SOCKET;
                status = CONN_FAILURE;
            }
        }
        else {
            sipSPIMessage_t sip_msg;

            if (CC_Config_Table[dn - 1].cc_type != CC_CCM) {
                /* We should not come here in non-ccm mode */
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"TLS and TCP not supported in non-ccm"
                                  " mode\n", fname);
                return (CONN_INVALID);
            }

            // TLS if tls and sec_level is not non secure
            if (((CCM_Config_Table[dn - 1][ccm_id]->ti_specific.ti_ccm.sec_level
                    == AUTHENTICATED) ||
                 (CCM_Config_Table[dn - 1][ccm_id]->ti_specific.ti_ccm.sec_level
                    == ENCRYPTED)) &&
                (CCM_Config_Table[dn - 1][ccm_id]->ti_common.conn_type == CONN_TLS)) {
                uint32_t port = 0;

                CCSIP_DEBUG_TASK(DEB_F_PREFIX"server_ipaddr %p", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), &server_ipaddr);
                sip_msg.createConnMsg.addr = server_ipaddr;
                config_get_value(ccm_config_id_port[ccm_id], &port,
                                 sizeof(port));
                sip_msg.createConnMsg.port = (uint16_t) port;
                sip_msg.context = NULL;
                server_conn_handle = sip_tls_create_connection(&sip_msg, TRUE,
                        CCM_Config_Table[dn - 1][ccm_id]->ti_specific.ti_ccm.sec_level);
                if (server_conn_handle != INVALID_SOCKET) {
                    CCM_Config_Table[dn - 1][ccm_id]->ti_common.port =
                        (uint16_t) port;
                }
            } else {
                sip_msg.createConnMsg.addr = server_ipaddr;
                sip_msg.createConnMsg.port = server_port;
                sip_msg.context = NULL;
                server_conn_handle = sip_tcp_create_connection(&sip_msg);
            }
            if (server_conn_handle != INVALID_SOCKET) {
                listener_port = sip_msg.createConnMsg.local_listener_port;
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"DN <%d>: CC TCP socket opened: "
                                 "to <%s>:<%d>, local_port: %d handle=<%d>\n",
                                 DEB_F_PREFIX_ARGS(SIP_TRANS, fname), dn, server_ipaddr_str,
                                 server_port, listener_port,
                                 server_conn_handle);
                status = CONN_SUCCESS;
                phone_local_tcp_port[CCM_Config_Table[dn-1][ccm_id]->ti_specific.ti_ccm.ccm_id] =
                    sip_msg.createConnMsg.local_listener_port;
            } else {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"DN <%d>:"
                                  "tcp channel create error "
                                  "server addr=%s, server port=%d) failed.\n",
                                  fname, dn, server_ipaddr_str, server_port);
                status = CONN_FAILURE;
            }
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"DN <%d>: CC address/port not configured.\n",
                         fname, dn);
        status = CONN_INVALID;
    }

    if ((status == CONN_SUCCESS) || (status == CONN_FAILURE)) {
        if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
            /*
             * regmgr
             */
            /*
             * Add the newly created socket to the CCM as the one
             * we listen for incoming messages as well.
             */
            ti_common = &CCM_Config_Table[dn - 1][ccm_id]->ti_common;
        } else {
            /*
             * Non CCM Case
             */
            ti_common = &CSPS_Config_Table[dn - 1].ti_common;
        }
        ti_common->addr = server_ipaddr;
        ti_common->port = server_port;
        ti_common->handle = server_conn_handle;
        ti_common->listen_port = listener_port;
    }

    return (status);
}


int
sip_transport_destroy_cc_conn (line_t dn, CCM_ID ccm_id)
{
    static const char *fname = "sip_transport_destroy_cc_conn";
    int          disconnect_status = 0;
    cpr_socket_t cc_handle;
    CONN_TYPE    conn_type;
    uint16_t     max_cc_count, cc_index;
    ti_common_t *ti_common;
    CC_ID        cc_type;

    /*
     * Checking for dn < 1, since we dereference the Config_Tables using
     * dn-1
     */
    if (((int)dn < 1) || ((int)dn > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn);
        return (disconnect_status);
    }

    if (ccm_id >= MAX_CCM) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccm id <%d> out of bounds.\n",
                          fname, ccm_id);
        return (disconnect_status);
    }

    cc_type = CC_Config_Table[dn - 1].cc_type;
    if (cc_type == CC_CCM) {
        /*
         * regmgr
         */
        ti_common = &CCM_Config_Table[dn - 1][ccm_id]->ti_common;
        max_cc_count = MAX_CCM;
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_common = &CSPS_Config_Table[dn - 1].ti_common;
        max_cc_count = MAX_CSPS;
    }
    cc_index = 0;
    do {
        cc_handle = ti_common->handle;
        conn_type = ti_common->conn_type;
        if (cc_handle != INVALID_SOCKET) {
            /* Close the UDP send channel to the proxy */
            if (sip_platform_udp_channel_destroy(cc_handle) < 0) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"DN <%d>:"
                                  "handle=%d) \n", fname, dn, cc_handle);
                disconnect_status = -1;
            } else {
                CCSIP_DEBUG_TASK(DEB_F_PREFIX"DN <%d>: CC socket closed: "
                                 "handle=<%d>\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), dn, cc_handle);
                disconnect_status = 0;
            }
            if (conn_type != CONN_UDP) {
                int connid;

                connid = sip_tcp_fd_to_connid(ti_common->handle);
                sipTcpFreeSendQueue(connid);
                sip_tcp_purge_entry(connid);
            }
        } else {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"DN <%d>: CC socket already closed.\n",
                             DEB_F_PREFIX_ARGS(SIP_TRANS, fname), dn);
            disconnect_status = 0;
        }
        cc_index++;
        ti_common = &CCM_Config_Table[dn - 1][cc_index]->ti_common;
        /*
         * Will break out for CSPS the first time in the loop.
         */
    } while (cc_index < max_cc_count);
    if (listen_socket != INVALID_SOCKET) {
        if (sip_platform_udp_channel_destroy(listen_socket) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"DN <%d>:"
                              "(handle=%d)\n", fname, dn, listen_socket);
            disconnect_status = -1;
        } else {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"DN <%d>: CC socket closed: handle=<%d>\n",
                             DEB_F_PREFIX_ARGS(SIP_TRANS, fname), dn, listen_socket);
            disconnect_status = 0;
        }
        sip_platform_task_reset_listen_socket(listen_socket);
        listen_socket = INVALID_SOCKET;
    }
    if (CC_Config_Table[dn - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        CCM_Config_Table[dn - 1][ccm_id]->ti_common.handle = INVALID_SOCKET;
    } else {
        ti_common = &CSPS_Config_Table[dn - 1].ti_common;
        ti_common->addr = ip_addr_invalid;
        ti_common->port = 0;
        ti_common->handle = INVALID_SOCKET;
    }
    return (disconnect_status);
}

/*
 ** sipTransportCreateSendMessage
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: This function first creates the char * sip message that
 *               needs to be sent out and then calls the
 *               sipTransportSendMessage() to send out the message.
 *
 *  RETURNS: -1 if failure and 0 if it succeeds.
 *
 *  NOTE: This function will be consolidated with the following function
 *        so that only the following function will be necessary. Also the
 *        signature of the new function that results will be made similar
 *        to that of the IOS sip stacks sipTransportSendMessage() call.
 *        That will make the port of IOS based Propel SIP stack into the
 *        phone easier.
 *
 */
int
sipTransportCreateSendMessage (ccsipCCB_t *ccb,
                               sipMessage_t *pSIPMessage,
                               sipMethod_t message_type,
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port,
                               boolean isRegister,
                               boolean reTx,
                               int timeout,
                               void *cbp,
                               int reldev_stored_msg)
{
    const char *fname = "sipTransportCreateSendMessage";
    static char aOutBuf[SIP_UDP_MESSAGE_SIZE + 1];
    uint32_t    nbytes = SIP_UDP_MESSAGE_SIZE;
    hStatus_t   sippmh_write_status = STATUS_FAILURE;

    /*
     * Check args
     */
    if (!pSIPMessage) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args: pSIPMessage is null\n", fname);
        return (-1);
    }

    /*
     * Get message from the reliable delivery stored msg first and
     * if there is no message from the reliable delivery message storage
     * then compose the SIP message.
     */
    nbytes = sipRelDevGetStoredCoupledMessage(reldev_stored_msg, &aOutBuf[0],
                                              nbytes);
    if (nbytes == 0) {
        nbytes = SIP_UDP_MESSAGE_SIZE;
        sippmh_write_status = sippmh_write(pSIPMessage, aOutBuf, &nbytes);
    } else {
        sippmh_write_status = STATUS_SUCCESS;
    }
    ccsip_dump_send_msg_info(aOutBuf, pSIPMessage, cc_remote_ipaddr,
                            cc_remote_port);

    free_sip_message(pSIPMessage);
    if (sippmh_write_status == STATUS_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          ccb ? ccb->index : 0, ccb ? ccb->dn_line : 0, fname,
                          "sippmh_write()");
        return (-1);
    }
    if ((aOutBuf[0] == '\0') || (nbytes == 0)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_write() returned empty buffer "
                          "string\n", fname);
        return (-1);
    }
    aOutBuf[nbytes] = '\0'; /* set NULL string for debug printing */

    if (sipTransportSendMessage(ccb, aOutBuf, nbytes, message_type,
                                cc_remote_ipaddr, cc_remote_port, isRegister,
                                reTx, timeout, cbp) < 0) {
        if (ccb) {
            CCSIP_DEBUG_ERROR("SIPCC-ENTRY: LINE %d/%d: %-35s: message not "
                "sent of type %s=%d. sipTransportSendMessage() failed.\n",
                 ccb->index, ccb->dn_line, fname,
                 message_type == sipMethodRegister ? "sipMethodRegister" : "", sipMethodRegister);
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sipTransportSendMessage()");
        }
        return (-1);
    }

    return (0);
}

/*
 ** sipTransportSendMessage
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Sends out the sip message.
 *
 *  RETURNS: -1 on failure and 0 on success.
 *
 *  NOTE: This function will be consolidated with the following function
 *        so that only the following function will be necessary. Also the
 *        signature of the new function that results will be made similar
 *        to that of the IOS sip stacks sipTransportSendMessage() call.
 *        That will make the port of IOS based Propel SIP stack into the
 *        phone easier.
 */
int
sipTransportSendMessage (ccsipCCB_t *ccb,
                         char *pOutMessageBuf,
                         uint32_t nbytes,
                         sipMethod_t message_type,
                         cpr_ip_addr_t *cc_remote_ipaddr,
                         uint16_t cc_remote_port,
                         boolean isRegister,
                         boolean reTx,
                         int timeout,
                         void *cbp)
{
    const char *fname = "sipTransportSendMessage";
    char         cc_config_ipaddr_str[MAX_IPADDR_STR_LEN];
    char         cc_remote_ipaddr_str[MAX_IPADDR_STR_LEN];
    char         obp_address[MAX_IPADDR_STR_LEN];
    cpr_socket_t send_to_proxy_handle = INVALID_SOCKET;
    int          nat_enable, dnsErrorCode;
    const char  *conn_type;
    uint32_t     local_udp_port = 0;
    int          tcp_error = SIP_TCP_SEND_OK;
    int          ip_mode = CPR_IP_MODE_IPV4;

    /*
     * Check args
     */
    if ((!pOutMessageBuf) || (pOutMessageBuf[0] == '\0')) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args: pOutMessageBuf is empty\n", fname);
        return (-1);
    }

#ifdef IPV6_STACK_ENABLED
    config_get_value(CFGID_IP_ADDR_MODE,
                     &ip_mode, sizeof(ip_mode));
#endif
    conn_type = sipTransportGetTransportType(1, TRUE, ccb);
    if (ccb) {
        /* Check to see whether the supplied address and port
         * match those of the currently configured proxy.
         * If not, open a new UDP channel, send the message,
         * and close the channel.
         */
        /* Check to see whether the supplied address and port
         * match those of the currently configured proxy.
         * If not, open a new UDP channel, send the message,
         * and close the channel.
         */
        cpr_ip_addr_t cc_config_ipaddr;
        uint16_t cc_config_port;

        sipTransportGetServerAddress(&cc_config_ipaddr, ccb->dn_line, ccb->index);
        cc_config_port   = sipTransportGetServerPort(ccb->dn_line, ccb->index);

        /*
         * Convert IP address to string, for debugs
         */
        if (SipDebugMessage) {
            ipaddr2dotted(cc_config_ipaddr_str, &cc_config_ipaddr);
            ipaddr2dotted(cc_remote_ipaddr_str, cc_remote_ipaddr);
        }

        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"ccb <%d>: config <%s>:<%d> - remote <%s>:<%d>\n",
                            DEB_F_PREFIX_ARGS(SIP_TRANS, fname), ccb->index,
                            cc_config_ipaddr_str, cc_config_port,
                            cc_remote_ipaddr_str, cc_remote_port);

        if (conn_type != NULL) {
            if (!cpr_strcasecmp(conn_type, "UDP")) {
                if (util_compare_ip(&cc_config_ipaddr, cc_remote_ipaddr) &&
                    (cc_config_port == cc_remote_port)) {
                    send_to_proxy_handle = sipTransportGetServerHandle(ccb->dn_line,
                                                                       ccb->index);
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Got handle %d\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                     send_to_proxy_handle);
                }
            } else { /* TCP */
                if (util_compare_ip(&cc_config_ipaddr, cc_remote_ipaddr)) {
                    send_to_proxy_handle = sipTransportGetServerHandle(ccb->dn_line,
                                                                       ccb->index);
                    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Got handle %d\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                     send_to_proxy_handle);
                    if (send_to_proxy_handle == INVALID_SOCKET) {
                        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Invalid socket\n", fname);
                        return (-1);
                    }
                }
            }
        } else {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "Invalid Connection type returned");
            return (-1);
        }
    }

    /*
     * Make the send code below believe that we do not have a connection to
     * the remote side so that it creates a send port for each SIP message.
     * This send port will have a source UDP port retrieved from the NAT
     * configuration settings
     */
    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 1) {
        send_to_proxy_handle = INVALID_SOCKET;
        config_get_value(CFGID_VOIP_CONTROL_PORT, &local_udp_port,
                         sizeof(local_udp_port));
    }
    /*
     * Check if we need to get the outbound proxy address and port.  If we
     * do then we will make the code below believe we do not have a
     * connection to the remote side so it creates a send port for this
     * message.  This involves two checks, the first check is if outbound
     * proxy support is enabled at all.  The second check is that only
     * REQUEST messages go to the outbound proxy.  All other message
     * types follow the normal rules for sending
     */
    if (ccb) {
        if (ccb->outBoundProxyPort == 0) {
            sipTransportGetOutbProxyAddress(ccb->dn_line, obp_address);
            if ((cpr_strcasecmp(obp_address, UNPROVISIONED) != 0) &&
                (obp_address[0] != 0) && (obp_address[0] != '0')) {

                /* Outbound proxy is configured, get it's IP address */
                dnsErrorCode = sipTransportGetServerAddrPort(obp_address,
                                         &ccb->outBoundProxyAddr,
                                         (uint16_t *)&ccb->outBoundProxyPort,
                                         &ccb->ObpSRVhandle,
                                         TRUE);

                if (dnsErrorCode != DNS_OK) {
                    /*
                     * SRV failed, do a DNS A record lookup
                     * on the outbound proxy
                     */
                    if (util_check_if_ip_valid(&(ccb->outBoundProxyAddr)) == FALSE) {
                        dnsErrorCode = dnsGetHostByName(obp_address,
                                                         &ccb->outBoundProxyAddr,
                                                         100, 1);
                    }
                    if (dnsErrorCode == DNS_OK) {
                        util_ntohl(&(ccb->outBoundProxyAddr),&(ccb->outBoundProxyAddr));
                    } else {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                          "sipTransportSendMessage",
                                          "dnsGetHostByName() returned error");

                        ccb->outBoundProxyAddr = ip_addr_invalid;
                        ccb->outBoundProxyPort = 0;
                        return (-1);
                    }
                } else {
                    util_ntohl(&(ccb->outBoundProxyAddr), &(ccb->outBoundProxyAddr));
                }
            }
        }

        /*
         * only use outbound proxy if we have one configured,
         * we are using the default proxy, we are not using the Emergency
         * route, and it is not the backup proxy registration.
         */
        if (util_check_if_ip_valid(&(ccb->outBoundProxyAddr)) &&
            (ccb->proxySelection == SIP_PROXY_DEFAULT) &&
            (ccb->routeMode != RouteEmergency) && (ccb->index != REG_BACKUP_CCB)) {
            send_to_proxy_handle = INVALID_SOCKET;
            switch (message_type) {
            case sipMethodResponse:
            case sipMethodUnknown:
                ccb->outBoundProxyPort = cc_remote_port;
                break;
            default:
                /*
                 * We have determined that this message is a REQUEST
                 * so we will change the one time outgoing port to
                 * the outbound proxy address and port
                 */
                *cc_remote_ipaddr = ccb->outBoundProxyAddr;
                if (ccb->outBoundProxyPort != 0) {
                    cc_remote_port = (uint16_t) ccb->outBoundProxyPort;
                } else {
                    cc_remote_port = (uint16_t) sipTransportGetOutbProxyPort(ccb->dn_line);
                    ccb->outBoundProxyPort = cc_remote_port;
                }
                break;
            }
        }
    }

    if (SipDebugTask || SipDebugMessage) {
        ipaddr2dotted(cc_remote_ipaddr_str, cc_remote_ipaddr);
    }

    if ((conn_type != NULL) && (cpr_strcasecmp(conn_type, "UDP")) &&
        (send_to_proxy_handle == INVALID_SOCKET)) {
        send_to_proxy_handle = sipTransportGetServerHandleWithAddr(cc_remote_ipaddr);
        CCSIP_DEBUG_TASK(DEB_F_PREFIX"<%s> remote ip addr\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), cc_remote_ipaddr_str);
        if (send_to_proxy_handle == INVALID_SOCKET) {
            // for TCP and TLS return if we do not have a connection to the
            // remote side
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"No connection to ip addr <%s>\n", fname, cc_remote_ipaddr_str);
            return (-1);
        }
    }
    if (send_to_proxy_handle != INVALID_SOCKET) {
        /*
         * Get the connection type and call ...sendto() or
         * ...send() appropriately (for udp and tcp respectively).
         * Also this will need to change to send the correct line
         * when we go with supporting CCM and proxy on the
         * same EP. For now we can just pass '0' as line as the phone
         * is only going to support either CSPS or CCM and not both at
         * the same time.
         */
        if (!cpr_strcasecmp(conn_type, "UDP")) {
            if (sip_platform_udp_channel_sendto(send_to_proxy_handle,
                                                pOutMessageBuf,
                                                nbytes,
                                                cc_remote_ipaddr,
                                                cc_remote_port) == SIP_ERROR) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sip_platform_udp_channel_sendto()");
                return (-1);
            }
        }
        else {
            /*
             * Assume TCP for now
             */
            tcp_error = sip_tcp_channel_send(send_to_proxy_handle,
                                     pOutMessageBuf,
                                     (unsigned short)nbytes);
            if (tcp_error == SIP_TCP_SEND_ERROR) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sip_platform_tcp_channel_send()");
                return (-1);
            }
        }

        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Sent SIP message: handle=<%d>,"
                            "length=<%d>, message=\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                            send_to_proxy_handle, nbytes);
        CCSIP_DEBUG_MESSAGE_PKT(pOutMessageBuf);
    } else {
        /* Create a new handle for the supplied address and port */
        cpr_socket_t one_time_handle = INVALID_SOCKET;
        int status = -1;

        /* Open UDP send channel to the specified address and port */
        status = sip_platform_udp_channel_create(ip_mode, &one_time_handle,
                                                 cc_remote_ipaddr,
                                                 cc_remote_port,
                                                 local_udp_port);
        if (status < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sip_platform_udp_channel_create()");
            return (-1);
        }
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Opened a one-time UDP send channel to server "
                            "<%s>:<%d>, handle = %d local port= %d\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                            cc_remote_ipaddr_str, cc_remote_port,
                            one_time_handle, local_udp_port);

        /* Send the message */
        if (sip_platform_udp_channel_sendto(one_time_handle,
                                            pOutMessageBuf,
                                            nbytes,
                                            cc_remote_ipaddr,
                                            cc_remote_port) == SIP_ERROR) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sip_platform_udp_channel_sendto()");
            /* Close the UDP send channel */
            status = sip_platform_udp_channel_destroy(one_time_handle);
            if (status < 0) {
                CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "sip_platform_udp_channel_destroy()");
            }
            return (-1);
        }
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Sent SIP message to <%s>:<%d>, "
                            "handle=<%d>, length=<%d>, message=\n",
                            DEB_F_PREFIX_ARGS(SIP_TRANS, fname), cc_remote_ipaddr_str, cc_remote_port,
                            one_time_handle, nbytes);
        CCSIP_DEBUG_MESSAGE_PKT(pOutMessageBuf);

        /* Close the UDP send channel */
        status = sip_platform_udp_channel_destroy(one_time_handle);
        if (status < 0) {
            CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                              fname, "sip_platform_udp_channel_destroy()");
            return (-1);
        }
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Closed a one-time UDP send channel "
                            "handle = %d\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), one_time_handle);
    }

    if (ccb) {
        //
        // Cancel any outstanding reTx timers, if any
        //
        /*
         * Dont do for fallback ccb's.
         */
        if ((ccb->index <= REG_BACKUP_CCB) && reTx) {
            CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_ENTRY),
                              ccb->index, ccb->dn_line, fname,
                              "Stopping reTx timer");
            sip_platform_msg_timer_stop(ccb->index);

            /*
             * If the specified timeout value is not 0 and UDP, start the reTx timer
             * When the phone is in remote location, then REG message send may fail
             * for TCP. In that case, that message has to be retransmitted from the SIP
             * layer. So if the error_no == CPR_ENOTCONN, then send the SIP message
             * during next retry.
             *
             */
            if ((timeout > 0) && ((!cpr_strcasecmp(conn_type, "UDP")
                || ((tcp_error == CPR_ENOTCONN) && (!cpr_strcasecmp(conn_type, "TCP")))))) {
                void *data;

                data = isRegister ? (void *) ccb : (void *)(long)ccb->index;

                CCSIP_DEBUG_STATE(DEB_F_PREFIX"LINE %d/%d: Starting reTx timer (%d "
                                  "msec)\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), ccb->index, ccb->dn_line,
                                  timeout);
                ccb->retx_flag = TRUE;
                if (sip_platform_msg_timer_start(timeout, data, ccb->index,
                                                 pOutMessageBuf, nbytes,
                                                 (int) message_type, cc_remote_ipaddr,
                                                 cc_remote_port, isRegister) != SIP_OK) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED), ccb->index,
                                      ccb->dn_line, fname,
                                      "sip_platform_msg_timer_start()");
                    ccb->retx_flag = FALSE;
                }
            }
        }

        if (sipMethodCancel == message_type || sipMethodBye == message_type) {
            gCallHistory[ccb->index].last_bye_dest_ipaddr = *cc_remote_ipaddr;
            gCallHistory[ccb->index].last_bye_dest_port   = cc_remote_port;
        }
    }
    else {
        sipSCB_t *scbptr = (sipSCB_t *)cbp;
        ccsip_publish_cb_t *pcb_p = (ccsip_publish_cb_t *)cbp;
        sipTCB_t *tcbp = (sipTCB_t *)cbp;

        if (cbp != NULL) {
            sipPlatformUITimer_t *timer = NULL;
            uint32_t id = 0;

            scbptr->hb.retx_flag = TRUE;
            if (((ccsip_common_cb_t *)cbp)->cb_type == SUBNOT_CB) {
                timer = &(sipPlatformUISMSubNotTimers[scbptr->line]);
                id = scbptr->line;
            } else if (((ccsip_common_cb_t *)cbp)->cb_type == PUBLISH_CB) {
                timer = &(pcb_p->retry_timer);
                id = pcb_p->pub_handle;
            } else { // unsolicited NOTIFY
                int temp_timeout = 0;
                config_get_value(CFGID_TIMER_T1, &temp_timeout, sizeof(temp_timeout));
                temp_timeout = (64 * temp_timeout);
                if (cprStartTimer(tcbp->timer, temp_timeout, (void *)(long)(tcbp->trxn_id)) == CPR_FAILURE) {
                    CCSIP_DEBUG_STATE(DEB_F_PREFIX"%s failed\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname), "cprStartTimer");
                }
            }

            if (timeout > 0 && timer != NULL) {
                CCSIP_DEBUG_STATE(DEB_F_PREFIX"Starting reTx timer for %d secs",
                                  DEB_F_PREFIX_ARGS(SIP_TRANS, fname), timeout);
                if (sip_platform_msg_timer_subnot_start(timeout, timer, id,
                                                        pOutMessageBuf, nbytes,
                                                        (int) message_type,
                                                        cc_remote_ipaddr,
                                                        cc_remote_port) != SIP_OK) {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                      fname, "sip_platform_msg_timer_subnot_start");
                }
            }
        }

    }

    return (0);
}

/*
 ** sipTransportGetListenPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: Reads and returns the Primary server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Primary Server port (Currently only SIP Proxy specific)
 *
 */
uint16_t
sipTransportGetListenPort (line_t line, ccsipCCB_t *ccb)
{
    ti_config_table_t *ccm_table_ptr = NULL;
    static const char *fname = "sipTransportGetListenPort";

    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return 0;
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (ccb) {
            ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
        }
        if (ccm_table_ptr) {
            CCM_ID ccm_id;

            ccm_id = ccm_table_ptr->ti_specific.ti_ccm.ccm_id;
            if (ccm_id >= MAX_CCM) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"ccm id <%d> out of bounds.\n",
                                  fname, ccm_id);
                return 0;
            }
            return ((uint16_t) CCM_Config_Table[line - 1][ccm_id]->
                    ti_common.listen_port);
        } else if (CCM_Active_Standby_Table.active_ccm_entry != NULL) {
            return ((uint16_t) CCM_Active_Standby_Table.active_ccm_entry->
                    ti_common.listen_port);
        } else {
            return ((uint16_t) CCM_Config_Table[line - 1][PRIMARY_CCM]->
                    ti_common.listen_port);
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        return ((uint16_t) CSPS_Config_Table[line - 1].ti_common.listen_port);
    }
}

/*
 ** sipTransportGetTransportType
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: Reads and returns the Primary server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Primary Server port (Currently only SIP Proxy specific)
 *
 */
const char *
sipTransportGetTransportType (line_t line, boolean upper_case,
                              ccsipCCB_t *ccb)
{
    const char *tcp, *udp, *tls;
    CONN_TYPE conn_type;
    ti_config_table_t *ccm_table_ptr = NULL;
    static const char *fname = "sipTransportGetTransportType";

    tcp = (upper_case) ? "TCP" : "tcp";
    udp = (upper_case) ? "UDP" : "udp";
    tls = (upper_case) ? "TLS" : "tls";

    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return udp;
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        // If ccb is not NULL get the conn type from cc_cfg_table_entry
        // This would be the case for REGISTER/keep alive messages.
        // else get from the active ccm entry if available.
        if (ccb) {
            ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
        }
        if (ccm_table_ptr) {
            conn_type = ccm_table_ptr->ti_common.conn_type;
        } else if (CCM_Active_Standby_Table.active_ccm_entry != NULL) {
            conn_type = CCM_Active_Standby_Table.active_ccm_entry->ti_common.conn_type;
        } else {
            conn_type = CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common.conn_type;
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        conn_type = CSPS_Config_Table[line - 1].ti_common.conn_type;
    }
    switch (conn_type) {
    case CONN_UDP:
        return (udp);
    case CONN_TCP:
    case CONN_TCP_TMP:
        return (tcp);
    case CONN_TLS:
        return (tls);
    default:
        return (NULL);
    }
}

/*
 ** sipTransportGetServerAddrPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Wrapper function for the sip dns srv functions.
 *               Currently code only caters to SIP Proxies. But
 *               with CCM, the code will have handle the different
 *               scenarios of retries etc in the CCM environment as
 *               well.
 *
 *  RETURNS: The dns error codes that the invoked functions return.
 *
 */
int
sipTransportGetServerAddrPort (char *domain, cpr_ip_addr_t *ipaddr_ptr,
                               uint16_t *port, srv_handle_t *psrv_order,
                               boolean retried_addr)
{
    int rc;

    if (psrv_order == NULL) {
        rc = sip_dns_gethostbysrvorname(domain, ipaddr_ptr, port);
    } else {
        rc = sip_dns_gethostbysrv(domain, ipaddr_ptr, port, psrv_order,
                                  retried_addr);
    }
    return (rc);
}

/*
 ** sipTransportGetPrimServerPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: Reads and returns the Primary server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Primary Server port (Currently only SIP Proxy specific)
 *
 */
int
sipTransportGetPrimServerPort (line_t line)
{
    static const char *fname = "sipTransportGetPrimServerPort";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (0);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (CCM_Active_Standby_Table.active_ccm_entry != NULL) {
            return (CCM_Active_Standby_Table.active_ccm_entry->
                    ti_common.port);
        } else {
            return (0);
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        return (CSPS_Config_Table[line - 1].ti_common.port);
    }
}

/*
 ** sipTransportGetBkupServerPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Backup server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Backup Server port (Currently only  SIP Proxy specific)
 *
 */
int
sipTransportGetBkupServerPort (line_t line)
{
    static const char *fname = "sipTransportGetBkupServerPort";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (0);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr -
         */
        return (0);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        return (ti_csps->bkup_pxy_port);
    }
}

/*
 ** sipTransportGetEmerServerPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Emergency server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Emergency Server port (Currently only  SIP Proxy specific)
 *
 */
int
sipTransportGetEmerServerPort (line_t line)
{
    static const char *fname = "sipTransportGetEmerServerPort";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (0);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        return (0);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        return (ti_csps->emer_pxy_port);
    }
}

/*
 ** sipTransportGetOutbProxyPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Outbound server port from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: Outbound Server port (Currently only SIP Proxy specific)
 *
 */
int
sipTransportGetOutbProxyPort (line_t line)
{
    static const char *fname = "sipTransportGetOutbProxyPort";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (0);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        return (0);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        return (ti_csps->outb_pxy_port);
    }
}

/*
 ** sipTransportGetPrimServerAddress
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Primary server address from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: IP address in buffer (Currently only SIP Proxy specific)
 *
 */
cpr_ip_type
sipTransportGetPrimServerAddress (line_t line, char *buffer)
{
    ti_common_t *ti_common;
    cpr_ip_type ip_type = CPR_IP_ADDR_IPV4;
    static const char *fname = "sipTransportGetPrimServerAddress";

    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (ip_type);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        if (CCM_Active_Standby_Table.active_ccm_entry != NULL) {
            sstrncpy(buffer, CCM_Active_Standby_Table.active_ccm_entry->
                     ti_common.addr_str, MAX_IPADDR_STR_LEN);
            ip_type = CCM_Active_Standby_Table.active_ccm_entry->ti_common.addr.type;

        } else {
            ti_common = &CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common;
            sstrncpy(buffer, ti_common->addr_str, MAX_IPADDR_STR_LEN);
            ip_type = ti_common->addr.type;
        }
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_common = &CSPS_Config_Table[line - 1].ti_common;
        sstrncpy(buffer, ti_common->addr_str, MAX_IPADDR_STR_LEN);
        ip_type = ti_common->addr.type;
    }

    return(ip_type);
}

/*
 ** sipTransportGetBkupServerAddress
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Backup server address from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: IP Address as uint32_t and str in buffer (Currently only
 *           SIP Proxy specific)
 *
 */
uint16_t
sipTransportGetBkupServerAddress (cpr_ip_addr_t *pip_addr,
                                  line_t line, char *buffer)
{
    static const char *fname = "sipTransportGetBkupServerAddress";
    *pip_addr = ip_addr_invalid;

    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return (0);
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         * This would be standby address for the ccm
         */
        sstrncpy(buffer, "UNPROVISIONED", MAX_IPADDR_STR_LEN);
        return (0);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        sstrncpy(buffer, ti_csps->bkup_pxy_addr_str, MAX_IPADDR_STR_LEN);
        *pip_addr = ti_csps->bkup_pxy_addr;
        return (1);
    }
}

/*
 ** sipTransportGetEmerServerAddress
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Emergency server address from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: IP address in buffer (Currently only SIP Proxy specific)
 *
 */
void
sipTransportGetEmerServerAddress (line_t line, char *buffer)
{
    static const char *fname = "sipTransportGetEmerServerAddress";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return;
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        sstrncpy(buffer, "UNPROVISIONED", MAX_IPADDR_STR_LEN);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        sstrncpy(buffer, ti_csps->emer_pxy_addr_str, MAX_IPADDR_STR_LEN);
    }
}

/*
 ** sipTransportGetOutbProxyAddress
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION: Reads and returns the Outbound server address from the
 *               local UDP table that is filled at init time and during
 *               config change notifications.
 *
 *  RETURNS: IP address in buffer (Currently only SIP Proxy specific)
 *
 */
void
sipTransportGetOutbProxyAddress (line_t line, char *buffer)
{
    static const char *fname = "sipTransportGetOutbProxyAddress";
    /*
     * Checking for line < 1, since we dereference the Config_Tables using
     * line-1
     */
    if (((int)line < 1) || ((int)line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, line);
        return;
    }

    if (CC_Config_Table[line - 1].cc_type == CC_CCM) {
        /*
         * regmgr
         */
        sstrncpy(buffer, "UNPROVISIONED", MAX_IPADDR_STR_LEN);
    } else {
        /*
         * Assume CSPS for now.
         */
        ti_csps_t *ti_csps;

        ti_csps = CSPS_Config_Table[line - 1].ti_specific.ti_csps;
        sstrncpy(buffer, ti_csps->outb_pxy_addr_str, MAX_IPADDR_STR_LEN);
    }
}

/*
 * sipTransportGetServerIPAddr()
 *
 * Perform DNS lookup on the primary proxy name and
 * return its IP address.
 *
 * Note: the IP Address is returned in the non-Telecaster
 *       SIP format, which is not byte reversed.
 *       Eg. 0xac2c33f8 = 161.44.51.248
 */
void
sipTransportGetServerIPAddr (cpr_ip_addr_t *pip_addr, line_t line)
{
    const char     *fname = "sipTransportGetServerIPAddr";
    cpr_ip_addr_t   IPAddress;
    uint16_t        port;
    srv_handle_t     srv_order = NULL;
    int             dnsErrorCode = 0;
    char            addr[MAX_IPADDR_STR_LEN];
    char            obp_address[MAX_IPADDR_STR_LEN];

    CPR_IP_ADDR_INIT(IPAddress);

    sipTransportGetOutbProxyAddress(line, obp_address);
    if ((cpr_strcasecmp(obp_address, UNPROVISIONED) != 0) &&
        (obp_address[0] != 0) && (obp_address[0] != '0')) {
        sstrncpy(addr, obp_address, MAX_IPADDR_STR_LEN);
    } else {
        sipTransportGetPrimServerAddress(line, addr);
    }
    dnsErrorCode = sipTransportGetServerAddrPort(addr, &IPAddress, &port,
                                                 &srv_order, FALSE);
    if (srv_order) {
        dnsFreeSrvHandle(srv_order);
    }

    if (dnsErrorCode != DNS_OK) {
        //CCSIP_DEBUG_TASK(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
        //                  fname, "sipTransportGetServerAddrPort");
        dnsErrorCode = dnsGetHostByName(addr, &IPAddress, 100, 1);
    }

    if (dnsErrorCode != 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "dnsGetHostByName()");
    }
    *pip_addr = IPAddress;
    util_ntohl(pip_addr, &IPAddress);
}

/*
 ** sip_regmgr_set_cc_info
 *
 *  FILENAME: ip_phone\sip\sip_common_regmgr.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sip_regmgr_set_cc_info (line_t line, line_t dn_line,
                        CC_ID *cc_type, void *cc_table_entry)
{
    static const char *fname = "sip_regmgr_set_cc_info";
    ti_config_table_t **active_standby_table_entry =
        (ti_config_table_t **) cc_table_entry;

    /*
     * Checking for dn_line < 1, since we dereference the Config_Tables using
     * dn_line-1
     */
    if (((int)dn_line < 1) || ((int)dn_line > MAX_REG_LINES)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Args check: DN <%d> out of bounds.\n",
                          fname, dn_line);
        return;
    }

    *cc_type = CC_Config_Table[dn_line - 1].cc_type;
    if (*cc_type == CC_CCM) {
        if (line == REG_BACKUP_CCB) {
            *active_standby_table_entry =
                CCM_Active_Standby_Table.standby_ccm_entry;
        } else {
            *active_standby_table_entry =
                CCM_Active_Standby_Table.active_ccm_entry;
        }
    }
}

int
sipTransportGetCCType (int line, void *cc_table_entry)
{
    if (cc_table_entry != NULL) {
        cc_table_entry = CC_Config_Table[line - 1].cc_table_entry;
    }
    return (CC_Config_Table[line - 1].cc_type);
}

/**
 *
 * SIPTransportUDPListenForSipMessages
 *
 * Establish a UDP socket to listen to for incoming SIP messages
 *
 * Parameters:   None
 *
 * Return Value: SIP_OK or SIP_ERROR
 *
 */
int
SIPTransportUDPListenForSipMessages (void)
{
    static const char *fname = "SIPTransportUDPListenForSipMessages";
    uint32_t local_sip_control_port;
    cpr_ip_addr_t    local_sip_ip_addr;
    int            ip_mode = CPR_IP_MODE_IPV4;

    /*
     * Start listening on port 5060 for incoming SIP messages
     */
    CPR_IP_ADDR_INIT(local_sip_ip_addr);

    config_get_value(CFGID_VOIP_CONTROL_PORT, &local_sip_control_port,
                     sizeof(local_sip_control_port));

#ifdef IPV6_STACK_ENABLED
   config_get_value(CFGID_IP_ADDR_MODE, &ip_mode, sizeof(ip_mode));
#endif
    switch (ip_mode) {
    case CPR_IP_MODE_IPV4:
        local_sip_ip_addr.type = CPR_IP_ADDR_IPV4;
        local_sip_ip_addr.u.ip4 = 0;
        break;
    case CPR_IP_MODE_IPV6:
    case CPR_IP_MODE_DUAL:
        local_sip_ip_addr = ip_addr_invalid;
        local_sip_ip_addr.type = CPR_IP_ADDR_IPV6;
        break;
    default:
        break;
    }
    /* Based on mode type configure the IP address IPv4 or IPv6 */


    if (sip_platform_udp_channel_listen(ip_mode, &listen_socket, &local_sip_ip_addr,
                                        (uint16_t) local_sip_control_port)
            != SIP_OK) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sip_platform_udp_channel_listen(0, %d) "
                          "returned error.\n", fname, local_sip_control_port);
        return SIP_ERROR;
    }

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Listening for SIP messages on UDP port <%d>, handle=<%d>\n",
                     DEB_F_PREFIX_ARGS(SIP_TRANS, fname), local_sip_control_port, listen_socket);

    return SIP_OK;
}

static void
sipTransportCfgTableInit (boolean *cc_udp)
{
//    int cc_num;
    line_t line;
    CC_ID dev_cc_type = CC_OTHER;
    uint32_t transport_prot = CONN_UDP;
    ti_common_t *ti_common;
    static const char *fname = "sipTransportCfgTableInit";


    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Transport Interface init\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname));

    ti_common = &CSPS_Config_Table[0].ti_common;
    sip_config_get_proxy_addr(1, ti_common->addr_str, sizeof(ti_common->addr_str));

    if (!cpr_strcasecmp(ti_common->addr_str, "USECALLMANAGER")) {
        dev_cc_type = CC_CCM;
    }
    if (dev_cc_type == CC_CCM) {
        /*
         * Initialize the ccm table
         */
        uint32_t listen_port;
        CCM_ID ccm_id;
        ti_ccm_t *ti_ccm;

        memset(CCM_Config_Table, 0,
               (sizeof(uint32_t) * MAX_CCM * (MAX_REG_LINES + 1)));
        config_get_value(CFGID_VOIP_CONTROL_PORT, &listen_port,
                         sizeof(listen_port));
        config_get_value(CFGID_TRANSPORT_LAYER_PROT, &transport_prot,
                         sizeof(transport_prot));
        if (transport_prot != CONN_UDP) {
            *cc_udp = FALSE;
        }

        /*
         * Initialize the dummy entry and point the Active
         * and standby to it.
         */
        CCM_Dummy_Entry.cc_type = CC_CCM;
        CCM_Dummy_Entry.ti_specific.ti_ccm.ccm_id = MAX_CCM;
        CCM_Dummy_Entry.ti_common.conn_type = (CONN_TYPE) transport_prot;

        for (ccm_id = PRIMARY_CCM; ccm_id < MAX_CCM; ccm_id++) {
            uint32_t port;
            phone_local_tcp_port[ccm_id] = 0;
            ti_common = &CCM_Device_Specific_Config_Table[ccm_id].ti_common;
            ti_ccm = &CCM_Device_Specific_Config_Table[ccm_id].ti_specific.ti_ccm;

            CCM_Device_Specific_Config_Table[ccm_id].cc_type = CC_CCM;
            sip_regmgr_get_config_addr(ccm_id, ti_common->addr_str);

            config_get_value(ccm_config_id_port[ccm_id], &port, sizeof(port));
            ti_common->port = (uint16_t) port;
            ti_common->conn_type = ti_common->configured_conn_type = (CONN_TYPE) transport_prot;
            ti_common->listen_port = (uint16_t) listen_port;
            ti_common->handle = INVALID_SOCKET;
            /*
             * CCM specific config variable read
             */
            ti_ccm->ccm_id = (CCM_ID) ccm_id;
            ti_ccm->sec_level = NON_SECURE;
            ti_ccm->is_valid = 1;
            config_get_value(ccm_config_id_sec_level[ccm_id],
                             &ti_ccm->sec_level, sizeof(ti_ccm->sec_level));
            config_get_value(ccm_config_id_is_valid[ccm_id],
                             &ti_ccm->is_valid, sizeof(ti_ccm->is_valid));
            if ((ti_ccm->sec_level == NON_SECURE) &&
                (transport_prot == CONN_TLS)) {
                ti_common->conn_type = CONN_TCP;
            }
            for (line = 0; line < MAX_REG_LINES; line++) {
                CCM_Config_Table[line][ccm_id] =
                    &CCM_Device_Specific_Config_Table[ccm_id];
                if (ccm_id == PRIMARY_CCM) {
                    CC_Config_Table[line].cc_type = CC_CCM;
                    CC_Config_Table[line].cc_table_entry = (void *)
                        CCM_Config_Table[ccm_id];
                }
            }
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"For CCM%d: line %d Addr: %s Port: %d"
                                " listen Port: %d transport: %d"
                                " Sec Level: %d Is Valid: %d\n",
                                DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                ccm_id, line, ti_common->addr_str,
                                ti_common->port, ti_common->listen_port,
                                ti_common->conn_type, ti_ccm->sec_level,
                                ti_ccm->is_valid);
        }
    } else {
        ti_csps_t *ti_csps = &CSPS_Device_Specific_Config_Table;
        uint32_t bkup_pxy_port;
        uint32_t emer_pxy_port;
        uint32_t outb_pxy_port;
        uint32_t listen_port;

        sip_config_get_backup_proxy_addr(&(ti_csps->bkup_pxy_addr),
                                     ti_csps->bkup_pxy_addr_str,
                                     sizeof(ti_csps->bkup_pxy_addr_str));
        config_get_value(CFGID_PROXY_BACKUP_PORT, &bkup_pxy_port,
                         sizeof(bkup_pxy_port));
        ti_csps->bkup_pxy_port = (uint16_t) bkup_pxy_port;
        config_get_string(CFGID_PROXY_EMERGENCY, ti_csps->emer_pxy_addr_str,
                          sizeof(ti_csps->emer_pxy_addr_str));
        config_get_value(CFGID_PROXY_EMERGENCY_PORT, &emer_pxy_port,
                         sizeof(emer_pxy_port));
        ti_csps->emer_pxy_port = (uint16_t) emer_pxy_port;
        config_get_string(CFGID_OUTBOUND_PROXY, ti_csps->outb_pxy_addr_str,
                          sizeof(ti_csps->outb_pxy_addr_str));
        config_get_value(CFGID_OUTBOUND_PROXY_PORT, &outb_pxy_port,
                         sizeof(outb_pxy_port));
        ti_csps->outb_pxy_port = (uint16_t) outb_pxy_port;

        config_get_value(CFGID_VOIP_CONTROL_PORT, &listen_port,
                         sizeof(listen_port));
        for (line = 0; line < MAX_REG_LINES; line++) {
            ti_common = &CSPS_Config_Table[line].ti_common;
            CSPS_Config_Table[line].ti_specific.ti_csps = ti_csps;

            sip_config_get_proxy_addr((line_t)(line + 1), ti_common->addr_str,
                                      sizeof(ti_common->addr_str));
            ti_common->port = sip_config_get_proxy_port((line_t) (line + 1));
            ti_common->conn_type = CONN_UDP;
            ti_common->listen_port = (uint16_t) listen_port;
            ti_common->addr = ip_addr_invalid;
            ti_common->handle = INVALID_SOCKET;

            CC_Config_Table[line].cc_table_entry = (void *) NULL; // NULL for now.
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"line %d Addr: %s Port: %d and listen Port: %d\n"
                                " transport: %d\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                line, ti_common->addr_str, ti_common->port,
                                ti_common->listen_port, ti_common->conn_type);
            if (line == 0) {
                ti_csps_t *ti_csps_cfg_table;

                ti_csps_cfg_table = CSPS_Config_Table[line].ti_specific.ti_csps;
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"bkup Addr: %s and Port: %d\n",
                                    DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                    ti_csps_cfg_table->bkup_pxy_addr_str,
                                    ti_csps_cfg_table->bkup_pxy_port);
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"emer Addr: %s and Port: %d\n",
                                    DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                    ti_csps_cfg_table->emer_pxy_addr_str,
                                    ti_csps_cfg_table->emer_pxy_port);
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"outb Addr: %s and Port: %d\n",
                                    DEB_F_PREFIX_ARGS(SIP_TRANS, fname),
                                    ti_csps_cfg_table->outb_pxy_addr_str,
                                    ti_csps_cfg_table->outb_pxy_port);
            }
        }
    }
}


/*
 ** sipTransportInit
 *
 *  FILENAME: sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  Note: Make sure this gets calls during config change notifications.
 *  from softphone\src-win\global_stub.c's and src-arm-79xx\config.c's
 *  config_commit() function that calls the prot_config_change_notify().
 *  RETURNS:
 *
 */
int
sipTransportInit (void)
{
    int result = 0;
    static const char *fname = "sipTransportInit";
    boolean cc_udp = TRUE;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"Transport_interface: Init function "
                     "call !\n", DEB_F_PREFIX_ARGS(SIP_TRANS, fname));
    /*
     * Init the cc related info into the cc config table
     */
    sipTransportCfgTableInit(&cc_udp);
    /*
     * Make sure that the IP stack is up before trying to connect
     */
    if (PHNGetState() > STATE_IP_CFG) {
        if (cc_udp) {
            /*
             * By now we know the DHCP is up, so we can
             * start open sockets for listening for SIP messages and
             * the UDP channel to the SIP proxy server
             */

            if (SIPTransportUDPListenForSipMessages() == SIP_ERROR) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"device unable to"
                                  " receive SIP messages.\n", fname);
            }
        } else {
            CCSIP_DEBUG_TASK(DEB_F_PREFIX"CCM in non udp mode so not "
                             "opening separate listen socket.\n",DEB_F_PREFIX_ARGS(SIP_TRANS, fname));
        }
        if (sip_regmgr_init() != SIP_OK) {
            result = SIP_ERROR;
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"IP Stack Not "
                         "Initialized.\n", fname);
        result = -1;
    }
    return (result);
}

/*
 ** sipTransportShutdown
 *
 *  FILENAME: sip_common_transport.c
 *
 *  PARAMETERS:
 *
 *  DESCRIPTION:
 *
 *  RETURNS:
 *
 */
void
sipTransportShutdown ()
{
    CCSIP_DEBUG_STATE(DEB_F_PREFIX"Transport_interface: Shutting down!\n", DEB_F_PREFIX_ARGS(SIP_TRANS, "sipTransportShutdown"));
    sip_regmgr_destroy_cc_conns();
}

/*
 ** sipTransportClearServerHandle
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: ip addr, port, connid
 *
 *  DESCRIPTION: This function clears the server handle and port
 *
 *  RETURNS: none
 *
 */
void
sipTransportClearServerHandle (cpr_ip_addr_t *ipaddr, uint16_t port, int connid)
{

    ti_common_t *ti_common;
    CCM_ID cc_index;

    CCSIP_DEBUG_TASK(DEB_F_PREFIX"addr %p port %d connid %d",
                     DEB_F_PREFIX_ARGS(SIP_TRANS, "sipTransportClearServerHandle"), ipaddr, port, connid);
    for (cc_index = PRIMARY_CCM; cc_index < MAX_CCM; cc_index++) {
        ti_common = &CCM_Device_Specific_Config_Table[cc_index].ti_common;
        if (util_compare_ip(&(ti_common->addr),ipaddr) && ti_common->port == port) {
            sip_tcp_purge_entry(connid);
            ti_common->handle = INVALID_SOCKET;
            ti_common->listen_port = 0;
            return;
        }
    }
}

/*
 ** sipTransportSetServerHandleAndPort
 *
 *  FILENAME: ip_phone\sip\sip_common_transport.c
 *
 *  PARAMETERS: Line id
 *
 *  DESCRIPTION: This function gets the server handle for a particular
 *               line id.
 *
 *  RETURNS: The server handle
 */
void
sipTransportSetServerHandleAndPort (cpr_socket_t socket_handle,
                                    uint16_t listen_port,
                                    ti_config_table_t *ccm_table_entry)
{
    ti_common_t *ti_common;
    ti_ccm_t *ti_ccm;

    ti_ccm = &ccm_table_entry->ti_specific.ti_ccm;

    if (ti_ccm->ccm_id < MAX_CCM) {
        ti_common = &CCM_Device_Specific_Config_Table[ti_ccm->ccm_id].ti_common;
        ti_common->handle = socket_handle;
        ti_common->listen_port = listen_port;
    }
}

void
sipTransportSetSIPServer() {
	char addr_str[MAX_IPADDR_STR_LEN];
	init_empty_str(addr_str);
	config_get_string(CFGID_CCM1_ADDRESS, addr_str, MAX_IPADDR_STR_LEN);
	sstrncpy(CCM_Config_Table[0][0]->ti_common.addr_str, addr_str, MAX_IPADDR_STR_LEN);
	sstrncpy(CCM_Device_Specific_Config_Table[PRIMARY_CCM].ti_common.addr_str, addr_str, MAX_IPADDR_STR_LEN);
}
