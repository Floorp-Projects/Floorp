/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ccapp_task.h"
#include "phone.h"
#include "CCProvider.h"
#include "platform_api.h"

#include <prcvar.h>
#include <prlock.h>

extern cprMsgQueue_t ccapp_msgq;
extern void CCAppInit();
static sll_lite_list_t sll_list;

PRCondVar *ccAppReadyToStartCond = NULL;
PRLock *ccAppReadyToStartLock = NULL;
char ccAppReadyToStart = 0;

/**
 * Add/Get ccapp task listener
 */
void addCcappListener(appListener* listener, int type) {

   listener_t *alistener = NULL;

   CCAPP_DEBUG(DEB_F_PREFIX"Entered: listenr=%p, type=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"),
           listener, type);

   if (listener == NULL)
   {
       CCAPP_ERROR(DEB_F_PREFIX"listener is NULL, returning", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"));
       return;
   }

   alistener = cpr_malloc(sizeof(listener_t));
   if (alistener == NULL) {
       CCAPP_ERROR(DEB_F_PREFIX"alistener is NULL, returning", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"));
       return;
   }

   alistener->type = type;
   alistener->listener_p = listener;

   sll_lite_link_tail(&sll_list, (sll_lite_node_t *)alistener);
   CCAPP_DEBUG(DEB_F_PREFIX"Added: listenr=%p, type=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"),
           alistener->listener_p, alistener->type);
}

appListener *getCcappListener(int type) {
    static const char fname[] ="getCcappListener";
    listener_t *temp_info;
    sll_lite_node_t *iterator;

    CCAPP_DEBUG(DEB_F_PREFIX"entered: for app[%d]", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
            type);

    iterator = sll_list.head_p;
    while (iterator) {
        temp_info = (listener_t *)iterator;
        CCAPP_DEBUG(DEB_F_PREFIX"appid=%d, listener=%p",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), temp_info->type, temp_info->listener_p);
        if (temp_info->type == type) {
            {
                return temp_info->listener_p;
            }
        }
        iterator = iterator->next_p;
    }
    return NULL;
}

/**
 *
 * CC Provider wrapper for posting msg to CCAPP
 *
 * @param   msgId - message ID
 * @param   data - ptr to data
 * @param   len - len of data
 *
 * @return  CPR_SUCCESS/CPR_FAILURE
 *
 * @pre     None
 */

cpr_status_e ccappTaskPostMsg(unsigned int msgId, void * data, uint16_t len, int appId)
{
    cprBuffer_t *msg;
    static const char fname[] = "ccappPostMsg";
    cpr_status_e retval = CPR_SUCCESS;

    msg = (cprBuffer_t *) cpr_malloc(len);
    if (msg == NULL) {
        CCAPP_ERROR(DEB_F_PREFIX"failed to allocate message.",
               DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        return CPR_FAILURE;
    }

    memcpy(msg, data, len);

    if ((retval=ccappTaskSendMsg(msgId, msg, len, appId)) == CPR_FAILURE) {
        cpr_free(msg);
    }

    return retval;
}

/**
 *
 * CC Provider wrapper for cprSendMessage
 *
 * @param   cmd - Command
 * @param   msg - msg ptr
 * @param   len - len of msg
 * @param   usr -
 *
 * @return  CPR_SUCCESS/CPR_FAILURE
 *
 * @pre     msg is a malloc mem ptr
 */
cpr_status_e
ccappTaskSendMsg (uint32_t cmd, void *msg, uint16_t len, uint32_t UsrInfo)
{
    phn_syshdr_t *syshdr;

    syshdr = (phn_syshdr_t *) cprGetSysHeader(msg);
    if (!syshdr) {
        return CPR_FAILURE;
    }
    syshdr->Cmd = cmd;
    syshdr->Len = len;
    syshdr->Usr.UsrInfo = UsrInfo;

    if (cprSendMessage(ccapp_msgq , (cprBuffer_t*)msg, (void **)&syshdr) == CPR_FAILURE) {
        cprReleaseSysHeader(syshdr);
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

/**
 *
 * CCApp Provider main routine.
 *
 * @param   arg - CCApp msg queue
 *
 * @return  void
 *
 * @pre     None
 */
void CCApp_task(void * arg)
{
    static const char fname[] = "CCApp_task";
    phn_syshdr_t   *syshdr = NULL;
    appListener *listener = NULL;
    void * msg;

    //initialize the listener list
    sll_lite_init(&sll_list);

    CCAppInit();

    // If the "ready to start" condition variable has been created
    // (is non-null), we're going to wait for it to be signaled
    // before we start processing messages.
    if (ccAppReadyToStartCond) {
      PR_Lock(ccAppReadyToStartLock);
      while (!ccAppReadyToStart) {
        PR_WaitCondVar(ccAppReadyToStartCond, PR_INTERVAL_NO_TIMEOUT);
      }
      PR_Unlock(ccAppReadyToStartLock);
    }


    while (1) {
        msg = cprGetMessage(ccapp_msgq, TRUE, (void **) &syshdr);
        if ( msg) {
            CCAPP_DEBUG(DEB_F_PREFIX"Received Cmd[%d] for app[%d]", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                    syshdr->Cmd, syshdr->Usr.UsrInfo);

            listener = getCcappListener(syshdr->Usr.UsrInfo);
            if (listener != NULL) {
                (* ((appListener)(listener)))(msg, syshdr->Cmd);
            } else {
                CCAPP_DEBUG(DEB_F_PREFIX"Event[%d] doesn't have a dedicated listener.", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                        syshdr->Usr.UsrInfo);
            }
            cprReleaseSysHeader(syshdr);
            cpr_free(msg);
        }
    }
}



