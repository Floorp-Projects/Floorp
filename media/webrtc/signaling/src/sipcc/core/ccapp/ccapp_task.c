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

#include "ccapp_task.h"
#include "phone.h"
#include "CCProvider.h"

extern cprMsgQueue_t ccapp_msgq;
extern void CCAppInit();
static sll_lite_list_t sll_list;

/**
 * Add/Get ccapp task listener
 */
void addCcappListener(appListener* listener, int type) {

   listener_t *alistener = NULL;

   CCAPP_DEBUG(DEB_F_PREFIX"Entered: listenr=0x%x, type=%d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"),
           listener, type);

   if (listener == NULL)
   {
       CCAPP_ERROR(DEB_F_PREFIX"listener is NULL, returning\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"));
       return;
   }

   alistener = cpr_malloc(sizeof(listener_t));
   if (alistener == NULL) {
       CCAPP_ERROR(DEB_F_PREFIX"alistener is NULL, returning\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"));
       return;
   }

   alistener->type = type;
   alistener->listener_p = listener;

   sll_lite_link_tail(&sll_list, (sll_lite_node_t *)alistener);
   CCAPP_DEBUG(DEB_F_PREFIX"Added: listenr=0x%x, type=%d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "addCcappListener"),
           alistener->listener_p, alistener->type);
}

appListener *getCcappListener(int type) {
    static const char fname[] ="getCcappListener";
    listener_t *temp_info;
    sll_lite_node_t *iterator;

    CCAPP_DEBUG(DEB_F_PREFIX"entered: for app[%d]\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
            type);

    iterator = sll_list.head_p;
    while (iterator) {
        temp_info = (listener_t *)iterator;
        CCAPP_DEBUG(DEB_F_PREFIX"appid=%d, listener=0x%x\n",
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

    msg = (cprBuffer_t *) cprGetBuffer(len);
    if (msg == NULL) {
        CCAPP_ERROR(DEB_F_PREFIX"failed to allocate message.\n",
               DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        return CPR_FAILURE;
    }

    memcpy(msg, data, len);

    if ((retval=ccappTaskSendMsg(msgId, msg, len, appId)) == CPR_FAILURE) {
        cprReleaseBuffer(msg);
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

    while (1) {
        msg = cprGetMessage(ccapp_msgq, TRUE, (void **) &syshdr);
        if ( msg) {
            CCAPP_DEBUG(DEB_F_PREFIX"Received Cmd[%d] for app[%d]\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                    syshdr->Cmd, syshdr->Usr.UsrInfo);

            listener = getCcappListener(syshdr->Usr.UsrInfo);
            if (listener != NULL) {
                (* ((appListener)(listener)))(msg, syshdr->Cmd);
            } else {
                CCAPP_DEBUG(DEB_F_PREFIX"Event[%d] doesn't have a dedicated listener.\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                        syshdr->Usr.UsrInfo);
            }
            cprReleaseSysHeader(syshdr);
            cprReleaseBuffer(msg);
        }
    }
}



