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

    err_msg("%%%s\n", status_msg);

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


