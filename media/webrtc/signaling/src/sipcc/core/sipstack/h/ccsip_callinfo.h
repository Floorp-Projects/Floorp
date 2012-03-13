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

#ifndef _CCSIP_CALLINFO_H_
#define _CCSIP_CALLINFO_H_

#include "cpr_types.h"
#include "ccapi.h"
#include "ccsip_core.h"

#define MAX_SIP_HEADER_LENGTH  1024
#define MAX_URI_LENGTH         256
#define URN_REMOTECC           "urn:x-cisco-remotecc:"
#define LAQUOT                 '<'
#define RAQUOT                 '>'
#define SPACE                  ' '
#define TAB                    '\t'
#define SEMI_COLON             ';'

// Feature string that appear in the feature-urn.  For example in
// <urn:x-cisco-remotecc:hold>, "hold" is the feature string.
#define SIP_CI_HOLD_STR        "hold"
#define SIP_CI_RESUME_STR      "resume"
#define SIP_CI_BARGE_STR       "barge"
#define SIP_CI_CBARGE_STR      "cbarge"
#define SIP_CI_CALL_INFO_STR   "callinfo"
#define SIP_CI_SILENT_STR      "silent"
#define SIP_CI_COACHING_STR    "coaching"


// Definitions relating to the hold-resume feature urn.
#define SIP_CI_HOLD_REASON         "reason="
#define SIP_CI_HOLD_REASON_XFER    "transfer"
#define SIP_CI_HOLD_REASON_CONF    "conference"

// Definitions relating to the callinfo feature urn.
#define SIP_CI_SECURITY           "security="
#define SIP_CI_SECURITY_UNKNOWN   "unknown"
#define SIP_CI_SECURITY_NOT_AUTH  "NotAuthenticated"
#define SIP_CI_SECURITY_AUTH      "Authenticated"
#define SIP_CI_SECURITY_ENCRYPTED "Encrypted"
#define SIP_CI_ORIENTATION        "orientation="
#define SIP_CI_ORIENTATION_TO     "to"
#define SIP_CI_ORIENTATION_FROM   "from"
#define SIP_CI_POLICY             "policy="	
#define SIP_CI_POLICY_UNKNOWN     "unknown"	
#define SIP_CI_POLICY_CHAPERONE   "chaperone"
#define SIP_CI_CALL_INSTANCE      "call-instance="
#define SIP_CI_CTI_CALLID         "cti-callid="
#define SIP_CI_UI_STATE           "ui-state="
#define SIP_CI_UI_STATE_RINGOUT   "ringout"
#define SIP_CI_UI_STATE_CONNECTED "connected"
#define SIP_CI_PRIORITY           "priority="
#define SIP_CI_PRIORITY_URGENT    "urgent"
#define SIP_CI_PRIORITY_EMERGENCY "emergency"
#define SIP_CI_UI_STATE_BUSY      "busy"
#define SIP_CI_GCID               "gci="
#define SIP_CI_DUSTINGCALL        "isDustingCall"

// Definitions relating to a generic feature parm.
#define SIP_CI_GENERIC            "purpose="
#define SIP_CI_GENERIC_ICON       "icon"
#define SIP_CI_GENERIC_INFO       "info"
#define SIP_CI_GENERIC_CARD       "card"

#define SKIP_LWS(p)            while (*p == SPACE || *p == TAB) { \
                                      p++; \
                               }
#define SKIP_WHITE_SPACE(p)    while (*p == SPACE || *p == TAB || \
                                      *p == '\n') { \
                                      p++; \
                               }

/*
 * Encoding function
 */
char* ccsip_encode_call_info_hdr(cc_call_info_t *call_info_p,
                                 const char *misc_parms_p);

void ccsip_free_call_info_header(cc_call_info_t *call_info_p); 

void ccsip_store_call_info(cc_call_info_t *call_info_p, ccsipCCB_t* ccb);
void ccsip_process_call_info_header(sipMessage_t *request_p, ccsipCCB_t* ccb);

#endif
