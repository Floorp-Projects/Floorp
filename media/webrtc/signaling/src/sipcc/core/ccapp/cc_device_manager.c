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

#include "cc_constants.h"
#include "session.h"
#include "ccSession.h"
#include "scSession.h"
#include "cpr_types.h"
#include "cpr_string.h"
#include "phone_debug.h"
#include "sessuri.h"
#include "config_api.h"
#include "CCProvider.h"
#include "ccapp_task.h"
#include "cc_device_manager.h"
#include "ccapi_service.h"
#include "cc_service.h"
#include "subscription_handler.h"
#include "ccapi_snapshot.h"
#include "util_string.h"


#define STARTUP_NORMAL           0
#define STARTUP_UNSPECIFIED      1
#define SHUTDOWN_NORMAL          2
#define SHUTDOWN_UNSPECIFIED     3
#define SHUTDOWN_VERMISMATHC     4
#define RSP_TYPE_COMPLETE        5


mgmt_state_t mgmtState = MGMT_STATE_IDLE;
int parse_config_properties (int device_handle, const char *device_name, const char *cfg, int from_memory);
static boolean isStartRequestPending = FALSE;
static boolean isServiceStopped = TRUE;
extern int g_dev_hdl;
extern char g_dev_name[];
extern char g_cfg_p[];
extern int g_compl_cfg;


extern cc_boolean is_action_to_be_deferred(cc_action_t action);


char *mgmt_event_to_str (int evt) {

    switch (evt) {
        case EV_CC_CREATE:
            return "EV_CC_CREATE";
        case EV_CC_START:
            return "EV_CC_START";
        case EV_CC_CONFIG_RECEIVED:
            return "EV_CC_CONFIG_RECEIVED";
        case EV_CC_DO_SOFT_RESET:
            return "EV_CC_DO_SOFT_RESET";
        case EV_CC_INSERVICE:
            return "EV_CC_INSERVICE";
        case EV_CC_OOS_FAILOVER:
            return "EV_CC_OOS_FAILOVER";
        case EV_CC_OOS_FALLBACK:
            return "EV_CC_OOS_FALLBACK";
        case EV_CC_OOS_REG_ALL_FAILED:
            return "EV_CC_OOS_REG_ALL_FAILED";
        case EV_CC_OOS_SHUTDOWN_ACK:
            return "EV_CC_OOS_SHUTDOWN_ACK";
        case EV_CC_RE_REGISTER:
            return "EV_CC_RE_REGISTER";
        case EV_CC_STOP:
            return "EV_CC_STOP";
        case EV_CC_DESTROY:
            return "EV_CC_DESTROY";
        case EV_CC_IP_VALID:
            return "EV_CC_IP_VALID";
        case EV_CC_IP_INVALID:
            return "EV_CC_IP_INVALID";
    }

    return "EV_INVALID";
}

char *mgmt_state_to_str (int state) {

    switch (state) {
        case MGMT_STATE_IDLE:
            return "MGMT_STATE_IDLE";
        case MGMT_STATE_CREATED:
            return "MGMT_STATE_CREATED";
        case MGMT_STATE_REGISTERING:
            return "MGMT_STATE_REGISTERING";
        case MGMT_STATE_REGISTERED:
            return "MGMT_STATE_REGISTERED";
        case MGMT_STATE_OOS:
            return "MGMT_STATE_OOS";
        case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
            return "MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK";
        case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
            return "MGMT_STATE_WAITING_FOR_CONFIG_FILE";
        case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
            return "MGMT_STATE_OOS_AWAIT_UN_REG_ACK";
        case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
            return "MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK";
        case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
            return "MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK";
    }

    return "MGMT_STATE_INVALID";
}

void  updateMediaConfigProperties( void )
{
        //TODO: check Java code


}

void  updateVideoConfigProperties( void )
{
        //TODO: check Java code

}

/*
 * action for handled events for device manager
 */
int action(int cmd)
{
    int retVal = 0;
    sessionProvider_cmd_t proCmd;

    memset ( &proCmd, 0, sizeof(sessionProvider_cmd_t));
    proCmd.sessionType = SESSIONTYPE_CALLCONTROL;
    proCmd.cmd = cmd;

    if (cmd == CMD_INSERVICE ) {
        CCAPP_DEBUG("CC_device_manager_action: CMD_INSERVICE \n");
        updateMediaConfigProperties();
        updateVideoConfigProperties();
        proCmd.cmdData.ccData.reason = STARTUP_NORMAL;
        if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                       sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG("ccInvokeFeature: ccappTaskSendMsg failed\n");
        }
    } else if (cmd == CMD_INIT) {

        CCAPP_DEBUG("CC_device_manager_action: CMD_INIT \n");
        proCmd.cmdData.ccData.reason = STARTUP_NORMAL;
       
        if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                       sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG("ccInvokeFeature: ccappTaskSendMsg failed\n");
        }
    } else if (cmd == CMD_RESTART) {

        CCAPP_DEBUG("CC_device_manager_action: CMD_RESTART \n");
        updateMediaConfigProperties();
        proCmd.cmdData.ccData.reason = STARTUP_NORMAL;
    
        if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                       sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG("ccInvokeFeature: ccappTaskSendMsg failed\n");
        }

    } else if (cmd == CMD_SHUTDOWN) {

        CCAPP_DEBUG("CC_device_manager_action: CMD_SHUTDOWN \n");
        proCmd.cmdData.ccData.reason = CC_CAUSE_REG_ALL_FAILED;
        if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                       sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG("ccInvokeFeature: ccappTaskSendMsg failed\n");
        }
    } else {

        proCmd.cmdData.ccData.reason = STARTUP_NORMAL;
        CCAPP_DEBUG("CC_device_manager_action: Default \n");
        if (ccappTaskPostMsg(CCAPP_SERVICE_CMD, (cprBuffer_t)&proCmd,
                       sizeof(sessionProvider_cmd_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG("ccInvokeFeature: ccappTaskSendMsg failed\n");
        }

    }

    return retVal;

}


cc_boolean is_phone_registered() {
    if (mgmtState == MGMT_STATE_REGISTERED) {
        return TRUE;
    } else {
        return FALSE;
    }
}
/*
 * Wrapper function for settting state for handled events for device manager
 */
void setState(int st) {
        DEF_DEBUG("setState: new registration state=  %s\n", mgmt_state_to_str(st));
        mgmtState = st;
}

void processInsToOos (void ) 
{
        //CCAPP_DEBUG("CC_device_manager:  processInsToOoS \n");
        DEF_DEBUG("CC_device_manager:  processInsToOoS \n");
        sub_hndlr_stop();
}

void prepareForSoftReset()
{
        CCAPP_DEBUG("CC_device_manager:  prepareForSoftReset\n");

}


void processInserviceEvent( void) 
{
    CCAPP_DEBUG("CC_device_manager:  process Inservice Event\n");
     if (g_deviceInfo.cucm_mode == CC_MODE_CCM ) {
        if (sub_hndlr_isAvailable() == FALSE) {
            sub_hndlr_start();
        }
    }   
        setState(MGMT_STATE_REGISTERED);
        //TODO: check Java code
}


/*
 * Event handler for device manager
 */
void registration_processEvent(int event) {
        
        boolean ignored=0;

        DEF_DEBUG("registration_processEvent:  Event %s, current State %s \n",
                     mgmt_event_to_str(event) , mgmt_state_to_str(mgmtState));

        switch (event) {
            case EV_CC_CREATE:
                switch ( mgmtState) {
                    case MGMT_STATE_IDLE:
                        setState(MGMT_STATE_CREATED);
                        init_empty_str(g_cfg_p);
                        CC_Service_create();
                        CC_Service_init(); 
                    break;
                    case MGMT_STATE_REGISTERED:
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
            break;

            case EV_CC_START:
                isServiceStopped = FALSE;
                switch ( mgmtState) {
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                        configFetchReq(0);
                    break;
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                        DEF_DEBUG("registration_processEvent: delaying start until SHUTDOWN_ACK is received.");
                        isStartRequestPending = TRUE;
                    break;
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_REGISTERED:
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
            break;

            case EV_CC_IP_INVALID:
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                        setState(MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK);
                        action(CMD_UNREGISTER_ALL_LINES);
                    break;
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                        setState(MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK);
                    break;
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }

                break;

            case EV_CC_IP_VALID:
                if (isServiceStopped == TRUE) {
                    ignored = 1;
                    DEF_DEBUG("registration_processEvent: "\
                    "Ignoring IP_VALID as service was not Started ");
                    break;
                }
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                    if (is_action_to_be_deferred(RE_REGISTER_ACTION)
                                == FALSE) {
                        processInsToOos();   
                        prepareForSoftReset();
                        setState(MGMT_STATE_OOS_AWAIT_UN_REG_ACK);
                        action(CMD_SHUTDOWN);
                    } 
                    break;    
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                    if (is_action_to_be_deferred(RE_REGISTER_ACTION)
                                == FALSE) {
                        prepareForSoftReset();
                        setState(MGMT_STATE_OOS_AWAIT_UN_REG_ACK);
                        action(CMD_SHUTDOWN);
                    } 
                    break;    
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
            break;

            case EV_CC_DO_SOFT_RESET:
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();          /* FALL THROUGH */
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                        prepareForSoftReset();
                        setState(MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK);
                        action(CMD_SHUTDOWN);
                        break;
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;

            case EV_CC_CONFIG_RECEIVED:
                switch ( mgmtState) {
                    case MGMT_STATE_CREATED:    // This state is only at init
                                             // needs handler to be added on receiving message
                        setState(MGMT_STATE_REGISTERING);
                        action(CMD_INSERVICE);
                        break;
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                        setState(MGMT_STATE_REGISTERING);
                        action(CMD_RESTART);
                        break;
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_REGISTERED:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;

            case EV_CC_INSERVICE:
                switch (mgmtState) {
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_REGISTERING:
                        setState(MGMT_STATE_REGISTERED);
                    case MGMT_STATE_REGISTERED:  
                        processInserviceEvent();
                        break;
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;

            case EV_CC_OOS_FAILOVER:
            case EV_CC_OOS_FALLBACK:
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();
                    case MGMT_STATE_REGISTERING:
                        setState(MGMT_STATE_OOS);
                        break;
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;
            case EV_CC_OOS_REG_ALL_FAILED:
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();      /* FALL THROUGH */
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_REGISTERING:
                        setState(MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK);
                        action(CMD_SHUTDOWN);
                        break;
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;

                /*
                 *This event shutdown sip stack to facilitate re-registration of
                 * all lines.
                 */
            case EV_CC_RE_REGISTER:
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();
                        setState(MGMT_STATE_OOS_AWAIT_UN_REG_ACK);
                        action(CMD_UNREGISTER_ALL_LINES);
                        break;
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                    default:
                        ignored = 1;
                        break;
                }
                break;

            case EV_CC_DESTROY:
                isStartRequestPending = FALSE;
                isServiceStopped = TRUE;
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                        setState(MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK);
                        action(CMD_UNREGISTER_ALL_LINES);
                        break;
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                        //setState(MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK);
                    	//Suhas
                    	setState(MGMT_STATE_IDLE);
                    	CC_Service_destroy();
                    	break;
                    case MGMT_STATE_CREATED:
                        CC_Service_destroy();
                        break;
                    case MGMT_STATE_IDLE:
                    	CC_Service_destroy();
                    	break;
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:

                    	// Suhas: Adding state change
                        setState(MGMT_STATE_IDLE);

                    default:
                        ignored = 1;
                        break;
                }

            break;
            case EV_CC_STOP:
                isStartRequestPending = FALSE;
                isServiceStopped = TRUE;
                switch ( mgmtState) {
                    case MGMT_STATE_REGISTERED:
                        processInsToOos();
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                        setState(MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK);
                        action(CMD_UNREGISTER_ALL_LINES);
                    break;
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                        setState(MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK);
                    break;
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_IDLE:
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
			 // Suhas: Adding state change
                        setState(MGMT_STATE_IDLE);

                    default:
                        ignored = 1;
                        break;
                }
            break;
            case EV_CC_OOS_SHUTDOWN_ACK:
                 switch ( mgmtState) {
                    case MGMT_STATE_OOS_AWAIT_UN_REG_ACK:
                        setState(MGMT_STATE_REGISTERING);
                        parse_config_properties(g_dev_hdl, g_dev_name, g_cfg_p, g_compl_cfg);
                        action(CMD_RESTART);
                        break;
                    case MGMT_STATE_OOS_AWAIT_SHUTDOWN_ACK:
                        setState(MGMT_STATE_WAITING_FOR_CONFIG_FILE);
                        configFetchReq(0);
			// Suhas: Adding state change 
			setState(MGMT_STATE_IDLE);
                        break;
                    case MGMT_STATE_STOP_AWAIT_SHUTDOWN_ACK:
                        setState(MGMT_STATE_WAITING_FOR_CONFIG_FILE);
                        if (isStartRequestPending == TRUE) {
                            isStartRequestPending = FALSE;
                            configFetchReq(0);
                        }
			// Suhas: Adding state change 
			setState(MGMT_STATE_IDLE);
                    break;
                    case MGMT_STATE_DESTROY_AWAIT_SHUTDOWN_ACK:
                        CC_Service_destroy();
                    break;
                    case MGMT_STATE_REGISTERED:
                    case MGMT_STATE_WAITING_FOR_CONFIG_FILE:
                    case MGMT_STATE_OOS:
                    case MGMT_STATE_REGISTERING:
                    case MGMT_STATE_CREATED:
                    case MGMT_STATE_IDLE:
                    default:
                        ignored = 1;
                        break;
                }
                break;
            default:
                break;
        }

        if (ignored) {
            DEF_DEBUG("registration_processEvent: IGNORED  Event  %s in State %s \n",
                     mgmt_event_to_str(event) , mgmt_state_to_str(mgmtState));
        }

}



