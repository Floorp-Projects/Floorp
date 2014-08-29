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
    /* Mozilla hack: just ignore everything that is posted here */
    (void)cmd;
    cpr_free(buf);
    (void)len;
    return CPR_SUCCESS;
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
