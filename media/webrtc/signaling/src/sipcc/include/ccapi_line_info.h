/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_LINE_INFO_H_
#define _CCAPI_LINE_INFO_H_

#include "ccapi_types.h"

/**
 * Get the line ID
 * @param [in] line - line reference handle
 * @return cc_call_handle_t - handle of the call created
 */
cc_uint32_t CCAPI_lineInfo_getID(cc_lineinfo_ref_t line);

/**
 * Get the line Name
 * @param [in] line - line reference handle
 * @return cc_string_t - line Name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getName(cc_lineinfo_ref_t line);

/**
 * Get the line Label
 * @param [in] line - line reference handle
 * @return cc_string_t - line Label
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getLabel(cc_lineinfo_ref_t line);

/**
 * Get the line DN Number
 * @param [in] line - line reference handle
 * @return cc_string_t - line DN
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getNumber(cc_lineinfo_ref_t line);

/**
 * Get the line External Number
 * @param [in] line - line reference handle
 * @return cc_string_t - line DN
 * NOTE: The memory for return string doesn't need to be freed it will be freedwhen the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getExternalNumber(cc_lineinfo_ref_t line);

/**
 * Get the physical button number on which this line is configured
 * @param [in] line - line reference handle
 * @return cc_uint32_t - button number
 * NOTE: This API is deprecated please don't use this. the CCAPI_lineInfo_getID() returns the button number
 * as we use the button as line id
 */
cc_uint32_t CCAPI_lineInfo_getButton(cc_lineinfo_ref_t line);

/**
 * Get the Line Type
 * @param [in] line - line reference handle
 * @return cc_uint32_t - line featureID ( Line )
 */
cc_line_feature_t CCAPI_lineInfo_getLineType(cc_lineinfo_ref_t line);

/**
 * Get the physical button number on which this line is configured
 * @param [in] line - line reference handle
 * @return cc_uint32_t - button number
 */
cc_boolean CCAPI_lineInfo_getRegState(cc_lineinfo_ref_t line);

/**
 * Get the CFWDAll status for the line
 * @param [in] line - line reference handle
 * @return cc_boolean - isForwarded
 */
cc_boolean CCAPI_lineInfo_isCFWDActive(cc_lineinfo_ref_t line);

/**
 * Get the CFWDAll destination
 * @param [in] line - line reference handle
 * @return cc_string_t - cfwd target
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_lineInfo_getCFWDName(cc_lineinfo_ref_t line);

/**
 * Get calls on line
 * @param [in] line - lineID
 * @param [in, out] handles[] - Array of callinfo references
 * @param [in,out] count - count of call references populated
 * @return void
 */
void CCAPI_LineInfo_getCalls(cc_lineid_t line, cc_call_handle_t handles[], int *count);

/**
 * Get calls on line by state
 * @param [in] line - lineID
 * @param [in] state - state
 * @param [in, out] handles[] - Array of callinfo references
 * @param [in,out] count - count of call references populated
 * @return void
 */
void CCAPI_LineInfo_getCallsByState(cc_lineid_t line, cc_call_state_t state,
                cc_call_handle_t handles[], int *count);

/**
 * Get the MWI Status
 * @param line - line reference handle
 * @return cc_uint32_t - MWI status (boolean 0 => no MWI)
 */
cc_uint32_t CCAPI_lineInfo_getMWIStatus(cc_lineinfo_ref_t line);

/**
 * Get the MWI Type
 * @param line - line reference handle
 * @return cc_uint32_t - MWI Type
 */
cc_uint32_t CCAPI_lineInfo_getMWIType(cc_lineinfo_ref_t line);

/**
 * Get the MWI new msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI new msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWINewMsgCount(cc_lineinfo_ref_t line);

/**
 * Get the MWI old msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI old msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIOldMsgCount(cc_lineinfo_ref_t line);

/**
 * Get the MWI high priority new msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI new msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIPrioNewMsgCount(cc_lineinfo_ref_t line);

/**
 * Get the MWI high priority old msg count
 * @param line - line reference handle
 * @return cc_uint32_t - MWI old msg count
 */
cc_uint32_t CCAPI_lineInfo_getMWIPrioOldMsgCount(cc_lineinfo_ref_t line);

/**
 * has capability - is the feature allowed
 * @param [in] handle - call info handle
 * @param [in] feat_id - feature id
 * @return boolean - is Allowed
 */
cc_boolean  CCAPI_LineInfo_hasCapability(cc_lineinfo_ref_t line, cc_int32_t feat_id);

/**
 * get Allowed Feature set
 * @param [in] handle - call info handle
 * @param [in,out] feat_set - array of len CC_CALL_CAP_MAX
 * @return cc_return_t - CC_SUCCESS or CC_FAILURE
 */
cc_return_t  CCAPI_LineInfo_getCapabilitySet(cc_lineinfo_ref_t line, cc_int32_t feat_set[]);

#endif /* _CCAPIAPI_LINE_INFO_H_ */
