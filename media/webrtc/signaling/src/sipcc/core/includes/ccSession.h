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

#ifndef _CCSESSION_H_
#define _CCSESSION_H_

#include "session.h"
#include "sessuri.h"

#define SID_TYPE_SHIFT 28
#define SID_LINE_SHIFT 16

#define GET_SESS_TYPE(x) ( (x & 0xF0000000) >> SID_TYPE_SHIFT )
#define GET_LINEID(x) (line_t)( (x & 0xFFF0000) >> SID_LINE_SHIFT )
#define GET_CALLID(x) (callid_t)(x & 0xFFFF) 




/** 
 *  ccSessionProviderCmd
 *      CallControl Provider Management Interface 
 *      Called by Application to issue cmds to sipStack
 *    
 *  @param data -  command and data
 *         data->cmd - see Session Provider Commands in session.h
 *         data->cmdData.ccData.reason - reason for SHUTDOWN/UNREGISTER_ALL_LINES CMD                        
                 CC_CAUSE_NORMAL/CC_CAUSE_NONE
 *         data->cmdData.ccData.reason_info - Descriptive stringa "notused"
 *  
 *  @return  none
 *
 */
void ccSessionProviderCmd(sessionProvider_cmd_t *data); 


/**
 *  ccSessionProviderState
 *      Method to report provider state updates to Application
 *
 *  @param state - indicates the Session Provider state CCApp_states_t
 *  @param data  - ccProvider_state_t indicating 
 *         data->stateData.ccData.mode  - REGMODE CCM
 *         data->stateData.ccData.cause - FAILOVER/FALLBACK
 *  
 *  @return  none
 *
 */
void ccSessionProviderState(unsigned int state, ccProvider_state_t *data); 


/**
 *  ccSessionCmd
 *      Method to Handle session lifecycle command such as realize, start etc.
 *
 *  @param sCmd - session liefcycle command
 *         sCmd->cmd - REQUEST CMD 
 *         sCmd->sessID - session ID
 *
 *  @return  none
 *
 */

void ccSessionCmd (sessionCmd_t *sCmd);


/**
 *  ccCreateSession
 *      
 *      Called by Application to create a new session
 *
 *  @param param - uri_t
 *         param->param.call_session_param.line_id - line on which to create the session
 *  @return  ccSession_id_t - id of the session created
 */

session_id_t ccCreateSession(uri_t *param);

/**
 *  ccCloseSession
 *
 *      Called by Application to close a session
 *
 *  @param sess_id - ID of the session to be closed
 *  
 *  @return  0 success -1 failure
 *
 */

int ccCloseSession(session_id_t sess_id);

/* Need to document IDs and data for the following 4 methods */

/**
 *  ccInvokeFeature
 *
 *      Called by Application to invoke feature on session
 *
 *  @param featData  - featID and Additional info if needed for the feature
 * 
 *  @return  none
 *
 */

void ccInvokeFeature(session_feature_t *featData);

/**
 *  ccInvokeProviderFeature
 *
 *      Called by Application to invoke device specific features
 *
 *  @param featData  - Additional info if needed for the feature
 *
 *  @return  none
 *
 */

void ccInvokeProviderFeature(session_feature_t *featData);


/**
 *  ccSessionUpdate
 *
 *      Called by sipstack to update session state and data
 *
 *  @param eventID - ID of the event updating the session data
 *  @param session_data  - event specific data
 *
 *  @return  none
 *
 */

void ccSessionUpdate(session_update_t *session);



/**
 *  ccFeatureUpdate
 *
 *      Called by sipstack to update feature state and data
 *
 *  @param featureID - ID of the feature updated
 *  @param session_data  - feature specific data
 *
 *  @return  none
 *
 */

void ccFeatureUpdate(feature_update_t *session);

/***** Internal APIs below this line ***************/

/**
 *  ccCreateSession 
 *
 *      Called to create a CC session 
 *
 *  @param param - ccSession_create_param_t
 *               Contains the type of session and specific data
 *
 *  @return  ccSession_id_t - id of the session created
 */
session_id_t createSessionId(line_t line, callid_t call);

void platform_sync_cfg_vers (char *cfg_ver, char *dp_ver, char *softkey_ver);
/********************************************************************************/

/* Misc getter/setter function  to get set the following */

#define PROPERTY_ID_MWI          1  // per line
#define PROPERTY_ID_TIME         2  // unsigned long
#define PROPERTY_ID_KPML         3
#define PROPERTY_ID_REGREASON    4
#define PROPERTY_ID_SPKR_HDST    5     1

void setIntProperty(unsigned int id, int  val);
int getIntProperty(unsigned int id);
void setStrProperty(unsigned int id, char * val);
char * getStrProperty(unsigned int id);


/* Preserved API's from TNP Platform */
char *ccSetDP(const char *dp_file_name);
// update_label_n_speed_dial method here
void setPropertyCacheBoolean(int cfg_id, int bool_value);	
void setPropertyCacheInteger(int cfg_id, int int_value);
void setPropertyCacheString(int cfg_id, const char *string_value);
void setPropertyCacheByte(int cfg_id, char byte_value);
void setPropertyCacheByteArray(int cfg_id, char *byte_value, int length);

/* BLF */
void ccBLFSubscribe(int request_id, int duration, const char *watcher,
                    const char *presentity, int app_id, int feature_mask);
void ccBLFUnsubscribe(int request_id);
void ccBLFUnsubscribeAll();
void blf_notification(int request_id, int status, int app_id);


/**********************************************************************

MID_JPlatUi_ui_log_status_msg,		Ask RCDN team if log is changing?

**********************************************************************/

#endif

