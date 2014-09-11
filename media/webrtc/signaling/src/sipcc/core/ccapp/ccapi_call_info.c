/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_stdio.h"
#include "ccapi_call.h"
#include "sessionHash.h"
#include "CCProvider.h"
#include "text_strings.h"
#include "phone_debug.h"
#include "peer_connection_types.h"

/**
 * get Line on which this call is
 * @param [in] handle - call handle
 * @return cc_line_id_t - line ID
 */
cc_lineid_t CCAPI_CallInfo_getLine(cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getLine";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %u", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), GET_LINE_ID(CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id)));
     return GET_LINE_ID(CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id));
  }

  return 0;
}

/**
 * get Call state
 * @param handle - call handle
 * @return call state
 */
cc_call_state_t CCAPI_CallInfo_getCallState(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallState";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->state);
     return data->state;
  }

  return ONHOOK;
}

/**
 * get FSM state
 * @param handle - call handle
 * @return call state
 */
fsmdef_states_t CCAPI_CallInfo_getFsmState(cc_callinfo_ref_t handle){
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering",
              DEB_F_PREFIX_ARGS(SIP_CC_PROV, __FUNCTION__));

  if ( data ){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X",
                 DEB_F_PREFIX_ARGS(SIP_CC_PROV, __FUNCTION__), data->state);
     return data->fsm_state;
  }

  return FSMDEF_S_IDLE;
}

/**
 * get call attributes
 * @param handle - call handle
 * @return call attributes
 */
cc_call_attr_t CCAPI_CallInfo_getCallAttr(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallAttr";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->attr);
     return data->attr;
  }

  return 0;
}

/**
 * get Call Type
 * @param handle - call handle
 * @return call type
 */
cc_call_type_t CCAPI_CallInfo_getCallType(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallType";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->type);
     return data->type;
  }

  return 0;
}

/**
 * get Called party name
 * @param handle - call handle
 * @return called party name
 */
cc_string_t CCAPI_CallInfo_getCalledPartyName(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCalledPartyName";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->cld_name);
     return data->cld_name;
  }

  return strlib_empty();
}

/**
 * get Called party number
 * @param handle - call handle
 * @return called party number
 */
cc_string_t CCAPI_CallInfo_getCalledPartyNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCalledPartyNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->cld_number);
     return data->cld_number;
  }

  return strlib_empty();
}

/**
 * get Calling party name
 * @param handle - call handle
 * @return calling party name
 */
cc_string_t CCAPI_CallInfo_getCallingPartyName(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallingPartyName";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->clg_name);
     return data->clg_name;
  }

  return strlib_empty();
}

/**
 * get Calling party number
 * @param handle - call handle
 * @return calling party number
 */
cc_string_t CCAPI_CallInfo_getCallingPartyNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallingPartyNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->clg_number);
     return data->clg_number;
  }

  return strlib_empty();
}

/**
 * get alternate number
 * @param handle - call handle
 * @return calling party number
 */
cc_string_t CCAPI_CallInfo_getAlternateNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getAlternateNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->alt_number);
     return data->alt_number;
  }

  return strlib_empty();
}

/**
 * get Original Called party name
 * @param handle - call handle
 * @return original called party name
 */
cc_string_t CCAPI_CallInfo_getOriginalCalledPartyName(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getOriginalCalledPartyName";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->orig_called_name);
     return data->orig_called_name;
  }

  return strlib_empty();
}

/**
 * get Original Called party number
 * @param handle - call handle
 * @return original called party number
 */
cc_string_t CCAPI_CallInfo_getOriginalCalledPartyNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getOriginalCalledPartyNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->orig_called_number);
     return data->orig_called_number;
  }

  return strlib_empty();
}

/**
 * get last redirecting party name
 * @param handle - call handle
 * @return last redirecting party name
 */
cc_string_t CCAPI_CallInfo_getLastRedirectingPartyName(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getLastRedirectingPartyName";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->last_redir_name);
     return data->last_redir_name;
  }

  return strlib_empty();
}

/**
 * get past redirecting party number
 * @param handle - call handle
 * @return last redirecting party number
 */
cc_string_t CCAPI_CallInfo_getLastRedirectingPartyNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getLastRedirectingPartyNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->last_redir_number);
     return data->last_redir_number;
  }

  return strlib_empty();
}

/**
 * get placed call party name
 * @param handle - call handle
 * @return placed party name
 */
cc_string_t CCAPI_CallInfo_getPlacedCallPartyName(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getPlacedCallPartyName";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->plcd_name);
     return data->plcd_name;
  }

  return strlib_empty();
}

/**
 * get placed call party number
 * @param handle - call handle
 * @return placed party number
 */
cc_string_t CCAPI_CallInfo_getPlacedCallPartyNumber(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getPlacedCallPartyNumber";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->plcd_number);
     return data->plcd_number;
  }

  return strlib_empty();
}


/**
 * get call instance number
 * @param handle - call handle
 * @return
 */
cc_int32_t CCAPI_CallInfo_getCallInstance(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getCallInstance";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->inst);
     return data->inst;
  }

  return 0;
}

/**
 * get call status prompt
 * @param handle - call handle
 * @return call status
 */
cc_string_t CCAPI_CallInfo_getStatus(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getStatus";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if (data && data->status){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->status);
     return data->status;
  }

  return strlib_empty();

}

/**
 * get call security
 * @param handle - call handle
 * @return call security status
 */
cc_call_security_t CCAPI_CallInfo_getSecurity(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getSecurity";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->security);
     return data->security;
  }

  return CC_SECURITY_NONE;
}

/**
 *  * get Call Selection Status
 *   * @param [in] handle - call info handle
 *    * @return cc_boolean - TRUE => selected
 *     */
cc_boolean CCAPI_CallInfo_getSelectionStatus(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getSelectionStatus";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->isSelected);
       return data->isSelected;
   }

   return FALSE;
}

/**
 * get call policy
 * @param handle - call handle
 * @return call policy
 */
cc_call_policy_t CCAPI_CallInfo_getPolicy(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getPolicy";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->policy);
     return data->policy;
  }

  return CC_POLICY_NONE;
}

/**
 * get GCID
 * @param handle - call handle
 * @return GCID
 */
cc_string_t CCAPI_CallInfo_getGCID(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getGCID";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->gci);
     return data->gci;
  }

  return strlib_empty();
}

/**
 * get ringer state.
 * @param handle - call handle
 * @return ringer state
 */
cc_boolean CCAPI_CallInfo_getRingerState(cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getRingerState";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->ringer_start);
     return data->ringer_start;
  }

  return FALSE;
}

/**
 * get ringer mode
 * @param handle - call handle
 * @return ringer mode
 */
int CCAPI_CallInfo_getRingerMode(cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getRingerMode";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->ringer_mode);
     return (int)(data->ringer_mode);
  }

  return -1;
}

/**
 * get ringer loop count
 * @param handle - call handle
 * @return once Vs continuous
 */
cc_boolean CCAPI_CallInfo_getIsRingOnce(cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getIsRingOnce";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->ringer_once);
     return (int)(data->ringer_once);
  }

  return TRUE;
}

/**
 * get onhook reason
 * @param handle - call handle
 * @return onhook reason
 */
cc_int32_t  CCAPI_CallInfo_getOnhookReason(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getOnhookReason";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->cause);
     return data->cause;
  }

  return CC_CAUSE_NORMAL;
}

/**
 * is Conference Call?
 * @param handle - call handle
 * @return boolean - is Conference
 */
cc_boolean  CCAPI_CallInfo_getIsConference(cc_callinfo_ref_t handle){
  session_data_t *data = (session_data_t *)handle;
  char isConf[32];

  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, __FUNCTION__));

  memset(isConf, 0, sizeof(isConf));

  if(platGetPhraseText(CONFERENCE_LOCALE_CODE, isConf, sizeof(isConf)) == CC_FAILURE){
	  return FALSE;
  }

  if( data != NULL){
      if( (strcasecmp(data->cld_name, isConf) == 0 && strcasecmp(data->cld_number, "") == 0) ||
          (strcasecmp(data->clg_name, isConf) == 0 && strcasecmp(data->clg_number, "") == 0) )
      {
	      return TRUE;
      }
  }

  return FALSE;
}

/**
 * getStream Statistics
 * @param handle - call handle
 * @return stream stats
 */
cc_return_t  CCAPI_CallInfo_getStreamStatistics(cc_callinfo_ref_t handle, cc_int32_t stats[], cc_int32_t *count)
{
  static const char *fname="CCAPI_CallInfo_getStreamStatistics";
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
  CCAPP_DEBUG(DEB_F_PREFIX"returned CC_SUCCESS (default)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
 // todo
 return CC_SUCCESS;
}


/**
 * has capability - is the feature allowed
 * @param handle - call handle
 * @param feat_id - feature id
 * @return boolean - is Allowed
 */
cc_boolean  CCAPI_CallInfo_hasCapability(cc_callinfo_ref_t handle, cc_int32_t feat_id){
  static const char *fname="CCAPI_CallInfo_hasCapability";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"feature id:  %d , value returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),feat_id,  data->allowed_features[feat_id]);
     return data->allowed_features[feat_id];
  }

  return FALSE;
}

/**
 * get Allowed Feature set
 * @param handle - call handle
 * @return boolean array that can be indexed using CCAPI_CALL_CAP_XXXX to check if feature is enabled
 */
cc_boolean  CCAPI_CallInfo_getCapabilitySet(cc_callinfo_ref_t handle, cc_int32_t feat_set[]){
  static const char *fname="CCAPI_CallInfo_getCapabilitySet";
  session_data_t *data = (session_data_t *)handle;
  int feat_id;

  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     for (feat_id = 0; feat_id < CCAPI_CALL_CAP_MAX; feat_id++) {
         feat_set[feat_id] = data->allowed_features[feat_id];
         CCAPP_DEBUG(DEB_F_PREFIX"feature id:  %d , value %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),feat_id,  feat_set[feat_id]);
     }

     CCAPP_DEBUG(DEB_F_PREFIX"returned CC_SUCCESS", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
     return CC_SUCCESS;
  }

  return CC_FAILURE;
}

/**
 * Call selection status
 * @param [in] handle - call handle
 * @return cc_boolean - selection status
 */
cc_boolean  CCAPI_CallInfo_isCallSelected(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_isCallSelected";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->isSelected);
     return data->isSelected;
  }

  return FALSE;
}

/**
 * Call negotiated video direction
 * @param [in] handle - call handle
 * @return cc_sdp_direction_t - video direction
 */
cc_sdp_direction_t  CCAPI_CallInfo_getVideoDirection(cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_getVideoDirection";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->vid_dir);
     return (data->vid_dir);
  }

  return CC_SDP_DIRECTION_INACTIVE;
}

/**
 * INFO Package for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - Info package header
 */
cc_string_t  CCAPI_CallInfo_getINFOPack (cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getINFOPackage";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->info_package);
     return data->info_package;
  }

  return strlib_empty();
}

/**
 * INFO type for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - content-type  header
 */
cc_string_t  CCAPI_CallInfo_getINFOType (cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getINFOType";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->info_type);
     return data->info_type;
  }

  return strlib_empty();
}

/**
 * INFO body for RECEIVED_INFO event
 * @param [in] handle - call info handle
 * @return cc_string_t - INFO body
 */
cc_string_t  CCAPI_CallInfo_getINFOBody (cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getINFOBody";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->info_body);
     return data->info_body;
  }

  return strlib_empty();
}

/**
 * Get the call log reference
 * @param [in] handle - call info handle
 * @return cc_string_t - INFO body
 * NOTE: Memory associated with the call log is tied to the cc_callinfo_ref_t handle
 * this would be freed when the callinfo ref is freed.
 */
cc_calllog_ref_t  CCAPI_CallInfo_getCallLogRef(cc_callinfo_ref_t handle)
{
  static const char *fname="CCAPI_CallInfo_getCallLogRef";
  session_data_t *data = (session_data_t *)handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( data != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"returned %p", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), &data->call_log);
     return &data->call_log;
  }

  return NULL;
}


/**
 * Returns the Audio mute state for this call
 * @return boolean true=muted false=not muted
 */
cc_boolean CCAPI_CallInfo_isAudioMuted (cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_isAudioMuted";
  session_data_t *data = (session_data_t *)handle;
  session_data_t * sess_data_p;

  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
  if ( data != NULL){
      sess_data_p = (session_data_t *)findhash(data->sess_id);
      if ( sess_data_p != NULL ) {
          CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sess_data_p->audio_mute);
          return sess_data_p->audio_mute;
      }
  }

  return FALSE;
}

/**
 * Returns the Video  mute state for this call
 * @return boolean true=muted false=not muted
 */
cc_boolean CCAPI_CallInfo_isVideoMuted (cc_callinfo_ref_t handle){
  static const char *fname="CCAPI_CallInfo_isVideoMuted";
  session_data_t *data = (session_data_t *)handle;
  session_data_t * sess_data_p;

  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
  if ( data != NULL){
      sess_data_p = (session_data_t *)findhash(data->sess_id);
      if ( sess_data_p != NULL ) {
          CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sess_data_p->video_mute);
          return sess_data_p->video_mute;
      }
  }

  return FALSE;
}


