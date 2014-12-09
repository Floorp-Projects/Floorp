/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_string.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "stdarg.h"
#include "logger.h"
#include "logmsg.h"
#include "phone_debug.h"
#include "text_strings.h"
#include "uiapi.h"
#include "platform_api.h"
#include "prot_configmgr.h"

#define MAX_LOG_CACHE_ENTRIES 20

/*
 * Log a message.
 * Cause the message to be printed to the console port
 * as well, the log is stored in a local data area for
 * later viewing
 */
void
log_msg (int phrase_index, ...)
{
    char phrase_buf[LOG_MAX_LEN * 4];
    char status_msg[LOG_MAX_LEN * 4];
    va_list ap;

    /*
     * Make sure that the phrase index is valid.
     */
    if (phrase_index == 0) {
        return;
    }

    /*
     * Get the translated phrase index from the Java code.
     */
    if (platGetPhraseText(phrase_index, phrase_buf, (LOG_MAX_LEN * 4)) == CPR_FAILURE) {
        return;
    }

    /*
     * If extra data is required, sprintf this into the status message buffer
     */
    va_start(ap, phrase_index);
    vsprintf(status_msg, phrase_buf, ap);
    va_end(ap);

    CSFLogError("common", "%%%s", status_msg);

    /*
     * For now, do not send the Registration messages over to the Java Status
     * Logs.  They come out too fast and will overwhelm the existing logging
     * mechanism.  We will need to implement a new mechanism in order to put
     * these in the phone's status menu.
     */
    switch (phrase_index) {
    case LOG_REG_MSG:
    case LOG_REG_RED_MSG:
    case LOG_REG_AUTH_MSG:
    case LOG_REG_AUTH_HDR_MSG:
    case LOG_REG_AUTH_SCH_MSG:
    case LOG_REG_CANCEL_MSG:
    case LOG_REG_AUTH:
    case LOG_REG_AUTH_ACK_TMR:
    case LOG_REG_AUTH_NO_CRED:
    case LOG_REG_AUTH_UNREG_TMR:
    case LOG_REG_RETRY:
    case LOG_REG_UNSUPPORTED:
    case LOG_REG_AUTH_SERVER_ERR:
    case LOG_REG_AUTH_GLOBAL_ERR:
    case LOG_REG_AUTH_UNKN_ERR:
        return;

    default:
        break;
    }

    ui_log_status_msg(status_msg);
}


