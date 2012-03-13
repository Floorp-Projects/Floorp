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

#ifndef _CCSIP_RELDEV_H_
#define _CCSIP_RELDEV_H_

#include "cpr_types.h"
#include "ccsip_pmh.h"
#include "ccsip_core.h"

#define RELDEV_MAX_USER_NAME_LEN    64
#define RELDEV_MAX_HOST_NAME_LEN    64
#define RELDEV_NO_STORED_MSG        (-1)

typedef struct
{
    char     message_buf[SIP_UDP_MESSAGE_SIZE];
    uint32_t message_buf_len;
    cpr_ip_addr_t dest_ipaddr;
    uint16_t dest_port;
} sipRelDevCoupledMessage_t;

typedef struct
{
    boolean                   is_request;
    char                      call_id[MAX_SIP_CALL_ID];
    uint32_t                  cseq_number;
    sipMethod_t               cseq_method;
    char                      tag[MAX_SIP_TAG_LENGTH];
    char                      from_user[RELDEV_MAX_USER_NAME_LEN];
    char                      from_host[RELDEV_MAX_HOST_NAME_LEN];
    char                      to_user[RELDEV_MAX_USER_NAME_LEN];
    int                       response_code;
    sipRelDevCoupledMessage_t coupled_message;
    boolean                   valid_coupled_message;
    //int                       line;
} sipRelDevMessageRecord_t;

void sipRelDevMessageStore(sipRelDevMessageRecord_t *pMessageRecord);
boolean sipRelDevMessageIsDuplicate(sipRelDevMessageRecord_t *pMessageRecord,
                                    int *index);
int sipRelDevCoupledMessageStore(sipMessage_t *pCoupledMessage,
                                 const char *call_id,
                                 uint32_t cseq_number,
                                 sipMethod_t cseq_method,
                                 boolean is_request,
                                 int status_code,
                                 cpr_ip_addr_t *dest_ipaddr,
                                 uint16_t dest_port,
                                 boolean ignore_tag);
int sipRelDevCoupledMessageSend(int index);
void sipRelDevMessagesClear(const char *call_id,
                            const char *from_user,
                            const char *from_host,
                            const char *to_user);
void sipRelDevAllMessagesClear();
uint32_t sipRelDevGetStoredCoupledMessage(int index,
                                          char *dest_buffer,
                                          uint32_t max_buff);


#endif
