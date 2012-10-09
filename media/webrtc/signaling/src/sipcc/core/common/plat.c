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

#include <string.h>
#include "cpr.h"
#include "phone_debug.h"
#include "CCProvider.h"
#include "ccsip_pmh.h"
#include "sessionTypes.h"
#include "ccapp_task.h"

// Why don't we modify strlib_malloc to handle NULL?
#define STRLIB_CREATE(str)  (str)?strlib_malloc((str), strlen((str))):strlib_empty()

void
platform_apply_config (char * configVersionStamp,
                       char * dialplanVersionStamp,
                       char * fcpVersionStamp,
                       char * cucmResult,
                       char * loadId, 
                       char * inactiveLoadId, 
                       char * loadServer,
                       char * logServer,
                       boolean ppid);
/**
 * This function calls the JNI function to sends the information received
 * in apply-config NOTIFY message to Java side.
 *
 * @param   configVersionStamp - version stamp for config file
 * @param   dialplanVersionStamp - version stamp for the dialplan file
 * @param   fcpVersionStamp - version stamp for the softkey file (?)
 * @param   cucmResult  - CUCM result after applying config by CUCM
 * @param   loadId  - loadId to upgrade as requested by CUCM
 * @param   inactiveLoadId  - inactive loadId for inactive partition as requested by CUCM
 * @param   loadServer  - load server form where to pick loadId
 * @param   logServer  - log server for logging output of peer to peer upgrade.
 * @param   ppid  - specify whether peer to peer upgrade is enabled/disabled.
 *
 * @return  none
 */
void
platform_apply_config (char * configVersionStamp,
                       char * dialplanVersionStamp,
                       char * fcpVersionStamp,
                       char * cucmResult,
                       char * loadId,
                       char * inactiveLoadId, 
                       char * loadServer,
                       char * logServer,
                       boolean ppid)
{
    static const char fname[] = "platform_apply_config";
    session_mgmt_t msg;

    fcpVersionStamp = (fcpVersionStamp != NULL) ? fcpVersionStamp : "";

    /// Print the arguments
    CCAPP_DEBUG(DEB_F_PREFIX"   configVersionStamp=%s \ndialplanVersionStamp=%s"
           "\nfcpVersionStamp=%s \ncucmResult=%s "
           "\nloadId=%s \ninactiveLoadId=%s \nloadServer=%s \nlogServer=%s "
           "\nppid=%s\n", DEB_F_PREFIX_ARGS(PLAT_API, fname),
           (configVersionStamp != NULL) ? configVersionStamp : "",
           (dialplanVersionStamp != NULL) ? dialplanVersionStamp:"",
           fcpVersionStamp,
           cucmResult != NULL ? cucmResult: "",
           (loadId != NULL) ? loadId : "",
           (inactiveLoadId != NULL) ? inactiveLoadId : "",
           (loadServer != NULL) ? loadServer : "",
           (logServer != NULL) ? logServer : "",
           ppid == TRUE? "True": "False");


    // following data is freed in function freeSessionMgmtData()
    msg.func_id = SESSION_MGMT_APPLY_CONFIG;
    msg.data.config.config_version_stamp = STRLIB_CREATE(configVersionStamp);
    msg.data.config.dialplan_version_stamp = STRLIB_CREATE(dialplanVersionStamp);
    msg.data.config.fcp_version_stamp = STRLIB_CREATE(fcpVersionStamp);
    msg.data.config.cucm_result = STRLIB_CREATE(cucmResult);
    msg.data.config.load_id = STRLIB_CREATE(loadId);
    msg.data.config.inactive_load_id = STRLIB_CREATE(inactiveLoadId);
    msg.data.config.load_server = STRLIB_CREATE(loadServer);
    msg.data.config.log_server = STRLIB_CREATE(logServer);
    msg.data.config.ppid = ppid;

    if ( ccappTaskPostMsg(CCAPP_SESSION_MGMT, &msg, sizeof(session_mgmt_t), CCAPP_CCPROVIER) != CPR_SUCCESS ) {
        CCAPP_DEBUG(DEB_F_PREFIX"failed to send platform_apply_config msg\n", DEB_F_PREFIX_ARGS(PLAT_API, fname));
    }
}
