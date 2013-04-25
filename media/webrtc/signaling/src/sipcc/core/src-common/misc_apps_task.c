/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "platform_api.h"

#define MISC_ERROR(format, ...) CSFLogError("misc" , format , ## __VA_ARGS__ )

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
        MISC_ERROR(MISC_F_PREFIX"invalid input, exiting", fname);
        return;
    }

    if (platThreadInit("MiscAppTask") != 0) {
        MISC_ERROR(MISC_F_PREFIX"Failed to Initialize the thread, exiting",
                fname);
        return;
    }

    /*
     * Create retry-after timers.
     */
    if (pres_create_retry_after_timers() == CPR_FAILURE) {
        MISC_ERROR(MISC_F_PREFIX"failed to create retry-after Timers, exiting",
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
                MISC_ERROR(MISC_F_PREFIX"invalid msg <%d> received",
                        fname, syshdr_p->Cmd);
                break;
            }

            cprReleaseSysHeader(syshdr_p);
            cpr_free(msg_p);
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
    DEF_DEBUG(DEB_F_PREFIX"Unloading Misc app and destroying Misc app thread",
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    configapp_shutdown();
    MiscAppTaskShutdown();
    (void)cprDestroyThread(misc_app_thread);
}

/************************************* THE END ******************************************/
