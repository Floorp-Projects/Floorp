/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
