/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debug.h"
#include "phone_debug.h"
#include "ccsip_publish.h"
#include "util_string.h"
#include "sip_interface_regmgr.h"

extern boolean dump_reg_msg;

/* SIP flags */
cc_int32_t SipDebugMessage          = 1; /* SIP messaging output */
cc_int32_t SipDebugState            = 0; /* SIP State Machine output */
cc_int32_t SipDebugTask             = 1; /* SIP Task output */
cc_int32_t SipDebugRegState         = 0; /* SIP Registration State Mach. output */

/**
 * SipRelDevEnabled flag is not a debug print flag. It actually turns
 * on and off reliable delivery module in sip layer. We will keep
 * this on as retransmissions are enabled on CUCM by default even for TCP transport
 */
int32_t SipRelDevEnabled           = 1; /* Is reliable delivery on? */

int32_t SipDebugGenContactHeader = 0; /* Generate Contact header */
cc_int32_t SipDebugTrx              = 0; /* SIP Transaction Layer output */

cc_int32_t GSMDebug                 = 0;
cc_int32_t FIMDebug                 = 0;
cc_int32_t LSMDebug                 = 0;
cc_int32_t FSMDebugSM               = 0;
int32_t CSMDebugSM               = 0;
cc_int32_t CCDebug                  = 0;
cc_int32_t CCDebugMsg               = 0;
cc_int32_t AuthDebug                = 0;
cc_int32_t g_DEFDebug               = 1;
cc_int32_t TNPDebug                 = 1;
cc_int32_t VCMDebug                 = 0;
cc_int32_t CCEVENTDebug             = 0;
cc_int32_t PLATDebug                = 0;
cc_int32_t TMRDebug                 = 0;
cc_int32_t g_dcsmDebug              = 0;

/**
 *
 * Dump sip messages
 *
 * @param msg - message buffer
 *        pSIPMessage - parsed message structure
 *        cc_remote_ipaddr - remote ip address
 *        cc_remote_port - remote port number
 *
 * @return  none
 *
 * @pre     (valid pSIPMessage AND
 *           valid cc_remote_ipaddr)
 */

void ccsip_dump_send_msg_info (char *msg, sipMessage_t *pSIPMessage,
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port)
{
    char *disp_buf;
    const char *req_uri;
    const char *cseq;
    const char *callid;
    char ipaddr_str[MAX_IPADDR_STR_LEN];
    static const char fname[] = "ccsip_dump_send_msg_info";

    ipaddr2dotted(ipaddr_str, cc_remote_ipaddr);

    req_uri = sippmh_get_header_val(pSIPMessage, SIP_HEADER_TO, NULL);
    if (req_uri == NULL) {
        /* No REQ URI, fill with blank */
        req_uri = "";
    }
    cseq = sippmh_get_header_val(pSIPMessage, SIP_HEADER_CSEQ, NULL);
    if (cseq == NULL) {
        /* No REQ CSEQ, fill with blank */
        cseq = "";
    }
    callid = sippmh_get_header_val(pSIPMessage, SIP_HEADER_CALLID, NULL);
    if (callid == NULL) {
        /* No REQ CSEQ, fill with blank */
        callid = "";
    }

    /* For messages starting with SIP add 8 byte. default
     * debugs do not show all the SIP message information
     * rather show initial msg.
     */
    if (msg != NULL) {
        if (msg[0] == 'S' &&
            msg[1] == 'I' &&
            msg[2] == 'P') {
            disp_buf = &msg[8];
        } else {
            disp_buf = msg;
        }
        if ((strncmp(disp_buf, SIP_METHOD_REGISTER, sizeof(SIP_METHOD_REGISTER)-1) == 0) &&
            (!dump_reg_msg)) {
            return;
        }
    } else {
        /* No msg. buffer */
        disp_buf = NULL;
    }


    if (disp_buf != NULL) {
        DEF_DEBUG(DEB_F_PREFIX"<%s:%-4d>:%c%c%c%c%c%c%c: %-10s :%-6s::%s",
                    DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname),
                    ipaddr_str, cc_remote_port,
                    disp_buf[0],
                    disp_buf[1],
                    disp_buf[2],
                    disp_buf[3],
                    disp_buf[4],
                    disp_buf[5],
                    disp_buf[6],
                    req_uri,
                    cseq, callid);
    } else {
        /* No msg to send */
        DEF_DEBUG(DEB_F_PREFIX"<%s:%-4d>: empty message",
                  DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname),
                  ipaddr_str, cc_remote_port);
    }
}

/**
 *
 * Dump sip messages
 *
 * @param msg - message buffer
 *        pSIPMessage - parsed message structure
 *        cc_remote_ipaddr - remote ip address
 *        cc_remote_port - remote port number
 *
 * @return  none
 *
 * @pre     (valid pSIPMessage AND
 *           valid cc_remote_ipaddr)
 */

void ccsip_dump_recv_msg_info (sipMessage_t *pSIPMessage,
                               cpr_ip_addr_t *cc_remote_ipaddr,
                               uint16_t cc_remote_port)
{
    char *disp_buf;
    const char *req_uri;
    const char *cseq;
    const char *callid;
    char ipaddr_str[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t cc_ipaddr;
    static const char fname[] = "ccsip_dump_recv_msg_info";

    util_ntohl(&cc_ipaddr, cc_remote_ipaddr);
    ipaddr2dotted(ipaddr_str, &cc_ipaddr);

    req_uri = sippmh_get_cached_header_val(pSIPMessage, FROM);
    if (req_uri == NULL) {
        /* No REQ URI, fill with blank */
        req_uri = "";
    }
    cseq = sippmh_get_cached_header_val(pSIPMessage, CSEQ);
    if (cseq == NULL) {
        /* No REQ CSEQ, fill with blank */
        cseq = "";
    }
    callid = sippmh_get_cached_header_val(pSIPMessage, CALLID);
    if (callid == NULL) {
        /* No REQ CSEQ, fill with blank */
        callid = "";
    }

    if (!dump_reg_msg) {
       if (strstr(cseq, SIP_METHOD_REGISTER) != NULL) {
          return;
       }
    }

    /* For messages starting with SIP add 8 byte. default
     * debugs do not show all the SIP message information
     * rather show initial msg.
     */
    if (pSIPMessage->mesg_line != NULL) {
        if (pSIPMessage->mesg_line[0] == 'S' &&
            pSIPMessage->mesg_line[1] == 'I' &&
            pSIPMessage->mesg_line[2] == 'P') {
            disp_buf = &(pSIPMessage->mesg_line[8]);
        } else {
            disp_buf = pSIPMessage->mesg_line;
        }
    } else {
        /*
         * It is possible that this function is called with
         * invalid message or partially received.
         */
        disp_buf = NULL;
    }

    if (disp_buf != NULL) {
        DEF_DEBUG(DEB_F_PREFIX"<%s:%-4d>:%c%c%c%c%c%c%c: %-10s :%-6s::%s",
                    DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname),
                    ipaddr_str, cc_remote_port,
                    disp_buf[0],
                    disp_buf[1],
                    disp_buf[2],
                    disp_buf[3],
                    disp_buf[4],
                    disp_buf[5],
                    disp_buf[6],
                    req_uri,
                    cseq, callid);
    } else {
        /* No line received */
        DEF_DEBUG(DEB_F_PREFIX"<%s:%-4d>: empty message",
                  DEB_F_PREFIX_ARGS(SIP_MSG_RECV, fname),
                  ipaddr_str, cc_remote_port);
    }
}

/**
 *
 * platform_print_sip_msg
 *
 * The function perform printing SIP msg.
 *
 * Parameters:   msg - pointer to character strings of the SIP messages.
 *
 * Return Value: None
 *
 */
void
platform_print_sip_msg (const char *msg)
{
    char *buf;
    int msg_to_crypto_line_len, msg_to_digits_tag_len, buf_len;
    const char *c_line_begin, *c_line_end;
    static const char crypto_line_tag[] = "a=crypto:";
    static const char crypto_mask[] = "...";
    static const char digits_tag[] = "digits=";

    if (msg == NULL) {
        return;
    }

    /* replace digits for security reasons */
    if (strstr(msg, "kpml-response")) {
        /* This is kpml response. so supress printing digits. */
        c_line_begin = strstr(msg, digits_tag);
        if (c_line_begin == NULL) {
            /* No digits, print everything */
            CSFLogDebug("sipstack", "%s", msg);
            return;
        }
        /*
         * Calculate the length of the msg. to print from
         * from the beginning to the end of the "digits=" i.e.
         */
        msg_to_digits_tag_len = c_line_begin - msg + sizeof(digits_tag);
        buf_len = msg_to_digits_tag_len + sizeof(crypto_mask);
        buf = (char *) cpr_malloc(buf_len);
        if (buf == NULL) {
            /* No memory */
            return;
        }

        /* Copy the message upto "digits=" */
        memcpy(buf, msg, msg_to_digits_tag_len);

        /* Copy mask and the end NULL terminating char to the buffer */
        memcpy(&buf[msg_to_digits_tag_len], crypto_mask, sizeof(crypto_mask));
        /* Print it out now */
        CSFLogDebug("sipstack", "%s", buf);
        cpr_free(buf);

        c_line_end = c_line_begin + sizeof(digits_tag) + 3; /* 3 beacuse " + digit + " */
        msg = c_line_end;
        CSFLogDebug("sipstack", "%s", msg);
    } else if (sip_regmgr_get_sec_level(1) == ENCRYPTED) {
        /* The 1st. line is using encrypted signaling */
        while (TRUE) {
            c_line_begin = strstr(msg, crypto_line_tag);
            if (c_line_begin == NULL) {
                /* No more crypto line, print whatever left and done */
                CSFLogDebug("sipstack", "%s", msg);
                return;
            } else {
                /*
                 * Found a crypto line:
                 *
                 * Allocate buffer to print the msg. including this
                 * instance of crypto line but no crypto parameter.
                 * For example:
                 *
                 * v=0
                 * o=Cisco-SIPUA 3052 7 IN IP4 10.80.35.217
                 * s=SIP Call
                 * t=0 0
                 * m=audio 28926 RTP/SAVP 0 8 18 101
                 * c=IN IP4 10.80.35.217
                 * a=crypto:1 AES_CM_128_HMAC_SHA1_32 inline:GqT...
                 * a=rtpmap:0 PCMU/8000
                 *
                 *
                 * The output of printing will be
                 *
                 * v=0
                 * o=Cisco-SIPUA 3052 7 IN IP4 10.80.35.217
                 * s=SIP Call
                 * t=0 0
                 * m=audio 28926 RTP/SAVP 0 8 18 101
                 * c=IN IP4 10.80.35.217
                 * a=crypto:1...
                 * a=rtpmap:0 PCMU/8000
                 *
                 */

                /*
                 * Calculate the length of the msg. to print from
                 * current position to the end of the "a=crypto:n" i.e.
                 * from the current of msg passes the ":" by one character
                 * after of the "a=crypto:". This allows the first character
                 * of the crypto tag field character to be included in the
                 * output. Note that there is no explicit adding this
                 * extra character in the code below because of the
                 * sizeof(crypto_line_tag) includes extra 1 byte of string
                 * termination character already. It will be the extra
                 * character to print after the ":".
                 */
                msg_to_crypto_line_len = c_line_begin - msg +
                                         sizeof(crypto_line_tag);
                /*
                 * The buffer allocated to print includes the length from
                 * current msg pointer to this instance of crypto line +
                 * the mask length plus 1 byte for NULL terminating character.
                 * The NULL terminating character is included in the
                 * sizeof(crypto_mask) below.
                 */
                buf_len = msg_to_crypto_line_len + sizeof(crypto_mask);
                buf = (char *) cpr_malloc(buf_len);
                if (buf == NULL) {
                    /* No memory */
                    return;
                }

                /* Copy the message including this instance of crypto line */
                memcpy(buf, msg, msg_to_crypto_line_len);

                /* Copy mask and the end NULL terminating char to the buffer */
                memcpy(&buf[msg_to_crypto_line_len], crypto_mask,
                       sizeof(crypto_mask));
                /* Print it out now */
                CSFLogDebug("sipstack", "%s", buf);
                cpr_free(buf);

                /* Find the end of the crypto line */
                c_line_end = strpbrk(c_line_begin, "\n");
                if (c_line_end != NULL) {
                    /* Skip pass the "\n" character of the crypto line */
                    msg = c_line_end + 1;
                } else {
                    /* Some thing is wrong, no end line character in SDP */
                    return;
                }
            }
        }
    } else {
        /* print full content */
        CSFLogDebug("sipstack", "%s", msg);
    }
}

/*
 *  Function: ccsip_debug_init()
 *
 *  Parameters: None
 *
 *  Description: Initialize the Debug keywords used by the SIP protocol stack.
 *
 *  Returns: None
 *
 */
void
ccsip_debug_init (void)
{
    /* Placeholder for doing any initialization related tasks */
}
