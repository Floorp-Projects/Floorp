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
#include "cpr_memory.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_ipc.h"
#include "cpr_errno.h"
#include "phone.h"
#include "phntask.h"
#include "phone_debug.h"
#include "debug.h"
#include "subapi.h"
#include "misc_apps_task.h"
#include "pres_sub_not_handler.h"
#include "configapp.h"

#define MISC_ERROR err_msg

cprMsgQueue_t s_misc_msg_queue;
void destroy_misc_app_thread(void); 
extern cprThread_t misc_app_thread;

cpr_status_e
MiscAppTaskSendMsg (uint32_t cmd, cprBuffer_t buf, uint16_t len)
{
    phn_syshdr_t *syshdr_p;

    syshdr_p = (phn_syshdr_t *) cprGetSysHeader(buf);
    if (!syshdr_p)
    {
        return CPR_FAILURE;
    }
    syshdr_p->Cmd = cmd;
    syshdr_p->Len = len;

    if (cprSendMessage(s_misc_msg_queue, buf, (void **)&syshdr_p) == CPR_FAILURE)
    {
        cprReleaseSysHeader(syshdr_p);
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}

void MiscAppTask (void *arg)
{
    static const char fname[] = "MiscAppTask";    
    void *msg_p;
    phn_syshdr_t *syshdr_p;

    /*
     * Get the misc apps message queue handle
     */
    s_misc_msg_queue  = (cprMsgQueue_t)arg;
    if (!s_misc_msg_queue) {
        MISC_ERROR(MISC_F_PREFIX"invalid input, exiting\n", fname);
        return;
    }
    
    if (platThreadInit("MiscAppTask") != 0) {
        MISC_ERROR(MISC_F_PREFIX"Failed to Initialize the thread, exiting\n",
                fname);
        return;
    }

    /*
     * Create retry-after timers.
     */
    if (pres_create_retry_after_timers() == CPR_FAILURE) {
        MISC_ERROR(MISC_F_PREFIX"failed to create retry-after Timers, exiting\n",
               fname);
        return;
    }

    while (1)
    {
        msg_p = cprGetMessage(s_misc_msg_queue, TRUE, (void **)&syshdr_p);
        if (msg_p)
        {
            switch(syshdr_p->Cmd) {
            case SUB_MSG_PRESENCE_SUBSCRIBE_RESP:
            case SUB_MSG_PRESENCE_NOTIFY:
            case SUB_MSG_PRESENCE_UNSOLICITED_NOTIFY:
            case SUB_MSG_PRESENCE_TERMINATE:
            case SUB_MSG_PRESENCE_GET_STATE:
            case SUB_MSG_PRESENCE_TERM_REQ:
            case SUB_MSG_PRESENCE_TERM_REQ_ALL:
            case TIMER_EXPIRATION:
            case SUB_HANDLER_INITIALIZED:
                pres_process_msg_from_msgq(syshdr_p->Cmd, msg_p);
                break;

            case SUB_MSG_CONFIGAPP_SUBSCRIBE:
            case SUB_MSG_CONFIGAPP_TERMINATE:
            case SUB_MSG_CONFIGAPP_NOTIFY_ACK:
                configapp_process_msg(syshdr_p->Cmd, msg_p);
                break;

            case THREAD_UNLOAD:
                destroy_misc_app_thread(); 
                break;

            default:
                MISC_ERROR(MISC_F_PREFIX"invalid msg <%d> received\n",
                        fname, syshdr_p->Cmd);
                break;
            }

            cprReleaseSysHeader(syshdr_p);
            cprReleaseBuffer(msg_p);
        }
    }
}

/**
 *
 * Perform cleaning up resoruces of misc. application task.
 *
 * @param none
 *
 * @return none
 *
 * @pre     none
 */
void MiscAppTaskShutdown (void)
{
    /* Destroy retry after timers */
    pres_destroy_retry_after_timers(); 
}

/*  
 *  Function: destroy_misc_thread
 *  Description:  shutdown and kill misc app thread 
 *  Parameters:   none
 *  Returns: none
 */
void destroy_misc_app_thread()
{
    static const char fname[] = "destroy_misc_app_thread";
    DEF_DEBUG(DEB_F_PREFIX"Unloading Misc app and destroying Misc app thread\n", 
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    configapp_shutdown();
    MiscAppTaskShutdown();
    (void)cprDestroyThread(misc_app_thread);
}

/************************************* THE END ******************************************/
