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

#include "ccapi_snapshot.h"
#include "ccapi_line.h"
#include "sessionHash.h"
#include "CCProvider.h"
#include "phone_debug.h"

/**
 * Get the line ID
 * @param line - line reference handle
 * @return line ID
 */
cc_int32_t CCAPI_lineInfo_getID(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getID";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
   
   /* We will use button as line ID */
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->button);
       return info->button;
   }
   return -1;
}

/**
 * Get the line Name
 * @param line - line reference handle
 * @return cc_string_t - handle of the call created
 */
cc_string_t CCAPI_lineInfo_getName(cc_lineinfo_ref_t line) {
   static const char *fname="CCAPI_lineInfo_getName";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->dn);
       return info->dn;
   }
   return NULL;
}

/**
 * Get the line Label
 * @param [in] line - line reference handle
 * @return cc_string_t - line Label
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getLabel(cc_lineinfo_ref_t line) {
   static const char *fname="CCAPI_lineInfo_getLabel";
   cc_line_info_t  *info = (cc_line_info_t *) line;
   cc_string_t label = strlib_empty();

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       label = ccsnap_get_line_label(info->button);
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), label);
   }
   return label;
}

/**
 * Get the line DN Number
 * @param line - line reference handle
 * @return cc_string_t - handle of the call created
 */
cc_string_t CCAPI_lineInfo_getNumber(cc_lineinfo_ref_t line) {
   static const char *fname="CCAPI_lineInfo_getNumber";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->name);
       return info->name;
   }
   return NULL;
}

/**
 * Get the line External Number
 * @param line - line reference handle
 * @return cc_string_t - handle of the call created
 */
cc_string_t CCAPI_lineInfo_getExternalNumber(cc_lineinfo_ref_t line) {
   static const char *fname="CCAPI_lineInfo_getExternalNumber";
   cc_line_info_t  *info = (cc_line_info_t *) line;
   char externalNumberMask[MAX_EXTERNAL_NUMBER_MASK_SIZE];
   memset(externalNumberMask, 0, sizeof(externalNumberMask));

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   config_get_string(CFGID_CCM_EXTERNAL_NUMBER_MASK, externalNumberMask, MAX_EXTERNAL_NUMBER_MASK_SIZE);
   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->name);
       if (strlen(externalNumberMask) > 0) {
           CCAPP_DEBUG(DEB_F_PREFIX"number with mask applied == %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->externalNumber);
           return info->externalNumber;
       } else {
           CCAPP_DEBUG(DEB_F_PREFIX"number without mask == %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->name);
           return info->name;
       }
   }
   return NULL;
}

/**
 * Get the physical button number on which this line is configured
 * @param line - line reference handle
 * @return cc_uint32_t - button number
 */
cc_uint32_t CCAPI_lineInfo_getButton(cc_lineinfo_ref_t line){
   static const char *fname="CCAPI_lineInfo_getButton";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->button);
       return info->button;
   }
   return 0;

}

/**
 * Get the Line Type
 * @param [in] line - line reference handle
 * @return cc_uint32_t - line featureID ( Line )
 */
cc_line_feature_t CCAPI_lineInfo_getLineType(cc_lineinfo_ref_t line){
   static const char *fname="CCAPI_lineInfo_getLineType";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->line_type);
       return info->line_type;
   }
   return 0;
}



/**
 * Get the CFWDAll status for the line
 * @param line - line reference handle
 * @return cc_boolean - isForwarded
 */
cc_boolean CCAPI_lineInfo_isCFWDActive(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_isCFWDActive";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->isCFWD);
       return info->isCFWD;
   }
   return FALSE;
}

/**
 * Get the CFWDAll destination
 * @param line - line reference handle
 * @return cc_string_t - cfwd target
 */
cc_string_t CCAPI_lineInfo_getCFWDName(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getCFWDName";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->cfwd_dest);
       return info->cfwd_dest;
   }
   return NULL;
}

/**
 * Get the MWI Status
 * @param line - line reference handle
 * @return cc_uint32_t - MWI status (boolean 0 => no MWI)
 */
cc_uint32_t CCAPI_lineInfo_getMWIStatus(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWIStatus";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d, status %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.status);
       return info->mwi.status;
   }
   return 0;
}

/**
 * Get the MWI Type
 * @param line - line reference handle
 * @return cc_uint32_t - MWI Type
 */
cc_uint32_t CCAPI_lineInfo_getMWIType(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWIType";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d, type %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.type);
       return info->mwi.type;
   }
   return 0;
}

/**
 * Get the MWI new msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI new msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWINewMsgCount(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWINewMsgCount";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d, new count %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.new_count);
       return info->mwi.new_count;
   }
   return 0;
}
/**
 * Get the MWI old msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI old msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIOldMsgCount(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWIOldMsgCount";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d, old_count %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.old_count);
       return info->mwi.old_count;
   }
   return 0;
}

/**
 * Get the MWI high priority new msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI new msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIPrioNewMsgCount(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWIPrioNewMsgCount";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d , pri_new count %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.pri_new_count);
       return info->mwi.pri_new_count;
   }
   return 0;
}
/**
 * Get the MWI high priority old msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI old msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIPrioOldMsgCount(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getMWIPrioOldMsgCount";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d, pri old_count %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->mwi, info->mwi.pri_old_count);
       return info->mwi.pri_old_count;
   }
   return 0;
}

/**
 * Get calls on line
 * @param [in] line - lineID
 * @param [out] callref[] - Array of callinfo references
 * @param [in/out] count - count of call references populated
 * @return void
 */

void CCAPI_LineInfo_getCalls(cc_lineid_t line, cc_call_handle_t handles[], int *count)
{
    static const char *fname="CCAPI_Line_getCalls";
    hashItr_t itr;
    session_data_t *data;
    int i=0;

    CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL &&
              i<*count ) {
         if ( GET_LINE_ID(data->sess_id) == line ){
            handles[i++] = CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id);
         }
    }
    *count=i;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return) \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * Get calls on line
 * @param [in] line - lineID
 * @param [out] callref[] - Array of callinfo references
 * @param [in/out] count - count of call references populated
 * @return void
 */
void CCAPI_LineInfo_getCallsByState(cc_lineid_t line, cc_call_state_t state,
                 cc_call_handle_t handles[], int *count)
{
    static const char *fname="CCAPI_Line_getCallsByState";
    hashItr_t itr;
    session_data_t *data;
    int i=0;

    CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL &&
              i<*count ) {
         if ( GET_LINE_ID(data->sess_id) == line && data->state ==state ){
            handles[i++] = CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id);
         }
    }
    *count=i;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return) \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * Get the physical button number on which this line is configured
 * @param [in] line - line reference handle
 * @return cc_uint32_t - button number
 */
cc_boolean CCAPI_lineInfo_getRegState(cc_lineinfo_ref_t line)
{
   static const char *fname="CCAPI_lineInfo_getRegState";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), info->reg_state);
       return info->reg_state;
   }
   return 0;
}

/**
 * has capability - is the feature allowed
 * @param [in] line - line reference handle
 * @param [in] feat_id - feature id
 * @return boolean - is Allowed
 */
cc_boolean  CCAPI_LineInfo_hasCapability (cc_lineinfo_ref_t line, cc_int32_t feat_id){
   static const char *fname="CCAPI_LineInfo_hasCapability";
   cc_line_info_t  *info = (cc_line_info_t *) line;

   CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

   if ( info != NULL){
     CCAPP_DEBUG(DEB_F_PREFIX"feature id:  %d , value returned %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),feat_id,  info->allowed_features[feat_id]);
      return info->allowed_features[feat_id];
    }

    return FALSE;
}


/**
 * get Allowed Feature set
 * @param [in] line - line reference handle
 * @param [in,out] feat_set - array of len CC_CALL_CAP_MAX 
 * @return cc_return_t - CC_SUCCESS or CC_FAILURE
 */
cc_return_t  CCAPI_LineInfo_getCapabilitySet (cc_lineinfo_ref_t line, cc_int32_t feat_set[]){
    static const char *fname="CCAPI_LineInfo_getCapabilitySet";
   cc_line_info_t  *info = (cc_line_info_t *) line;
    int feat_id;
	         
    CCAPP_DEBUG(DEB_F_PREFIX"Entering\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if ( info != NULL){
       for (feat_id = 0; feat_id < CCAPI_CALL_CAP_MAX; feat_id++) {
         feat_set[feat_id] = info->allowed_features[feat_id];
         CCAPP_DEBUG(DEB_F_PREFIX"feature id:  %d , value %d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),feat_id,  feat_set[feat_id]);
       }
			            
       CCAPP_DEBUG(DEB_F_PREFIX"returned CC_SUCCESS\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
       return CC_SUCCESS;
    }

    return CC_FAILURE;
}


