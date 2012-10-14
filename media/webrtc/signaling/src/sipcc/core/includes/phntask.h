/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PHNTASK_H
#define PHNTASK_H

/* used for certain bugtraps */
#define  ERRid       0xE0
#define  ERR_1       0xE1
#define  ERR_2       0XE2
#define  ERR_3       0XE3
#define  ERR_4       0XE4
#define  ERR_5       0XE5

/*
 * SysHdr Network Command definitions (0x00 - 0x0F)
 */
#define  RCV_CMD       0x00
#define  TRAP_CMD      0x01
#define  OPEN_CMD      0x02
#define  CLOSE_CMD     0x03
#define  HANG_CMD      0x04
#define  TO_CMD        0x05
#define  ISR_CMD       0x06
#define  ACK_CMD       0x07
#define  NACK_CMD      0x08
#define  PING_CMD      0x09
#define  JOIN_CMD      0x0A
#define  LEAVE_CMD     0x0B
#define  TO2_CMD       0x0C
#define  TO3_CMD       0x0D

/*---------------------------------------------
 * Message Definition
 *
 * original requirement is to get a unique id I guess
 * for TNP we just get some unique numbers through enum
 */

enum {
    /* CPR Messages */
    TIMER_EXPIRATION,

    /* DHCP Messages */
    DHCP_UDP,
    DHCP_TMR,
    DHCP_TFTP,
    DHCP_DHCP,
    DHCP_TO,
    DHCP_RESET_IP,
    DHCP_DNS,
    DHCP_CFG,
    DHCP_CDP,
    DHCP_RESTART,
    DHCP_SOC,
    DHCP_UI,

    /* SNTP Messages */
    SNTP_UDP,
    SNTP_REQ,

    /* TFTP Messages */
    TFTP_PHN,
    TFTP_CFG,
    TFTP_DHCP,
    TFTP_UDP,
    TFTP_TO,

    /* UDP Messages */
    UDP_TFTP,
    UDP_DHCP,
    UDP_DNS,
    UDP_RTP,
    UDP_SNTP,

    /* TCP Messages */
    TCP_PHN,
    TCP_PHN_OPEN,
    TCP_PHN_CLOSE,
    TCP_CFG,
    TCP_CFG_OPEN,
    TCP_CFG_CLOSE,
    TCP_TO,
    TCP_SOC,

    /* HTTP Messages */
    HTTP_RCV,

    /* Socket Messages */
    ICMP_DHCP,
    ICMP_ARP,
    ICMP_RTP,
    SOC_ICMP,
    SOC_IGMP,
    SOC_ROUTE,
    SOC_FRAG,
    IGMP_PHN_JOIN,
    IGMP_PHN_LEAVE,
    IP_PHN,
    SOC_ISR,
    NET_CDP,

    /* Phone Messages */
    PHN_TCP,
    PHN_CFG,
    PHN_CFG_UI,
    PHN_TFTP,
    PHN_TICK_TO,
    PHN_DSP,
    PHN_REG,

    PHN_DIS,

    /* DNS Messages */
    DNS_DHCP,
    DNS_CFG,
    DNS_CFG_URL,
    DNS_UDP,
    DNS_TO,


    CFG_DNS,
    CFG_TFTP,
    CFG_TMR,
    CFG_CDP_TMR,
    CFG_UI,
    CFG_NET_CFG,
    CFG_FMW_CFG,
    CFG_STA_CFG,
    CFG_KAZOO_CFG,
    CFG_ETH_MEDIA,
    TCP_PHN_CFG_TCP_DONE,

    NET_DNS,
    CDP_C3PO,
    CDP_TO_WAIT,
    CDP_ETH,
    CDP_TO_SYNC,
    CDP_SEND,
    CDP_CACHE_TO,
    CDP_NET,
    CDP_TRIG,
    C3PO_ETH_MODE,

    /* RTP Messages */
    RTP_ISR,
    RTP_UDP,

    /* TTY Messages */
    TTY_TTY,

    /* SIP Task Messages */
    SIP_UDP,
    SIP_GSM,
    SIP_REG_REQ,
    SIP_REG_CANCEL,
    SIP_REG_PHONE_IDLE,
    SIP_REG_FALLBACK,
    SIP_TMR_REG_ACK,
    SIP_TMR_REG_EXPIRE,
    SIP_TMR_REG_WAIT,
    SIP_TMR_REG_RETRY,
    SIP_TMR_REG_STABLE,
    SIP_TMR_INV_LOCALEXPIRE,
    SIP_TMR_INV_EXPIRE,
    SIP_TMR_MSG_RETRY,
    SIP_REG_CLEANUP,
    CC_EVENT,
    SIP_ICMP_UNREACHABLE,
    SIP_TMR_SUPERVISION_DISCONNECT,
    SIP_TMR_CALL_DISCONNECT,
    SIP_TMR_MSG_RETRY_SUBNOT,
    SIP_TMR_PERIODIC_SUBNOT,
    SIP_TMR_GLARE_AVOIDANCE,
    SIPSPI_EV_CC_SUBSCRIBE_REGISTER,
    SIPSPI_EV_CC_SUBSCRIBE,
    SIPSPI_EV_CC_SUBSCRIBE_RESPONSE,
    SIPSPI_EV_CC_NOTIFY,
    SIPSPI_EV_CC_NOTIFY_RESPONSE,
    SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED,
    SIPSPI_EV_CC_PUBLISH_REQ,
    REG_MGR_STATE_CHANGE,
    SUB_MSG_KPML_TERMINATE,
    SUB_MSG_KPML_SUBSCRIBE,
    SUB_MSG_KPML_NOTIFY_ACK,
    SUB_MSG_KPML_SUBSCRIBE_TIMER,
    SUB_MSG_KPML_DIGIT_TIMER,
    DP_MSG_INIT_DIALING,
    DP_MSG_DIGIT_TIMER,
    DP_MSG_DIGIT_STR,
    DP_MSG_STORE_DIGIT,
    DP_MSG_DIGIT,
    DP_MSG_DIAL_IMMEDIATE,
    DP_MSG_REDIAL,
    DP_MSG_ONHOOK,
    DP_MSG_OFFHOOK,
    DP_MSG_CANCEL_OFFHOOK_TIMER,
    DP_MSG_UPDATE,
    SIP_TMR_STANDBY_KEEPALIVE,
    SUB_MSG_PRESENCE_GET_STATE,
    SUB_MSG_PRESENCE_TERM_REQ,
    SUB_MSG_PRESENCE_TERM_REQ_ALL,
    SUB_MSG_PRESENCE_SUBSCRIBE_RESP,
    SUB_MSG_PRESENCE_NOTIFY,
    SUB_MSG_PRESENCE_UNSOLICITED_NOTIFY,
    SUB_MSG_PRESENCE_TERMINATE,
    SUB_HANDLER_INITIALIZED,
    SIP_TMR_DM_SHR_WAIT_DM_UPD_EVENT,
    SIP_SHUTDOWN,
    SIP_TMR_SHUTDOWN_PHASE2,
    SIP_RESTART,
    SUB_MSG_CONFIGAPP_SUBSCRIBE,
    SUB_MSG_CONFIGAPP_TERMINATE,
    SUB_MSG_CONFIGAPP_NOTIFY,
    SUB_MSG_CONFIGAPP_NOTIFY_ACK,
    SIP_REG_UPDATE,
    SUB_MSG_FEATURE_SUBSCRIBE_RESP,
    SUB_MSG_FEATURE_NOTIFY,
    SUB_MSG_FEATURE_TERMINATE,
    SUB_MSG_B2BCNF_SUBSCRIBE_RESP,
    SUB_MSG_B2BCNF_NOTIFY,
    SUB_MSG_B2BCNF_TERMINATE,
    THREAD_UNLOAD,
    DCSM_EV_READY,

    /* GSM Messages */
    GSM_RM,
    GSM_FM,
    GSM_GSM,
    GSM_SIP,
    FM_GSM
};

#define REG_CMD_PRINT(arg) \
        (arg == SIP_REG_REQ ?  "SIP_REG_REQ" : \
        arg == SIP_REG_CANCEL ?  "SIP_REG_CANCEL" : \
        arg == SIP_REG_PHONE_IDLE ?  "SIP_REG_PHONE_IDLE" : \
        arg == SIP_REG_FALLBACK ?  "SIP_REG_FALLBACK" : \
        arg == SIP_TMR_REG_ACK ?  "SIP_TMR_REG_ACK" : \
        arg == SIP_TMR_REG_EXPIRE ?  "SIP_TMR_REG_EXPIRE" : \
        arg == SIP_TMR_REG_WAIT ?  "SIP_TMR_REG_WAIT" : \
        arg == SIP_TMR_REG_RETRY ?  "SIP_TMR_REG_RETRY" : \
        arg == SIP_TMR_REG_STABLE ?  "SIP_TMR_REG_STABLE" : \
        arg == SIP_REG_CLEANUP ?  "SIP_REG_CLEANUP" : "")\



#define TCP_USR_OPEN   TCP_PHN_OPEN
#define TCP_USR_CLOSE  TCP_PHN_CLOSE
#define TCP_USR        TCP_PHN
#define IGMP_USR_JOIN  IGMP_PHN_JOIN
#define IGMP_USR_LEAVE IGMP_PHN_LEAVE

/*
 * END INTER-TASK COMMANDS
 *
 */



/*
 * This is used by the config task to allow it to
 * accept messages sent to it by the phone task.
 */
#define TFTP_USR       TFTP_PHN


/*
 * Stack Definition
 * Stacks need to be 32bit aligned.
 */

#define  STKSZ      61440       //default stacksize rountable platform


#if defined SIP_OS_LINUX

#define NICE_STEP -4
#define TIMER_THREAD_RELATIVE_PRIORITY  4*(NICE_STEP) /* -16 */
/* redid priorities to adjust relative priority with EDT cannot
     use NICE_STEP so using absolute numbers here */
#define GSM_THREAD_RELATIVE_PRIORITY    -14
#define SIP_THREAD_RELATIVE_PRIORITY    -14
#define APP_THREAD_RELATIVE_PRIORITY    -14
#define CCPROVIDER_THREAD_RELATIVE_PRIORITY -14

#elif defined SIP_OS_OSX

#define NICE_STEP -4
#define TIMER_THREAD_RELATIVE_PRIORITY  4*(NICE_STEP) /* -16 */
#define GSM_THREAD_RELATIVE_PRIORITY    3*(NICE_STEP) /* -12 */
#define SIP_THREAD_RELATIVE_PRIORITY    3*(NICE_STEP) /* -12 */
#define APP_THREAD_RELATIVE_PRIORITY    3*(NICE_STEP) /* -12 */
#define CCPROVIDER_THREAD_RELATIVE_PRIORITY 3*(NICE_STEP) /*-12 */

#elif defined SIP_OS_WINDOWS

#define TIMER_THREAD_RELATIVE_PRIORITY  0
#define GSM_THREAD_RELATIVE_PRIORITY    -1
#define SIP_THREAD_RELATIVE_PRIORITY    -1
#define APP_THREAD_RELATIVE_PRIORITY    -1
#define CCPROVIDER_THREAD_RELATIVE_PRIORITY    -1

#endif

extern int platThreadInit(char *threadName);

#endif /* PHNTASK_H */
