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

#include "cpr_stdio.h"
#include <time.h>
#include "sessionHash.h"
#include "CCProvider.h"
#include "call_logger.h"



void calllogger_init_call_log (cc_call_log_t *log)
{
    log->localPartyName = strlib_empty();
    log->localPartyNumber = strlib_empty();
    log->remotePartyName[0] = strlib_empty();
    log->remotePartyName[1] = strlib_empty();
    log->remotePartyNumber[0] = strlib_empty();
    log->remotePartyNumber[1] = strlib_empty();
    log->altPartyNumber[0] = strlib_empty();
    log->altPartyNumber[1] = strlib_empty();
    log->startTime = 0;
    log->duration = 0;

    log->logDisp = CC_LOGD_UNKNWN;
    log->callState = MAX_CALL_STATES;
}

void calllogger_copy_call_log (cc_call_log_t *dest, cc_call_log_t * src)
{
    dest->localPartyName = strlib_copy(src->localPartyName);
    dest->localPartyNumber = strlib_copy(src->localPartyNumber);
    dest->remotePartyName[0] = strlib_copy(src->remotePartyName[0]);
    dest->remotePartyName[1] = strlib_copy(src->remotePartyName[1]);
    dest->remotePartyNumber[0] = strlib_copy(src->remotePartyNumber[0]);
    dest->remotePartyNumber[1] = strlib_copy(src->remotePartyNumber[1]);
    dest->altPartyNumber[0] = strlib_copy(src->altPartyNumber[0]);
    dest->altPartyNumber[1] = strlib_copy(src->altPartyNumber[1]);
    dest->startTime = src->startTime;
    dest->duration = src->duration;

    dest->logDisp = src->logDisp;
    dest->callState = src->callState;
}

void calllogger_free_call_log (cc_call_log_t *log)
{
    strlib_free(log->localPartyName);
    strlib_free(log->localPartyNumber);
    strlib_free(log->remotePartyName[0]);
    strlib_free(log->remotePartyName[1]);
    strlib_free(log->remotePartyNumber[0]);
    strlib_free(log->remotePartyNumber[1]);
    strlib_free(log->altPartyNumber[0]);
    strlib_free(log->altPartyNumber[1]);

    calllogger_init_call_log(log);
}

void calllogger_print_call_log(cc_call_log_t *log)
{
    static const char *fname = "calllogger_print_call_log";

    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    CCLOG_DEBUG("Remote ID %s:%s %s:%s\n LocalID %s:%s \n alt %s:%s\n", 
           log->remotePartyName[0], log->remotePartyNumber[0], 
           log->remotePartyName[1], log->remotePartyNumber[1], 
           log->localPartyName, log->localPartyNumber, 
           log->altPartyNumber[0], log->altPartyNumber[1] );
    CCLOG_DEBUG("state %d \n Disp %d\n", log->callState, log->logDisp);
}
  
/**
 * Call logger update to be called placed call num update
 */
void calllogger_setPlacedCallInfo (session_data_t *data)
{
    static const char *fname = "calllogger_setPlacedCallInfo";

    CCLOG_DEBUG(DEB_F_PREFIX"updating placed number for session %x to %s:%s\n",
                            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->sess_id, 
                     data->cld_name, data->cld_number);
    if ( data->call_log.logDisp == CC_LOGD_RCVD ) { return;}
    data->call_log.remotePartyName[0] = strlib_copy(data->plcd_name);
    data->call_log.remotePartyNumber[0] = strlib_copy(data->plcd_number);
    data->call_log.logDisp = CC_LOGD_SENT;
    data->call_log.startTime = time(NULL);
}

/**
 * call logger api to be called for log disp update
 */
void calllogger_updateLogDisp (session_data_t *data)
{
    static const char *fname = "calllogger_updateLogDisp";

    CCLOG_DEBUG(DEB_F_PREFIX"updating log disposition for session %x to %d\n",
                            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->sess_id, data->log_disp);
    data->call_log.logDisp = data->log_disp;
}
                              
cc_boolean partyInfoPassedTheNumberFilter (cc_string_t partyString) 
{
    static const char *fname = "partyInfoPassedTheNumberFilter";

    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        // check if partyString is something we should not be logging at this point
        // Uris: CfwdALL 105
        // Conference 152
        if (partyString[1] == 17 ||
            partyString[1] == 91 ||
//            partyString[1] == 698 ||
            partyString[1] == 05 ||
            partyString[1] == 18 ||
            partyString[1] == 16 ||
            partyString[1] == 52 ) {

            CCLOG_DEBUG(DEB_F_PREFIX"Filtering out the partyName=%s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), partyString);
            return FALSE;
        }
        return TRUE;
}

/**
 * partyInfoPassedTheNameFilter
 *
 * @param partyString String
 * @return boolean
 */
cc_boolean partyInfoPassedTheNameFilter(cc_string_t partyString) {
    static const char *fname = "partyInfoPassedTheNameFilter";

    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    // If the name String has Conference, filter it out
    if (partyString[1] == 52 || partyString[1] == 53) {
        CCLOG_DEBUG(DEB_F_PREFIX"Filtering out the partyName=%s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), partyString);
        return FALSE;
    }
    return TRUE;
}

static cc_string_t missedCallMask=NULL;
void calllogger_setMissedCallLoggingConfig(cc_string_t mask) {
    static const char *fname = "calllogger_setMissedCallLoggingConfig";

    
    CCLOG_DEBUG(DEB_F_PREFIX"Entering... mask=%s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), mask);
    if ( missedCallMask == NULL) {
        missedCallMask = strlib_empty();
    }
    missedCallMask = strlib_update(missedCallMask, mask);
}

cc_boolean isMissedCallLoggingEnabled (unsigned int line)
{
    static const char *fname = "isMissedCallLoggingEnabled";

    CCLOG_DEBUG(DEB_F_PREFIX"Entering... mask=%s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), missedCallMask);
    if (missedCallMask == NULL) {
    	return TRUE;
    }

    if ((line > 0) && strlen(missedCallMask) > (line-1)) {
        if ( missedCallMask[line-1] == '0' ) {
            return FALSE;
        }
    }
    return TRUE;
}


void handlePlacedCall (session_data_t *data) 
{
    static const char *fname = "handlePlacedCall";
   
    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n",DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    //populate calling party if not already populated
    if ( data->call_log.localPartyNumber == strlib_empty() ) {
       data->call_log.localPartyNumber = strlib_update(data->call_log.localPartyNumber, data->clg_number);
       data->call_log.localPartyName = strlib_update(data->call_log.localPartyName, data->clg_name);
    }

    // first update or update with the same number we update name and number
    if ( data->call_log.remotePartyNumber[0] == strlib_empty() ||
            (data->cld_number[0] != 0 && strncmp(data->call_log.remotePartyNumber[0], 
                        data->cld_number, strlen(data->cld_number)) == 0 )) {
       if ( partyInfoPassedTheNameFilter(data->cld_name) &&
            partyInfoPassedTheNumberFilter(data->cld_number) )
           data->call_log.remotePartyNumber[0] = strlib_update(data->call_log.remotePartyNumber[0], data->cld_number);
           data->call_log.remotePartyName[0] = strlib_update(data->call_log.remotePartyName[0], data->cld_name);
    } 

    // Start the duration count once the call reaches connected state.
    if ( data->state == CONNECTED &&
         data->call_log.startTime == 0 ) {
       data->call_log.startTime = time(NULL);
    }

    if (data->state == ONHOOK ) {
        // only set the duration if the call entered the connected state
	if ( data->call_log.startTime != 0 ) {
            data->call_log.duration = (cc_uint32_t) (time(NULL) - data->call_log.startTime);
	} else {
            data->call_log.startTime = time(NULL);
        }
    } 

    data->call_log.callState = data->state;
}


void handleMissedOrReceviedCall ( session_data_t *data )
{
    static const char *fname = "handleMissedOrReceviedCall";
    int line = GET_LINE_ID(data->sess_id);
    cc_string_t localName = strlib_empty(), localNumber = strlib_empty();
    cc_string_t remoteName = strlib_empty(), remoteNumber = strlib_empty();
    
    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (data->type == CC_CALL_TYPE_INCOMING || data->type == CC_CALL_TYPE_FORWARDED ) {
        localName = data->cld_name;
        localNumber = data->cld_number;
        remoteName = data->clg_name;
        remoteNumber = data->clg_number;
    } else {
        localName = data->clg_name;
        localNumber = data->clg_number;
        remoteName = data->cld_name;
        remoteNumber = data->cld_number;
    }

    if ( data->call_log.localPartyNumber == strlib_empty() ) {
       data->call_log.localPartyNumber = strlib_update(data->call_log.localPartyNumber, localNumber);
       data->call_log.localPartyName = strlib_update(data->call_log.localPartyName, localName);
    }

    // first update or update with the same number we update name and number
    if ( data->call_log.remotePartyNumber[0] == strlib_empty() ||
            (remoteNumber[0] != 0 && strncmp(data->call_log.remotePartyNumber[0], 
                        remoteNumber, strlen(remoteNumber)) == 0 )) {
       data->call_log.remotePartyNumber[0] = strlib_update(data->call_log.remotePartyNumber[0], remoteNumber);
       data->call_log.altPartyNumber[0] = strlib_update(data->call_log.altPartyNumber[0], data->alt_number);
       if (data->call_log.remotePartyName[0] == strlib_empty() ) {
           data->call_log.remotePartyName[0] = strlib_update(data->call_log.remotePartyName[0], remoteName);
       }
    } else {
       // since number doesn't match this is a new leg
       data->call_log.remotePartyName[1] = strlib_update(data->call_log.remotePartyName[1], remoteName);
       data->call_log.remotePartyNumber[1] = strlib_update(data->call_log.remotePartyNumber[1], remoteNumber);
       data->call_log.altPartyNumber[1] = strlib_update(data->call_log.altPartyNumber[1], data->alt_number);
    }

    if ( data->state == ONHOOK ) {
       if ( data->call_log.callState == RINGIN ) {
           data->call_log.startTime = time(NULL);
           if (isMissedCallLoggingEnabled(line)) { 
               data->call_log.logDisp = CC_LOGD_MISSED;
           } else { 
               data->call_log.logDisp = CC_LOGD_DELETE;
           }
           data->call_log.startTime = time(NULL);
           data->call_log.duration = 0; //missed call duration is 0
       } else if ( data->call_log.startTime != 0 ){
	   //connected call going onhook update duration
           data->call_log.duration = (cc_uint32_t) (time(NULL) - data->call_log.startTime);
       }
    }

    if ( data->state == CONNECTED && data->call_log.startTime == 0 ) {
       data->call_log.logDisp = CC_LOGD_RCVD;
       data->call_log.startTime = time(NULL);
    }

    data->call_log.callState = data->state;
}
    

/**
 * Call logger update to be called on attr or callinfo or state events
 */
void  calllogger_update (session_data_t *data)
{
    static const char *fname = "calllogger_update";
    
    CCLOG_DEBUG(DEB_F_PREFIX"Entering...\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    if (data->call_log.logDisp == CC_LOGD_DELETE) {
        CCLOG_DEBUG(DEB_F_PREFIX"log disposition set to delete. Ignoring call logging for sess_id=%x\n", 
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->sess_id);
    }

    if (data->call_log.logDisp == CC_LOGD_RCVD || data->call_log.logDisp == CC_LOGD_MISSED ||
          data->type == CC_CALL_TYPE_INCOMING || data->type == CC_CALL_TYPE_FORWARDED ) {
        handleMissedOrReceviedCall(data);
    } else if ( data->call_log.logDisp == CC_LOGD_SENT ||
                    data->type == CC_CALL_TYPE_OUTGOING ) {
        handlePlacedCall (data);
    }

}
