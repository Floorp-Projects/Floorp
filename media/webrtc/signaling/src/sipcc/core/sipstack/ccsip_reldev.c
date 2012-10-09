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
#include "cpr_string.h"
#include "ccsip_reldev.h"
#include "ccsip_macros.h"
#include "phone_debug.h"
#include "ccsip_pmh.h"
#include "ccsip_messaging.h"
#include "sip_common_transport.h"
#include "util_string.h"

/* Constants */
#define SIP_RRLIST_LENGTH (MAX_TEL_LINES)

/* Global variables */
sipRelDevMessageRecord_t gSIPRRList[SIP_RRLIST_LENGTH];


void
sipRelDevMessageStore (sipRelDevMessageRecord_t * pMessageRecord)
{
    static int counter = 0;

    /* Loop around */
    if (counter > (SIP_RRLIST_LENGTH - 1)) {
        counter = 0;
    }

    gSIPRRList[counter] = *pMessageRecord;
    gSIPRRList[counter].valid_coupled_message = FALSE;
    counter++;
}


boolean
sipRelDevMessageIsDuplicate (sipRelDevMessageRecord_t *pMessageRecord,
                             int *idx)
{
    int i = 0;

    for (i = 0; i < SIP_RRLIST_LENGTH; i++) {
        if ((strcmp(pMessageRecord->call_id, gSIPRRList[i].call_id) == 0) &&
            (pMessageRecord->cseq_number == gSIPRRList[i].cseq_number) &&
            (pMessageRecord->cseq_method == gSIPRRList[i].cseq_method) &&
            (strcasecmp_ignorewhitespace(pMessageRecord->tag, gSIPRRList[i].tag) == 0) &&
            (strcmp(pMessageRecord->from_user, gSIPRRList[i].from_user) == 0) &&
            (strcmp(pMessageRecord->from_host, gSIPRRList[i].from_host) == 0) &&
            (strcmp(pMessageRecord->to_user, gSIPRRList[i].to_user) == 0)) {

            if (pMessageRecord->is_request) {
                *idx = i;
                return (TRUE);
            } else {
                if (pMessageRecord->response_code == gSIPRRList[i].response_code) {
                    *idx = i;
                    return (TRUE);
                }
            }
        }
    }

    *idx = -1;
    return (FALSE);
}


/*
 *  Function: sipRelDevCoupledMessageStore
 *
 *  Parameters:
 *      pCoupledMessage - pointer to sipMessage_t for the message that
 *                        needed reliable delivery store.
 *      call_id         - pointer to const. char for the SIP call ID.
 *      cseq_number     - the uint32_t for CSeq of the message.
 *      sipMethod_t     - sipMethod_t the SIP method of the message.
 *      dest_ipaddr     - uint32_t IP address of the destination address.
 *      dest_port       - uint16_t destination port
 *      ignore_tag      - boolean to ignore tag.
 *
 *  Description:
 *      The function finds the corresponding entry in the gSIPRRList
 *  array that matches call_id, cseq number, method and possibly tag.
 *  If a match entry is found, the SIP message is composed and stored
 *  in the corresponding entry.
 *
 *  Returns:
 *      idx - When the entry is found and successfully stored the
 *              SIP message.
 *      RELDEV_NO_STORED_MSG - encounter errors or no message is
 *                             stored.
 */
int
sipRelDevCoupledMessageStore (sipMessage_t *pCoupledMessage,
                              const char *call_id,
                              uint32_t cseq_number,
                              sipMethod_t cseq_method,
                              boolean is_request,
                              int response_code,
                              cpr_ip_addr_t *dest_ipaddr,
                              uint16_t dest_port,
                              boolean ignore_tag)
{
    static const char *fname = "sipRelDevCoupledMessageStore";
    int i = 0;
    char to_tag[MAX_SIP_TAG_LENGTH];

    sipGetMessageToTag(pCoupledMessage, to_tag, MAX_SIP_TAG_LENGTH);
    CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Storing for reTx (cseq=%d, method=%s, "
                        "to_tag=<%s>)\n", DEB_F_PREFIX_ARGS(SIP_STORE, fname), cseq_number,
                        sipGetMethodString(cseq_method), to_tag);

    for (i = 0; i < SIP_RRLIST_LENGTH; i++) {
        if ((strcmp(call_id, gSIPRRList[i].call_id) == 0) &&
            (cseq_number == gSIPRRList[i].cseq_number) &&
            (cseq_method == gSIPRRList[i].cseq_method) &&
            ((ignore_tag) ? TRUE : (strcasecmp_ignorewhitespace(to_tag,
                                                     gSIPRRList[i].tag)
                                    == 0))) {
            hStatus_t sippmh_write_status = STATUS_FAILURE;
            uint32_t nbytes = SIP_UDP_MESSAGE_SIZE;

            /*
              When storing the ACK message check that you are storing it
              coupled with the correct response code and not transitional responses
            */
            if (is_request == FALSE ||
                (is_request == TRUE && gSIPRRList[i].response_code == response_code)) {
                gSIPRRList[i].coupled_message.message_buf[0] = '\0';
                sippmh_write_status =
                    sippmh_write(pCoupledMessage,
                                 gSIPRRList[i].coupled_message.message_buf,
                                 &nbytes);
                if (sippmh_write_status == STATUS_FAILURE) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_write() failed.\n", fname);
                    return (RELDEV_NO_STORED_MSG);
                }
                if ((gSIPRRList[i].coupled_message.message_buf[0] == '\0') ||
                    (nbytes == 0)) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sippmh_write() returned empty buffer string.\n",
                                      fname);
                    return (RELDEV_NO_STORED_MSG);
                }
                gSIPRRList[i].coupled_message.message_buf_len = nbytes;
                gSIPRRList[i].coupled_message.dest_ipaddr = *dest_ipaddr;
                gSIPRRList[i].coupled_message.dest_port = dest_port;
                gSIPRRList[i].valid_coupled_message = TRUE;
                /* Return the stored idx to the caller */
                return (i);
            }
        }   
    }
    return (RELDEV_NO_STORED_MSG);
}

/*
 *  Function: sipRelDevGetStoredCoupledMessage
 *
 *  Parameters:
 *      idx         - int for the idx to retrieve stored SIP message.
 *      dest_buffer   - pointer to char to retrieve the copy of the
 *                      stored SIP message.
 *      dest_buf_size - uint32_t for the size of the dest_buffer.
 *
 *  Description:
 *      The function gets the stored SIP message for the request entry.
 *
 *  Returns:
 *      The function returns the number of the bytes of the SIP message
 *  obtained if SIP message is found otherwise it returns zero (
 *  no SIP message available).
 */
uint32_t
sipRelDevGetStoredCoupledMessage (int idx,
                                  char *dest_buffer,
                                  uint32_t dest_buf_size)
{
    sipRelDevMessageRecord_t *record;

    if (dest_buffer == NULL) {
        /* No destination buffer given, can not provide any result */
        return (0);
    }
    if ((idx < 0) || (idx >= SIP_RRLIST_LENGTH)) {
        /* the stored message is out of range */
        return (0);
    }

    record = &gSIPRRList[idx];

    if (!record->valid_coupled_message) {
        /* No message stored for the given idx */
        return (0);
    }
    if ((record->coupled_message.message_buf_len > dest_buf_size) ||
        (record->coupled_message.message_buf_len == 0)) {
        /*
         * The stored message size is larger than the give buffer or
         * stored message length is zero?
         */
        return (0);
    }
    /* Get the stored message to the caller */
    memcpy(dest_buffer, &record->coupled_message.message_buf[0],
           record->coupled_message.message_buf_len);
    return (record->coupled_message.message_buf_len);
}

int
sipRelDevCoupledMessageSend (int idx)
{
    static const char *fname = "sipRelDevCoupledMessageSend";
    char dest_ipaddr_str[MAX_IPADDR_STR_LEN];

    if ((idx < 0) || (idx >= SIP_RRLIST_LENGTH)) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Argument Check: idx (=%d) out of bounds.\n",
                          fname, idx);
        return SIP_ERROR;
    }

    if (gSIPRRList[idx].valid_coupled_message) {
        ipaddr2dotted(dest_ipaddr_str,
                      &gSIPRRList[idx].coupled_message.dest_ipaddr);

        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Sending stored coupled message (idx=%d) to "
                            "<%s>:<%d>\n", DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname), idx, dest_ipaddr_str,
                            gSIPRRList[idx].coupled_message.dest_port);
        if (sipTransportChannelSend(NULL,
                       gSIPRRList[idx].coupled_message.message_buf,
                       gSIPRRList[idx].coupled_message.message_buf_len,
                       sipMethodInvalid,
                       &(gSIPRRList[idx].coupled_message.dest_ipaddr),
                       gSIPRRList[idx].coupled_message.dest_port,
                       0) < 0) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX"sipTransportChannelSend() failed."
                              " Stored message not sent.\n", fname);
            return SIP_ERROR;
        }
    } else {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Duplicate message detected but failed to"
                          " find valid coupled message. Stored message not sent.\n",
                          fname);
        return SIP_ERROR;
    }
    return SIP_OK;
}


/*
 * Function for clearing all the messages belonging to the
 * associated with the specified call_id, from_user, from_host,
 * and to_user
 */
void
sipRelDevMessagesClear (const char *call_id,
                        const char *from_user,
                        const char *from_host,
                        const char *to_user)
{
    int i = 0;

    for (i = 0; i < SIP_RRLIST_LENGTH; i++) {
        if ((strcmp(call_id, gSIPRRList[i].call_id) == 0) &&
            (strcmp(from_user, gSIPRRList[i].from_user) == 0) &&
            (strcmp(from_host, gSIPRRList[i].from_host) == 0) &&
            (strcmp(to_user, gSIPRRList[i].to_user) == 0)) {
            memset(&gSIPRRList[i], 0, sizeof(sipRelDevMessageRecord_t));
        }
    }
    return;
}

/*
 * Function for clearing all the messages
 */
void sipRelDevAllMessagesClear(){
	int i = 0;
	for (i = 0; i < SIP_RRLIST_LENGTH; i++) {
		memset(&gSIPRRList[i], 0, sizeof(sipRelDevMessageRecord_t));
	}
	return;
}
