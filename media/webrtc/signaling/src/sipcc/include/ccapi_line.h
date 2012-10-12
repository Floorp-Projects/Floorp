/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_LINE_H_
#define _CCAPI_LINE_H_

#include "ccapi_types.h"

/**
 * Get reference handle for the line
 * @param [in] line - lineID on which to create call
 * @return cc_call_handle_t - handle of the call created
 * NOTE: The info returned by this method must be released using CCAPI_Line_releaseLineInfo()
 */
cc_lineinfo_ref_t CCAPI_Line_getLineInfo(cc_uint32_t line);

/**
 * Create a call on the line
 * @param [in] line - lineID on which to create call
 * @return cc_call_handle_t - handle of the call created
 */
cc_call_handle_t CCAPI_Line_CreateCall(cc_lineid_t line);

/**
 * Retain the lineInfo snapshot - this info shall not be freed until a
 * CCAPI_Line_releaseLineInfo() API releases this resource.
 * @param [in] ref - reference to the block to be retained
 * @return void
 * NOTE: Application may use this API to retain the device info using this API inside
 * CCAPI_LineListener_onLineEvent() App must release the Info using
 * CCAPI_Line_releaseLineInfo() once it is done with it.
 */
void CCAPI_Line_retainLineInfo(cc_lineinfo_ref_t ref);

/**
 * Release the lineInfo snapshot that was retained using the CCAPI_Line_retainLineInfo API
 * @param [in] ref - line info reference to be freed
 * @return void
 */
void CCAPI_Line_releaseLineInfo(cc_lineinfo_ref_t ref);

#endif /* _CCAPIAPI_LINE_H_ */
