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

#ifndef _SESSION_H_
#define _SESSION_H_

#include "sessionConstants.h"
#include "sessionTypes.h"
#include "sessuri.h"

/**
 *  sessionProviderCmd
 *      Session Provider Management Interfaces
 *      Called by Application to issue cmds to Session Provider
 *
 *  @param data -  sessionProvider_cmd_t
 *              Contains the command session provider type and provider specific data
 *
 *  @return  none
 *
 */
void sessionProviderCmd(sessionProvider_cmd_t *);

/**
 *  sessionProviderState
 *      Method to report session provider state updates to Application
 *
 *  @param state  - provider_state_t
 *                Contains the INS/OOS state along with provider specific data
 *
 *  @return  none
 *
 */
void sessionProviderState(provider_state_t *state);

/**
 *  createSession
 *
 *      Called to create a session of requested type
 *
 *  @param param - uri
 *                indicates type of session and specific params
 *
 *  @return  ccSession_id_t - id of the session created
 */

session_id_t createSession(uri_t uri_info);

/**
 *  closeSession
 *
 *      Called to close an existing session
 *
 *  @param sess_id - session id of the session to be closed
 *
 *  @return  >=0 success, -1 failure
 */

int closeSession(session_id_t sess_id);

/**
 *  sessionCmd
 *      Session Lifecycle Management Interfaces
 *      Called by Application to manage Session States
 *
 *  @param data -  sessionCmd_t
 *              Contains the command session type and session specific data
 *
 *  @return  none
 *
 */
void sessionCmd(sessionCmd_t *sCmd);

/**
 *  invokeFeature
 *
 *      Called to invoke a feature on session or device
 *
 *  @param feat - feature specific data along with its id
 *  @param featData  - Additional info if needed for the feature
 *
 *  @return  none
 *
 */

void invokeFeature(session_feature_t *feat);

/**
 *  invokeProviderFeature
 *
 *      Called to invoke a feature on session or device
 *
 *  @param feat - feature specific data along with its id
 *  @param featData  - Additional info if needed for the feature
 *
 *  @return  none
 *
 */

void invokeProviderFeature(session_feature_t *feat);

/**
 *  sessionUpdate
 *
 *      Called by session provider to update session state and data
 *
 *  @param session - session_update_t
 *                 Contains session specific event state and data
 *
 *  @return  none
 *
 */
void sessionUpdate(session_update_t *session);

/**
 *  featureUpdate
 *
 *      Called by session provider to update feature state and data
 *  not specific to a session
 *
 *  @param feature - feature specific events and data
 *
 *  @return  none
 *
 */
void featureUpdate(feature_update_t *feature);


/**
 *  sessionMgmt
 *
 *      Called to manage various misc. functions of the device
 *
 *  @param sessMgmt - the data
 *
 *  @return  none
 *
 */
void sessionMgmt (session_mgmt_t *sess_mgmt);

/**
 *  sessionSendInfo
 *
 *      Called to send an Info Package
 *
 *  @param send_info - the session ID and the Info Package to be sent
 *
 *  @return  none
 *
 */
void sessionSendInfo (session_send_info_t *send_info);

/**
 *  sessionRcvdInfo
 *
 *      Called to forward a received Info Package (either parsed or unparsed)
 *      to the Java side
 *
 *  @param rcvd_info - the session ID and Info Package received
 *
 *  @return  none
 *
 */
void sessionRcvdInfo (session_rcvd_info_t *rcvd_info);

#endif

