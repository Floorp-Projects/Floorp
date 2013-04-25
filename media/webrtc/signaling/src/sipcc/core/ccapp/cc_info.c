/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cc_info.h"
#include "sessionTypes.h"
#include "phone_debug.h"
#include "CCProvider.h"
#include "sessionConstants.h"
#include "ccapp_task.h"

/**
 * Send call information.
 * @param call_handle call handle
 * @param info_package the Info-Package header of the Info Package
 * @param info_type the Content-Type header of the Info Package
 * @param info_body the message body of the Info Package
 * @return void
 */
void CC_Info_sendInfo(cc_call_handle_t call_handle,
        string_t info_package,
        string_t info_type,
        string_t info_body) {
    static const char *fname = "CC_Info_sendInfo";
    session_send_info_t send_info;

    CCAPP_DEBUG(DEB_F_PREFIX"entry... call_handle=0x%x",
                DEB_F_PREFIX_ARGS(SIP_CC_SES, fname), call_handle);

    send_info.sessionID= (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT) + call_handle;;
    send_info.generic_raw.info_package = strlib_malloc(info_package, strlen(info_package));
    send_info.generic_raw.content_type = strlib_malloc(info_type, strlen(info_type));
    send_info.generic_raw.message_body = strlib_malloc(info_body, strlen(info_body));

    /* Once the msg is posted to ccapp_msgq, ccapp 'owns' these strings */
    // ccappTaskPostMsg does a shallow copy of *send_info
    if (ccappTaskPostMsg(CCAPP_SEND_INFO, &send_info,
                         sizeof(session_send_info_t), CCAPP_CCPROVIER) != CPR_SUCCESS) {
        CCAPP_ERROR(DEB_F_PREFIX"ccappTaskPostMsg failed",
                    DEB_F_PREFIX_ARGS(SIP_CC_SES, fname));
    }

}
