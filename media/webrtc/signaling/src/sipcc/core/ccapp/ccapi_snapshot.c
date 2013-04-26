/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "string.h"
#include "string_lib.h"
#include "text_strings.h"
#include "ccapi_snapshot.h"
#include "ccapi_device.h"
#include "ccapi_device_listener.h"
#include "ccapi_line.h"
#include "ccapi_line_listener.h"
#include "ccapi_line_info.h"
#include "ccapi_call.h"
#include "ccapi_call_listener.h"
#include "CCProvider.h"
#include "capability_set.h"
#include "phone_debug.h"

cc_device_info_t g_deviceInfo;
accessory_cfg_info_t g_accessoryCfgInfo;
cc_line_info_t lineInfo[MAX_CONFIG_LINES+1];
cc_feature_info_t featureInfo[MAX_CONFIG_LINES+1];

static void printCallInfo(cc_callinfo_ref_t info, const char* fname);
static void printFeatureInfo (ccapi_device_event_e type, cc_featureinfo_ref_t feature_info, const char* fname);

cc_string_t lineLabels[MAX_CONFIG_LINES+1] = {0};


void ccsnap_set_line_label(int btn, cc_string_t label) {

   CCAPP_DEBUG(DEB_F_PREFIX"btn=%d label=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_set_line_label"), btn, label);
   if ( btn > 0 && btn <= MAX_CONFIG_LINES+1 ) {
       if ( label == NULL ) {
         label = strlib_empty();
       }
       if ( lineLabels[btn] == NULL ) {
         lineLabels[btn] = strlib_empty();
       }
       lineLabels[btn] = strlib_update(lineLabels[btn], label);
   }
}

cc_string_t ccsnap_get_line_label(int btn) {
   if ( btn > 0 && btn <= MAX_CONFIG_LINES+1 ) {
     return lineLabels[btn];
   }
   return strlib_empty();
}

/*
 * The below two functions are borrowed from CUCM/CUP as they both perform
 * identical functions.  That is, taking a DN 1555 and
 * a mask 919476XXXX to build a true external number 9194761555.
 */
static void stringInsert(char *string, int num, char ch)
{

   int len = strlen(string);
   int k, j;
   char tempString[100];
   sstrncpy(tempString, string, 100);

   for (k = 0; k < num; k++)
       string[k] = ch;

   for (j = 0; j < len; j++)
       string[k++] = tempString[j];

   string[k] = 0;

}

/*
 * Taken from CUCM/CUP code as they have done this already.
 */
cc_string_t CCAPI_ApplyTranslationMask (const char *ext, const char *mask)
{

    char translationMask[100] = {'\0'};
    char dn[100] = {'\0'};
    char translatedString[100] = {'\0'};
    cc_string_t result;
    unsigned int maskLen,
                 dnLen,
                 i, j = 0;

    if ((ext == NULL) || (mask == NULL)) {
        return NULL;
    }

    maskLen = strlen(mask);
    dnLen = strlen(ext);

    if ((dnLen == 0) || (maskLen == 0)) {
        CCAPP_DEBUG(DEB_F_PREFIX"CCAPI_ApplyTranslationMask DN or mask has len=0",
DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI_ApplyTranslationMask"));
        return NULL;
    }

    /* make sure there's enough space in the buffer to
     * hold the translated string.
     */
    if (dnLen + maskLen > 99) {
        CCAPP_DEBUG(DEB_F_PREFIX"CCAPI_ApplyTranslationMask length overflow", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI_ApplyTranslationMask"));
       return NULL;
    }

    sstrncpy(translationMask, mask, 100);
    sstrncpy(dn, ext, 100);

    /* make sure DN is numeric only */
    for (i=0; i< dnLen; i++) {
        if (isalpha(dn[i])) {
           return 0;
        }
    }

    if (maskLen > dnLen) {
       stringInsert(dn, maskLen - dnLen, '?');
    }

    /* if the digit string is longer than the translation mask
     * prepad the translation mask with '%'.
     */
    if (dnLen > maskLen) {
       stringInsert(translationMask, dnLen - maskLen, '%');
    }

    dnLen = strlen(dn);

    for (i=0; i < dnLen; i++) {
        if (translationMask[i] == '%')
            continue;
        else if (translationMask[i] == 'X')
            translatedString[j++] = dn[i];
        else
            translatedString[j++] = translationMask[i];
    }

    translatedString[j] = 0;
    result = strlib_malloc(translatedString, strlen(translatedString));
    return result;
}

/**
 * Before initing the line_info release any memory which has been used
 * so we do not leak any here.
 */
void ccsnap_line_pre_init () {
    int i;

    CCAPP_DEBUG(DEB_F_PREFIX"Entering line_pre_init to clear it out to avoid mem leaks", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_line_pre_init"));

    for (i=1;i<MAX_CONFIG_LINES;i++) {
        if ((lineInfo[i].name) && (strlen(lineInfo[i].name) > 0)) {
            strlib_free(lineInfo[i].name);
        }
        if ((lineInfo[i].dn) && (strlen(lineInfo[i].dn) > 0)) {
            strlib_free(lineInfo[i].dn);
        }
        if ((lineInfo[i].cfwd_dest) && (strlen(lineInfo[i].cfwd_dest) > 0)) {
            strlib_free(lineInfo[i].cfwd_dest);
        }
        if ((lineInfo[i].externalNumber) &&
            (strlen(lineInfo[i].externalNumber) > 0)) {
            strlib_free(lineInfo[i].externalNumber);
        }
        if ((featureInfo[i].speedDialNumber) &&
            (strlen(featureInfo[i].speedDialNumber) > 0)) {
            strlib_free(featureInfo[i].speedDialNumber);
        }
        if ((featureInfo[i].contact) && (strlen(featureInfo[i].contact) > 0)) {
            strlib_free(featureInfo[i].contact);
        }
        if ((featureInfo[i].name) && (strlen(featureInfo[i].name) > 0)) {
            strlib_free(featureInfo[i].name);
        }
        if ((featureInfo[i].retrievalPrefix) &&
            (strlen(featureInfo[i].retrievalPrefix) > 0)) {
            strlib_free(featureInfo[i].retrievalPrefix);
        }
    }
}

/**
 * Initialize lineinfo and featureinfo arrays
 */
void ccsnap_line_init() {
   int i;
   cc_uint32_t tmpInt;
   char tempStr[MAX_URL_LENGTH];
   char maskStr[MAX_EXTERNAL_NUMBER_MASK_SIZE];

   /* clean up structure if need be */
   ccsnap_line_pre_init();

   memset(lineInfo, 0, MAX_CONFIG_LINES*sizeof(cc_line_info_t));
   memset(featureInfo, 0, MAX_CONFIG_LINES*sizeof(cc_feature_info_t));
   for (i=1;i<=MAX_CONFIG_LINES;i++) {
      config_get_line_value(CFGID_LINE_FEATURE, &tmpInt, sizeof(tmpInt), i);
      if ( tmpInt == cfgLineFeatureDN ) {
          lineInfo[i].button = i;
          lineInfo[i].line_type = tmpInt;
          config_get_line_value(CFGID_LINE_INDEX,  &tmpInt, sizeof(tmpInt), i);
          lineInfo[i].line_id = tmpInt;
          config_get_line_value(CFGID_LINE_DISPLAYNAME_STRING, tempStr,
                          MAX_URL_LENGTH, i);
          lineInfo[i].dn = strlib_malloc(tempStr, strlen(tempStr));
          config_get_line_value(CFGID_LINE_NAME_STRING, tempStr,
                          MAX_URL_LENGTH, i);
          lineInfo[i].name = strlib_malloc(tempStr, strlen(tempStr));
          config_get_line_value(CFGID_LINE_CFWDALL, tempStr,
                          MAX_URL_LENGTH, i);
          lineInfo[i].cfwd_dest = strlib_malloc(tempStr, strlen(tempStr));
          config_get_line_value(CFGID_LINE_SPEEDDIAL_NUMBER_STRING, tempStr,
                          MAX_URL_LENGTH, i);
          memset(maskStr, 0, sizeof(maskStr));
          config_get_string(CFGID_CCM_EXTERNAL_NUMBER_MASK, maskStr, MAX_EXTERNAL_NUMBER_MASK_SIZE);
          if (strlen(maskStr) > 0) {
              lineInfo[i].externalNumber = CCAPI_ApplyTranslationMask(lineInfo[i].name, maskStr);
              CCAPP_DEBUG("Setting lineInfo[i].externalNumber to %s", lineInfo[i].externalNumber);
          } else {
              lineInfo[i].externalNumber = strlib_empty();
          }
      } else {
          lineInfo[i].line_id = MAX_CONFIG_LINES+1; // invalid line id
          lineInfo[i].button = i;
          lineInfo[i].dn = strlib_empty();
          lineInfo[i].name = strlib_empty();
          lineInfo[i].cfwd_dest = strlib_empty();
          lineInfo[i].externalNumber = strlib_empty();
      }
      capset_get_idleset(CC_MODE_CCM, lineInfo[i].allowed_features);

      // get feature again because it might have been changed if it is a DN
      // and the tmpInt might have a different value
      config_get_line_value(CFGID_LINE_FEATURE, &tmpInt, sizeof(tmpInt), i);

      // features which have no properties
      if ( tmpInt == cfgLineFeatureAllCalls ||
    		  tmpInt == cfgLineFeatureMaliciousCallID ||
    		  tmpInt == cfgLineFeatureRedial || tmpInt == cfgLineFeatureAnswerOldest || tmpInt == cfgLineFeatureServices ) {
    	  featureInfo[i].feature_id = tmpInt;
    	  featureInfo[i].button = i;
          featureInfo[i].speedDialNumber = strlib_empty();
          featureInfo[i].contact = strlib_empty();
          featureInfo[i].name = strlib_empty();
          featureInfo[i].retrievalPrefix = strlib_empty();
          featureInfo[i].featureOptionMask = 0;
      } else if ( tmpInt == cfgLineFeatureSpeedDialBLF || tmpInt == cfgLineFeatureSpeedDial){
    	  featureInfo[i].feature_id = tmpInt;
    	  featureInfo[i].button = i;
          config_get_line_value(CFGID_LINE_SPEEDDIAL_NUMBER_STRING, tempStr,
                          MAX_URL_LENGTH, i);
          featureInfo[i].speedDialNumber = strlib_malloc(tempStr, strlen(tempStr));
          featureInfo[i].contact = strlib_empty();
          config_get_line_value(CFGID_LINE_NAME_STRING, tempStr,
                          MAX_URL_LENGTH, i);
          featureInfo[i].name = strlib_malloc(tempStr, strlen(tempStr));
          featureInfo[i].retrievalPrefix = strlib_empty();
          config_get_line_value(CFGID_LINE_FEATURE_OPTION_MASK,  &tmpInt, sizeof(tmpInt), i);
          featureInfo[i].featureOptionMask = tmpInt;
          featureInfo[i].blf_state = CC_SIP_BLF_UNKNOWN;
      } else {
          featureInfo[i].feature_id = 0;
          featureInfo[i].button = MAX_CONFIG_LINES+1; // invalid button value
          featureInfo[i].speedDialNumber = strlib_empty();
          featureInfo[i].contact = strlib_empty();
          featureInfo[i].name = strlib_empty();
          featureInfo[i].retrievalPrefix = strlib_empty();
          featureInfo[i].featureOptionMask = 0;
      }
   }
}

cc_line_info_t* ccsnap_getLineInfo(int lineID)
{
     int i;
     cc_lineid_t line = (cc_lineid_t)lineID;

     for (i=1;i<=MAX_CONFIG_LINES;i++) {
        if ( lineInfo[i].line_id == line ) {
            return &lineInfo[i];
        }
     }

     return NULL;
}

cc_line_info_t* ccsnap_getLineInfoFromBtn(int btnID)
{
     int i;

     for (i=1;i<=MAX_CONFIG_LINES;i++) {
        if ( lineInfo[i].button == btnID ) {
            return &lineInfo[i];
        }
     }

     return NULL;
}

cc_boolean allowedFeature(int fid){
    return TRUE;
}

cc_feature_info_t* ccsnap_getFeatureInfo(int featureIndex)
{
	if ( ( featureIndex<=MAX_CONFIG_LINES ) &&
			( featureIndex>= 1 ) &&
		( featureInfo[featureIndex].button == featureIndex ) ) {
            if ( allowedFeature(featureInfo[featureIndex].feature_id) ){
		return &featureInfo[featureIndex];
            }
	}

     return NULL;
}

/**
 * Release any used mem to avoid a leak.
 */
void ccsnap_device_pre_init () {
    int i = 0;

    CCAPP_DEBUG(DEB_F_PREFIX"Entering device_pre_init to clear it out to avoid mem leaks", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_device_pre_init"));
    if ((g_deviceInfo.not_prompt) && (strlen(g_deviceInfo.not_prompt) > 0)) {
        strlib_free(g_deviceInfo.not_prompt);
    }

    i = 0;
    while (i < CCAPI_MAX_SERVERS) {
        if ((g_deviceInfo.ucm[i].name) &&
            (strlen(g_deviceInfo.ucm[i].name) > 0)) {
            strlib_free(g_deviceInfo.ucm[i].name);
        }
        i++;
    }
}

void ccsnap_device_init() {
   char temp[MAX_SIP_URL_LENGTH];

   /* clean up structure if need be */
   ccsnap_device_pre_init();

   memset (&g_deviceInfo, 0, sizeof(g_deviceInfo));
   g_deviceInfo.not_prompt =strlib_empty();

   g_deviceInfo.not_prompt_prio = 0;
   g_deviceInfo.not_prompt_prog = 0;
   g_deviceInfo.mwi_lamp = FALSE;
   g_deviceInfo.cucm_mode = CC_MODE_CCM;
   g_deviceInfo.ins_state = CC_STATE_IDLE;
   g_deviceInfo.ins_cause = CC_CAUSE_NONE;
   g_deviceInfo.reg_time  = 0;

   config_get_string(CFGID_CCM1_ADDRESS, temp, MAX_SIP_URL_LENGTH);
   g_deviceInfo.ucm[0].name = strlib_malloc(temp, strlen(temp));
   g_deviceInfo.ucm[0].type = CC_MODE_CCM;
   g_deviceInfo.ucm[0].status = CC_CCM_STATUS_NONE;

   config_get_string(CFGID_CCM2_ADDRESS, temp, MAX_SIP_URL_LENGTH);
   g_deviceInfo.ucm[1].name = strlib_malloc(temp, strlen(temp));
   g_deviceInfo.ucm[1].type = CC_MODE_CCM;
   g_deviceInfo.ucm[1].status = CC_CCM_STATUS_NONE;

   config_get_string(CFGID_CCM3_ADDRESS, temp, MAX_SIP_URL_LENGTH);
   g_deviceInfo.ucm[2].name = strlib_malloc(temp, strlen(temp));
   g_deviceInfo.ucm[2].type = CC_MODE_CCM;
   g_deviceInfo.ucm[2].status = CC_CCM_STATUS_NONE;

   config_get_string(CFGID_CCM_TFTP_IP_ADDR, temp, MAX_SIP_URL_LENGTH);
   g_deviceInfo.ucm[3].name = strlib_malloc(temp, strlen(temp));
   g_deviceInfo.ucm[3].type = CC_MODE_CCM;
   g_deviceInfo.ucm[3].status = CC_CCM_STATUS_NONE;

   g_accessoryCfgInfo.camera = ACCSRY_CFGD_CFG;
   g_accessoryCfgInfo.video = ACCSRY_CFGD_CFG;
}

void ccsnap_gen_deviceEvent(ccapi_device_event_e event, cc_device_handle_t handle){
    const char* fname = "ccsnap_gen_deviceEvent";

    cc_device_info_t *device_info = CCAPI_Device_getDeviceInfo(handle);
    if ( device_info != NULL ) {
        CCAPP_DEBUG(DEB_F_PREFIX"data->ref_count=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->ref_count);

        switch (event) {
            case CCAPI_DEVICE_EV_NOTIFYPROMPT:
              CCAPP_DEBUG(DEB_F_PREFIX"data->not_prompt=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->not_prompt);
              CCAPP_DEBUG(DEB_F_PREFIX"data->not_prompt_prio=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->not_prompt_prio);
              CCAPP_DEBUG(DEB_F_PREFIX"data->not_prompt_prog=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->not_prompt_prog);
              break;
            case CCAPI_DEVICE_EV_STATE:
              CCAPP_DEBUG(DEB_F_PREFIX"setting property %s to %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), "FullyRegistered", ((device_info->ins_state == CC_STATE_INS) ? "1" : "0"));
              //intentional follow through to let the debugs get printed.
            default:
              CCAPP_DEBUG(DEB_F_PREFIX"data->mwi_lamp=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->mwi_lamp);
              CCAPP_DEBUG(DEB_F_PREFIX"data->ins_state=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->ins_state);
              CCAPP_DEBUG(DEB_F_PREFIX"data->cucm_mode=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->cucm_mode);
              CCAPP_DEBUG(DEB_F_PREFIX"data->ins_cause=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device_info->ins_cause);
              break;

        }

        CCAPI_DeviceListener_onDeviceEvent(event, handle, device_info);
    }
    CCAPI_Device_releaseDeviceInfo(device_info);
}

void ccsnap_gen_lineEvent(ccapi_line_event_e event, cc_lineid_t handle){
    const char* fname = "ccsnap_gen_lineEvent";
    cc_line_info_t *line_info = CCAPI_Line_getLineInfo(handle);

    if ( line_info != NULL ) {
        if (g_CCAppDebug) {
            CCAPP_DEBUG(DEB_F_PREFIX"data->ref_count=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->ref_count);
            CCAPP_DEBUG(DEB_F_PREFIX"data->line_id=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->line_id);
            CCAPP_DEBUG(DEB_F_PREFIX"data->button=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->button);
            CCAPP_DEBUG(DEB_F_PREFIX"data->reg_state=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->reg_state);
            CCAPP_DEBUG(DEB_F_PREFIX"data->isCFWD=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->isCFWD);
            CCAPP_DEBUG(DEB_F_PREFIX"data->isLocalCFWD=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->isLocalCFWD);
            CCAPP_DEBUG(DEB_F_PREFIX"data->mwi=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->mwi.status);
            CCAPP_DEBUG(DEB_F_PREFIX"data->name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->name);
            CCAPP_DEBUG(DEB_F_PREFIX"data->dn=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->dn);
            CCAPP_DEBUG(DEB_F_PREFIX"data->cfwd_dest=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), line_info->cfwd_dest);
        }
        CCAPI_LineListener_onLineEvent(event, handle, line_info);
    }
    CCAPI_Line_releaseLineInfo(line_info);
}

void ccsnap_gen_callEvent(ccapi_call_event_e event, cc_call_handle_t handle){

    session_data_t *call_info = CCAPI_Call_getCallInfo(handle);

    if ( call_info == NULL ) {
        call_info = getDeepCopyOfSessionData(NULL);
    }

    //print all info
    if (g_CCAppDebug) {
        printCallInfo(call_info, "ccsnap_gen_callEvent");
    }

    CCAPI_CallListener_onCallEvent(event, handle, call_info);
    CCAPI_Call_releaseCallInfo(call_info);
}

void ccsnap_update_ccm_status(cc_string_t addr, cc_ccm_status_t status)
{
    int i;

    CCAPP_DEBUG(DEB_F_PREFIX"entry ccm %s status=%d",
    		DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_update_ccm_status"), addr, status);

    for (i=0;i< CCAPI_MAX_SERVERS;i++) {
        if ( g_deviceInfo.ucm[i].status == status ) {
            //move the status to the new addr
            g_deviceInfo.ucm[i].status = CC_CCM_STATUS_NONE;
        }
        if ( !strcmp(addr, g_deviceInfo.ucm[i].name) ) {
           g_deviceInfo.ucm[i].status = status;
           CCAPP_DEBUG(DEB_F_PREFIX"server %s is now status=%d",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_update_ccm_status"),
            g_deviceInfo.ucm[i].name, status);
        }
    }
}

void ccsnap_handle_mnc_reached (cc_line_info_t *line_info, cc_boolean mnc_reached, cc_cucm_mode_t mode)
{
    cc_call_handle_t handles[MAX_CALLS];
    int count = MAX_CALLS, i;
    session_data_t *cinfo;

    if (mnc_reached) {
 	line_info->allowed_features[CCAPI_CALL_CAP_NEWCALL] = FALSE;
        line_info->allowed_features[CCAPI_CALL_CAP_REDIAL] = FALSE;
        line_info->allowed_features[CCAPI_CALL_CAP_CALLFWD] = FALSE;
    } else {
         capset_get_idleset(mode, line_info->allowed_features);
    }

    // update connected calls caps on this line
    CCAPI_LineInfo_getCallsByState(line_info->line_id, CONNECTED, handles, &count);
    for ( i=0; i<count; i++) {
	cinfo = CCAPI_Call_getCallInfo(handles[i]);
	if (cinfo) {
	    if ( cinfo->attr == (cc_call_attr_t) CONF_CONSULT ||
	         cinfo->attr == (cc_call_attr_t) XFR_CONSULT ) {
	       CCAPI_Call_releaseCallInfo(cinfo);
	       continue;
	    }
            cinfo->allowed_features[CCAPI_CALL_CAP_TRANSFER] = mnc_reached?FALSE:TRUE;
            cinfo->allowed_features[CCAPI_CALL_CAP_CONFERENCE] = mnc_reached?FALSE:TRUE;
            //print call info
            if (g_CCAppDebug) {
                printCallInfo(cinfo, "ccsnap_handle_mnc_reached");
            }
            CCAPI_CallListener_onCallEvent(CCAPI_CALL_EV_CAPABILITY, handles[i], cinfo);
	}
    }
    // update RIU call caps on this line
    CCAPI_LineInfo_getCallsByState(line_info->line_id, REMINUSE, handles, &count);
    for ( i=0; i<count; i++) {
        cinfo = CCAPI_Call_getCallInfo(handles[i]);
        if (cinfo) {
            cinfo->allowed_features[CCAPI_CALL_CAP_BARGE] = mnc_reached?FALSE:TRUE;
            //print call info
            if (g_CCAppDebug) {
                printCallInfo(cinfo, "ccsnap_handle_mnc_reached");
            }
            CCAPI_CallListener_onCallEvent(CCAPI_CALL_EV_CAPABILITY, handles[i], cinfo);
        }
    }
}

void ccsnap_gen_blfFeatureEvent(cc_blf_state_t state, int appId)
{
    cc_feature_info_t *feature_info = NULL;

    feature_info = ccsnap_getFeatureInfo(appId);

    // if the feature exists
    if (feature_info != NULL) {
        feature_info->blf_state = state;
        printFeatureInfo(CCAPI_DEVICE_EV_BLF, feature_info, "ccsnap_gen_blfFeatureEvent");
        CCAPI_DeviceListener_onFeatureEvent(CCAPI_DEVICE_EV_BLF, CC_DEVICE_ID, feature_info);
    }
}

/**
 * Inserts localized strings into existing strings with escape characters.
 * @param destination the return phrase holder
 * @param source the phrase with escape characters.
 * @param len the input length to cap the maximum value
 * @return pointer to the new string
 */
cc_string_t ccsnap_EscapeStrToLocaleStr(cc_string_t destination, cc_string_t source, int len)
{
	static const char *fname="ccsnap_EscapeStrToLocaleStr";
	char phrase_collector[MAX_LOCALE_STRING_LEN] = { '\0' };
	char* phrase_collector_ptr = phrase_collector;
	char* esc_string_itr = (char*)source;
	int remaining_length = 0;
	cc_string_t ret_str = strlib_empty();

	if(destination == NULL){
		CCAPP_DEBUG(DEB_F_PREFIX"Error: destination is NULL", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
		return NULL;
	}

	if(source == NULL){
		CCAPP_DEBUG(DEB_F_PREFIX"Error: source is NULL", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
		strlib_free(destination);
		return strlib_empty();
	}

	if(source[0] == '\0'){
		strlib_free(destination);
		return strlib_empty();
	}

	if (len == LEN_UNKNOWN) {
		len = strlen(source) + MAX_LOCALE_PHRASE_LEN;
	}

	if (len <= 0){
		CCAPP_DEBUG(DEB_F_PREFIX"Error: cannot write string of length <= 0", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
		strlib_free(destination);
		return strlib_empty();
	}

	if (len > MAX_LOCALE_STRING_LEN){
		len = MAX_LOCALE_STRING_LEN;
	}

	remaining_length = len;
	while(  *esc_string_itr != NUL    &&
			remaining_length > 0     &&
			strlen(phrase_collector_ptr) < (size_t)(len-1))
	{
		int rtn = CC_SUCCESS;
		int phrase_index = 0;
                char* phrase_bucket_ptr = (char*)cpr_malloc(remaining_length * sizeof(char));

                if (phrase_bucket_ptr == NULL) {
                    CCAPP_ERROR(DEB_F_PREFIX"Error: phrase_bucket_ptr is NULL", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
                    strlib_free(destination);
                    return NULL;
                }
                phrase_bucket_ptr[0] = '\0';
		switch(*esc_string_itr){
		case OLD_CUCM_DICTIONARY_ESCAPE_TAG:
			phrase_index += CALL_CONTROL_PHRASE_OFFSET;
			// Do not set break to combine common code
		case NEW_CUCM_DICTIONARY_ESCAPE_TAG:
			esc_string_itr++;
			phrase_index += (int)(*esc_string_itr);
			rtn = platGetPhraseText(phrase_index, phrase_bucket_ptr, remaining_length-1);
			if(rtn == CC_FAILURE) break;
			sstrncat(phrase_collector_ptr, (cc_string_t)phrase_bucket_ptr, remaining_length);
			remaining_length--;
			break;
		default:
			// We need length 2 to concat 1 char and a terminating char
			sstrncat(phrase_collector_ptr, esc_string_itr, 1 + sizeof(char));
			remaining_length--;
			break;
		}
		esc_string_itr++;
                cpr_free(phrase_bucket_ptr);
	}

    ret_str = strlib_malloc(phrase_collector_ptr, len);

    if (!ret_str) {
        /*
         * If a malloc error occurred, give them back what they had.
         * It's not right, but it's better than nothing.
         */
        ret_str = destination;
    } else {
        strlib_free(destination);
    }

    CCAPP_DEBUG(DEB_F_PREFIX"Localization String returning %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ret_str);
    return (ret_str);
}

static boolean missed, placed, received;
void ccsnap_set_phone_services_provisioning(boolean misd, boolean plcd, boolean rcvd) {
   CCAPP_ERROR(DEB_F_PREFIX"missed=%d placed=%d received=%d",
    		DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccsnap_set_phone_services_provisioning"), misd, plcd, rcvd);
   missed = misd;
   placed = plcd;
   received = rcvd;
}

boolean ccsnap_isMissedCallLoggingEnabled()
{
   return missed;
}

boolean ccsnap_isReceivedCallLoggingEnabled()
{
   return received;
}

boolean ccsnap_isPlacedCallLoggingEnabled()
{
   return placed;
}

/**
 * Helper method
 */

static void printCallInfo(cc_callinfo_ref_t info, const char* fname) {
    CCAPP_DEBUG(DEB_F_PREFIX"data->ref_count=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->ref_count);
    CCAPP_DEBUG(DEB_F_PREFIX"data->sess_id=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->sess_id);
    CCAPP_DEBUG(DEB_F_PREFIX"data->line=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->line);
    CCAPP_DEBUG(DEB_F_PREFIX"data->id=%u", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->id);
    CCAPP_DEBUG(DEB_F_PREFIX"data->inst=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->inst);
    CCAPP_DEBUG(DEB_F_PREFIX"data->state=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->state);
    CCAPP_DEBUG(DEB_F_PREFIX"data->attr=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->attr);
    CCAPP_DEBUG(DEB_F_PREFIX"data->type=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->type);
    CCAPP_DEBUG(DEB_F_PREFIX"data->security=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->security);
    CCAPP_DEBUG(DEB_F_PREFIX"data->policy=%02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->policy);
    CCAPP_DEBUG(DEB_F_PREFIX"data->isSelected=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->isSelected);
    CCAPP_DEBUG(DEB_F_PREFIX"data->log_disp=%u", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->log_disp);
    CCAPP_DEBUG(DEB_F_PREFIX"data->clg_name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->clg_name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->clg_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->clg_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->alt_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->alt_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->cld_name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->cld_name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->cld_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->cld_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->orig_called_name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->orig_called_name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->orig_called_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->orig_called_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->last_redir_name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->last_redir_name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->last_redir_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->last_redir_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->plcd_name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->plcd_name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->plcd_number=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->plcd_number);
    CCAPP_DEBUG(DEB_F_PREFIX"data->status=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->status);
    CCAPP_DEBUG(DEB_F_PREFIX"data->gci=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->gci);
    CCAPP_DEBUG(DEB_F_PREFIX"data->cause=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->cause);
    CCAPP_DEBUG(DEB_F_PREFIX"data->vid_dir=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->vid_dir);
    CCAPP_DEBUG(DEB_F_PREFIX"data->vid_offer=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->vid_offer);
    CCAPP_DEBUG(DEB_F_PREFIX"data->is_conf=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->is_conf);
    CCAPP_DEBUG(DEB_F_PREFIX"data->ringer_start=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->ringer_start);
    CCAPP_DEBUG(DEB_F_PREFIX"data->ringer_mode=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->ringer_mode);
    CCAPP_DEBUG(DEB_F_PREFIX"data->ringer_once=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->ringer_once);
}

static void printFeatureInfo (ccapi_device_event_e type, cc_featureinfo_ref_t feature_info, const char* fname) {
    CCAPP_DEBUG(DEB_F_PREFIX"data->button=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->button);
    CCAPP_DEBUG(DEB_F_PREFIX"data->contact=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->contact);
    CCAPP_DEBUG(DEB_F_PREFIX"data->featureOptionMask=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->featureOptionMask);
    CCAPP_DEBUG(DEB_F_PREFIX"data->feature_id=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->feature_id);
    CCAPP_DEBUG(DEB_F_PREFIX"data->name=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->name);
    CCAPP_DEBUG(DEB_F_PREFIX"data->retrievalPrefix=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->retrievalPrefix);
    CCAPP_DEBUG(DEB_F_PREFIX"data->speedDialNumber=%s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->speedDialNumber);
    if (type == CCAPI_DEVICE_EV_BLF) {
        CCAPP_DEBUG(DEB_F_PREFIX"data->blf_state=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), feature_info->blf_state);
    }

}
